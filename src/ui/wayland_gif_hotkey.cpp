#include "ui/wayland_gif_hotkey.hpp"

#if defined(GOLIATH_WAYLAND_CAPTURE)

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusPendingCall>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QChar>
#include <QEventLoop>
#include <QList>
#include <QProcess>
#include <QStandardPaths>
#include <QStringList>
#include <QTimer>
#include <QUuid>

#include <utility>

namespace goliath {

QDBusArgument& operator<<(QDBusArgument& arg, const PortalShortcut& shortcut) {
    arg.beginStructure();
    arg << shortcut.id << shortcut.properties;
    arg.endStructure();
    return arg;
}

const QDBusArgument& operator>>(const QDBusArgument& arg, PortalShortcut& shortcut) {
    arg.beginStructure();
    arg >> shortcut.id >> shortcut.properties;
    arg.endStructure();
    return arg;
}

} // namespace goliath

namespace goliath {

class ShortcutReply final : public QObject {
    Q_OBJECT
public:
    bool completed = false;
    uint code = 2;
    QVariantMap values;
signals:
    void arrived();
public slots:
    void response(uint result, const QVariantMap& data) {
        completed = true;
        code = result;
        values = data;
        emit arrived();
    }
};

namespace {
constexpr auto kPortal = "org.freedesktop.portal.Desktop";
constexpr auto kDesktop = "/org/freedesktop/portal/desktop";
constexpr auto kInterface = "org.freedesktop.portal.GlobalShortcuts";
constexpr auto kShortcutId = "goliath_record_gif";

QString newToken() {
    return "goliath_" + QUuid::createUuid().toString(QUuid::Id128);
}

QString portalTrigger(const QKeySequence& sequence) {
    if (sequence.isEmpty() || sequence.count() != 1) return {};
    const auto combination = sequence[0];
    const auto modifiers = combination.keyboardModifiers();
    if (modifiers & ~(Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier)) return {};
    const int key = combination.key();
    QString name;
    if (key >= Qt::Key_A && key <= Qt::Key_Z)
        name = QChar(static_cast<char16_t>('a' + key - Qt::Key_A));
    else if (key >= Qt::Key_0 && key <= Qt::Key_9)
        name = QChar(static_cast<char16_t>('0' + key - Qt::Key_0));
    else if (key >= Qt::Key_F1 && key <= Qt::Key_F24)
        name = QString("F%1").arg(key - Qt::Key_F1 + 1);
    else {
        switch (key) {
        case Qt::Key_Semicolon: name = "semicolon"; break;
        case Qt::Key_Comma: name = "comma"; break;
        case Qt::Key_Period: name = "period"; break;
        case Qt::Key_Equal: name = "equal"; break;
        case Qt::Key_Minus: name = "minus"; break;
        case Qt::Key_Slash: name = "slash"; break;
        case Qt::Key_QuoteLeft: name = "grave"; break;
        case Qt::Key_BracketLeft: name = "bracketleft"; break;
        case Qt::Key_Backslash: name = "backslash"; break;
        case Qt::Key_BracketRight: name = "bracketright"; break;
        case Qt::Key_Apostrophe: name = "apostrophe"; break;
        default: break;
        }
    }
    if (name.isEmpty()) return {};
    if (modifiers & Qt::ControlModifier) name.prepend("CTRL+");
    if (modifiers & Qt::AltModifier) name.prepend("ALT+");
    if (modifiers & Qt::ShiftModifier) name.prepend("SHIFT+");
    return name;
}

QString portalAssignedTrigger(const PortalShortcuts& shortcuts,
                              bool* found = nullptr) {
    if (found) *found = false;
    for (const auto& shortcut : shortcuts) {
        if (shortcut.id != QString::fromLatin1(kShortcutId)) continue;
        if (found) *found = true;
        return shortcut.properties.value("trigger_description").toString();
    }
    return {};
}

bool portalRequest(QDBusConnection bus, const QString& method, QVariantList args,
                   QVariantMap options, QVariantMap* result, QString* error) {
    if (!bus.isConnected()) {
        *error = "Cannot connect to the Wayland session bus.";
        return false;
    }
    QString sender = bus.baseService().mid(1);
    sender.replace('.', '_');
    const QString token = newToken();
    const QString path = QString("/org/freedesktop/portal/desktop/request/%1/%2")
                             .arg(sender, token);
    ShortcutReply reply;
    if (!bus.connect(kPortal, path, "org.freedesktop.portal.Request",
                     "Response", &reply, SLOT(response(uint,QVariantMap)))) {
        *error = "Cannot receive the shortcut portal's response.";
        return false;
    }
    options.insert("handle_token", token);
    args.append(options);
    QDBusMessage call = QDBusMessage::createMethodCall(kPortal, kDesktop,
                                                        kInterface, method);
    call.setArguments(args);
    const QDBusMessage answer = bus.call(call, QDBus::Block, 5000);
    if (answer.type() == QDBusMessage::ErrorMessage || answer.arguments().isEmpty()) {
        *error = QString("Global shortcut portal unavailable: %1").arg(answer.errorMessage());
    } else if (answer.arguments().first().value<QDBusObjectPath>().path() != path) {
        *error = "Shortcut portal returned an unexpected request path.";
    } else {
        if (!reply.completed) {
            QEventLoop loop;
            QTimer timer;
            timer.setSingleShot(true);
            QObject::connect(&reply, &ShortcutReply::arrived, &loop, &QEventLoop::quit);
            QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
            timer.start(90000);
            loop.exec();
        }
        if (!reply.completed)
            *error = "Timed out waiting for the global shortcut portal.";
        else if (reply.code != 0)
            *error = reply.code == 1 ? "Shortcut assignment was cancelled."
                                      : "The desktop denied this shortcut.";
        else
            *result = reply.values;
    }
    bus.disconnect(kPortal, path, "org.freedesktop.portal.Request",
                   "Response", &reply, SLOT(response(uint,QVariantMap)));
    return error->isEmpty();
}
} // namespace

WaylandGifHotkey::WaylandGifHotkey(std::function<void()> trigger, QObject* parent)
    : QObject(parent),
      m_busName("goliath_gif_" + QUuid::createUuid().toString(QUuid::Id128)),
      m_bus(QDBusConnection::connectToBus(QDBusConnection::SessionBus, m_busName)),
      m_trigger(std::move(trigger)) {
    qDBusRegisterMetaType<PortalShortcut>();
    qDBusRegisterMetaType<PortalShortcuts>();
    if (!m_bus.isConnected()) {
        m_registrationError = "Cannot open the shortcut portal bus connection.";
        return;
    }
    QDBusMessage registration = QDBusMessage::createMethodCall(
        kPortal, kDesktop, "org.freedesktop.host.portal.Registry", "Register");
    registration.setArguments({QStringLiteral("io.github.epsx.Goliath"),
                               QVariant::fromValue(QVariantMap{})});
    const QDBusMessage response = m_bus.call(registration, QDBus::Block, 5000);
    if (response.type() == QDBusMessage::ErrorMessage &&
        response.errorName() != "org.freedesktop.DBus.Error.UnknownMethod" &&
        response.errorName() != "org.freedesktop.DBus.Error.UnknownInterface" &&
        !response.errorMessage().contains("already associated", Qt::CaseInsensitive))
        m_registrationError = response.errorMessage();
    m_signalConnected = m_bus.connect(kPortal, kDesktop, kInterface,
        "Activated", this,
        SLOT(onActivated(QDBusObjectPath,QString,qulonglong,QVariantMap)));
    m_shortcutsSignalConnected = m_bus.connect(kPortal, kDesktop, kInterface,
        "ShortcutsChanged", this,
        SLOT(onShortcutsChanged(QDBusObjectPath,PortalShortcuts)));
}

WaylandGifHotkey::~WaylandGifHotkey() {
    closeSession();
    m_bus.disconnect(kPortal, kDesktop, kInterface,
        "Activated", this,
        SLOT(onActivated(QDBusObjectPath,QString,qulonglong,QVariantMap)));
    m_bus.disconnect(kPortal, kDesktop, kInterface,
        "ShortcutsChanged", this,
        SLOT(onShortcutsChanged(QDBusObjectPath,PortalShortcuts)));
    QDBusConnection::disconnectFromBus(m_busName);
}

void WaylandGifHotkey::closeSession() {
    if (m_sessionPath.isEmpty()) return;
    QDBusMessage call = QDBusMessage::createMethodCall(kPortal, m_sessionPath,
        "org.freedesktop.portal.Session", "Close");
    m_bus.asyncCall(call);
    m_sessionPath.clear();
}

void WaylandGifHotkey::setAssignedTrigger(const QString& trigger) {
    if (m_assignedTrigger == trigger) return;
    m_assignedTrigger = trigger;
    emit assignedTriggerChanged(m_assignedTrigger);
}

bool WaylandGifHotkey::configureShortcut(QString* error) {
    if (error) error->clear();
    if (m_sessionPath.isEmpty()) {
        if (error) *error = "No Wayland shortcut session is active.";
        return false;
    }
    if (!m_shortcutsSignalConnected) {
        if (error) *error = "Cannot monitor shortcut changes from the desktop portal.";
        return false;
    }

    QDBusMessage call = QDBusMessage::createMethodCall(
        kPortal, kDesktop, kInterface, "ConfigureShortcuts");
    call.setArguments({QVariant::fromValue(QDBusObjectPath(m_sessionPath)),
                       QString(), QVariant::fromValue(QVariantMap{})});
    const QDBusMessage reply = m_bus.call(call, QDBus::Block, 5000);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        if (reply.errorName() == "org.freedesktop.DBus.Error.UnknownMethod" &&
            qEnvironmentVariable("XDG_CURRENT_DESKTOP")
                .contains("GNOME", Qt::CaseInsensitive)) {
            const QString settings =
                QStandardPaths::findExecutable("gnome-control-center");
            if (!settings.isEmpty() &&
                QProcess::startDetached(
                    settings, QStringList{QStringLiteral("applications")}))
                return true;
        }
        if (error) {
            if (reply.errorName() == "org.freedesktop.DBus.Error.UnknownMethod") {
                *error = "This desktop portal cannot reconfigure an existing "
                         "shortcut. Use the desktop's application permissions "
                         "to change or remove the Goliath global shortcut.";
            } else {
                *error = QString("The desktop could not open shortcut settings: %1")
                             .arg(reply.errorMessage());
            }
        }
        return false;
    }
    return true;
}

