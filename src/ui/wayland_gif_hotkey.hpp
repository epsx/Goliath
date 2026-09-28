#pragma once

#if defined(GOLIATH_WAYLAND_CAPTURE)

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QKeySequence>
#include <QList>
#include <QObject>
#include <QString>
#include <QVariantMap>

#include <functional>

namespace goliath {

struct PortalShortcut {
    QString id;
    QVariantMap properties;
};
using PortalShortcuts = QList<PortalShortcut>;

QDBusArgument& operator<<(QDBusArgument& arg, const PortalShortcut& shortcut);
const QDBusArgument& operator>>(const QDBusArgument& arg, PortalShortcut& shortcut);

// GNOME grants shortcuts through the desktop portal, not a keyboard grab.
// The portal can assign a different trigger than the application's preference.
class WaylandGifHotkey final : public QObject {
    Q_OBJECT
public:
    explicit WaylandGifHotkey(std::function<void()> trigger, QObject* parent = nullptr);
    ~WaylandGifHotkey() override;
    bool setShortcut(const QKeySequence& key, QString* error,
                     bool configureExisting = false);
    QString assignedTrigger() const { return m_assignedTrigger; }

signals:
    void assignedTriggerChanged(const QString& trigger);

private slots:
    void onActivated(const QDBusObjectPath& session, const QString& id,
                     qulonglong timestamp, const QVariantMap& options);
    void onShortcutsChanged(const QDBusObjectPath& session,
                            const PortalShortcuts& shortcuts);

private:
    bool configureShortcut(QString* error);
    void closeSession();
    void setAssignedTrigger(const QString& trigger);
    QString m_busName;
    QDBusConnection m_bus;
    QString m_registrationError;
    QString m_sessionPath;
    QString m_assignedTrigger;
    bool m_signalConnected = false;
    bool m_shortcutsSignalConnected = false;
    std::function<void()> m_trigger;
};

} // namespace goliath

Q_DECLARE_METATYPE(goliath::PortalShortcut)
Q_DECLARE_METATYPE(goliath::PortalShortcuts)
#endif
