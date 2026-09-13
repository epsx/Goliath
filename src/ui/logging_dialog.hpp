// logging_dialog.hpp — session-scoped JGRF verbose launch control and safe
// access to the two runtime logs.
#pragma once

#include <QDialog>
#include <QString>

#include <filesystem>
#include <functional>

#include "common/paths.hpp"

class QCheckBox;
class QLabel;
class QPushButton;

namespace goliath {

class LoggingDialog final : public QDialog {
public:
    LoggingDialog(
        bool session_verbose,
        bool configured_verbose,
        AppPaths paths,
        std::function<bool()> jgrf_process_active,
        QWidget* parent = nullptr);

    bool sessionVerboseLoggingEnabled() const;

private:
    void refresh();
    void openLog(const std::filesystem::path& path, const QString& name);
    void openLogFolder();
    void clearFrontendLog();
    void clearJgrfLog();

    AppPaths m_paths;
    std::filesystem::path m_frontendLog;
    std::filesystem::path m_jgrfLog;
    std::function<bool()> m_jgrfProcessActive;
    bool m_configuredVerbose = false;

    QCheckBox* m_verboseCheck = nullptr;
    QLabel* m_frontendStatus = nullptr;
    QLabel* m_jgrfStatus = nullptr;
    QLabel* m_processNote = nullptr;
    QPushButton* m_openFrontendButton = nullptr;
    QPushButton* m_clearFrontendButton = nullptr;
    QPushButton* m_openJgrfButton = nullptr;
    QPushButton* m_clearJgrfButton = nullptr;
};

} // namespace goliath
