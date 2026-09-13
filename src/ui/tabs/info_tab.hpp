// info_tab.hpp — read-only information about the exact JGRF/Geolith/JG
// components installed next to the frontend. Probes are lazy/asynchronous so
// opening Settings is never blocked by executable or shared-library I/O.
#pragma once

#include <QString>
#include <QWidget>
#include <filesystem>

class QLabel;
class QProcess;
class QPushButton;

namespace goliath {

class InfoTab : public QWidget {
    Q_OBJECT
public:
    explicit InfoTab(std::filesystem::path jollygoodExe, QWidget* parent = nullptr);

    // Called when the tab becomes visible. The first activation probes the
    // installed components; later activations reuse the result until Refresh.
    void activate();

private slots:
    void refresh();

private:
    void setupUi();
    void startJgrfProbe();
    void startCoreProbe();
    void finishJgrfProbe();
    void finishCoreProbe();
    void updateRefreshState();

    std::filesystem::path m_jollygoodExe;

    QLabel* m_jgrfVersion = nullptr;
    QLabel* m_jgrfExecutable = nullptr;
    QLabel* m_vulkanRenderer = nullptr;
    QLabel* m_geolithVersion = nullptr;
    QLabel* m_coreLibrary = nullptr;
    QLabel* m_neocdFormats = nullptr;
    QLabel* m_chdSupport = nullptr;
    QLabel* m_jgApiVersion = nullptr;
    QLabel* m_status = nullptr;
    QPushButton* m_refreshButton = nullptr;

    QProcess* m_jgrfProcess = nullptr;
    bool m_activated = false;
    bool m_jgrfPending = false;
    bool m_corePending = false;
    bool m_jgrfProbeOk = false;
    bool m_coreProbeOk = false;
    QString m_jgrfError;
    QString m_coreError;
};

} // namespace goliath