bool WaylandGifHotkey::setShortcut(const QKeySequence& key, QString* error,
                                   bool configureExisting) {
    if (error) error->clear();
    if (key.isEmpty()) {
        closeSession();
        setAssignedTrigger({});
        return true;
    }
    if (!m_signalConnected) {
        if (error) *error = m_registrationError.isEmpty()
            ? "Cannot receive global shortcut activations from the desktop."
            : m_registrationError;
        return false;
    }
    const QString trigger = portalTrigger(key);
    if (trigger.isEmpty()) {
        if (error) *error = "Choose one letter, number, F1-F24, or punctuation key; Ctrl/Alt/Shift are optional.";
        return false;
    }

    // The portal owns the actual binding after BindShortcuts. Recreating the
    // session only restores that persisted binding and does not apply a new
    // preferred_trigger. Portal v2 provides ConfigureShortcuts for changes.
    if (!m_sessionPath.isEmpty())
        return configureExisting ? configureShortcut(error) : true;

    QString failure;
    QVariantMap result;
    if (!portalRequest(m_bus, "CreateSession", {}, {{"session_handle_token", newToken()}},
                       &result, &failure)) {
        if (failure.contains("app id is required", Qt::CaseInsensitive)) {
            failure += " Install io.github.epsx.Goliath.desktop using the "
                       "Wayland setup script and restart Goliath.";
            if (!m_registrationError.isEmpty())
                failure += QString(" Registration: %1").arg(m_registrationError);
        }
        if (error) *error = failure;
        return false;
    }
    const QString candidate = result.value("session_handle").toString();
    if (candidate.isEmpty()) {
        if (error) *error = "The shortcut portal did not create a session.";
        return false;
    }
    const auto closeCandidate = [this, &candidate]() {
        QDBusMessage close = QDBusMessage::createMethodCall(kPortal, candidate,
            "org.freedesktop.portal.Session", "Close");
        m_bus.asyncCall(close);
    };
    PortalShortcut shortcut;
    shortcut.id = QString::fromLatin1(kShortcutId);
    shortcut.properties.insert("description", "Record a Goliath game GIF");
    shortcut.properties.insert("preferred_trigger", trigger);
    const PortalShortcuts shortcuts{shortcut};
    if (!portalRequest(m_bus, "BindShortcuts",
                       {QVariant::fromValue(QDBusObjectPath(candidate)),
                        QVariant::fromValue(shortcuts), QString()},
                       {}, &result, &failure)) {
        closeCandidate();
        if (error) *error = failure;
        return false;
    }
    const QVariant assigned = result.value("shortcuts");
    const auto bound = assigned.canConvert<QDBusArgument>()
        ? qdbus_cast<PortalShortcuts>(assigned.value<QDBusArgument>())
        : PortalShortcuts{};
    bool accepted = false;
    const QString triggerDescription = portalAssignedTrigger(bound, &accepted);
    if (!accepted) {
        closeCandidate();
        if (error) *error = "The desktop did not assign a GIF shortcut.";
        return false;
    }
    closeSession();
    m_sessionPath = candidate;
    setAssignedTrigger(triggerDescription);
    return true;
}

void WaylandGifHotkey::onActivated(const QDBusObjectPath& session,
                                   const QString& id, qulonglong timestamp,
                                   const QVariantMap& options) {
    Q_UNUSED(timestamp);
    Q_UNUSED(options);
    if (session.path() == m_sessionPath && id == QString::fromLatin1(kShortcutId))
        m_trigger();
}

void WaylandGifHotkey::onShortcutsChanged(const QDBusObjectPath& session,
                                          const PortalShortcuts& shortcuts) {
    if (session.path() != m_sessionPath) return;
    bool found = false;
    const QString trigger = portalAssignedTrigger(shortcuts, &found);
    setAssignedTrigger(found ? trigger : QString());
}

} // namespace goliath
#include "wayland_gif_hotkey.moc"
#endif
