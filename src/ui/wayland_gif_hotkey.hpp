#pragma once

#if defined(GOLIATH_WAYLAND_CAPTURE)

#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QKeySequence>
#include <QObject>
#include <QString>
#include <QVariantMap>

#include <functional>

namespace goliath {

// GNOME grants shortcuts through the desktop portal, not a keyboard grab.
// The portal can assign a different trigger than the application's preference.
class WaylandGifHotkey final : public QObject {
    Q_OBJECT
public:
    explicit WaylandGifHotkey(std::function<void()> trigger, QObject* parent = nullptr);
    ~WaylandGifHotkey() override;
    bool setShortcut(const QKeySequence& key, QString* error);
    QString assignedTrigger() const { return m_assignedTrigger; }

private slots:
    void onActivated(const QDBusObjectPath& session, const QString& id,
                     qulonglong timestamp, const QVariantMap& options);

private:
    void closeSession();
    QString m_busName;
    QDBusConnection m_bus;
    QString m_registrationError;
    QString m_sessionPath;
    QString m_assignedTrigger;
    bool m_signalConnected = false;
    std::function<void()> m_trigger;
};

} // namespace goliath
#endif
