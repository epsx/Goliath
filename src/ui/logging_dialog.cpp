#include "ui/logging_dialog.hpp"

#include "common/debug_logger.hpp"
#include "common/filesystem_safety.hpp"
#include "common/log_file.hpp"
#include "ui/widgets/title_bar.hpp"

#include <QCheckBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QFileInfo>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

#include <utility>

namespace fs = std::filesystem;

namespace goliath {

namespace {

QString path_to_qstring(const fs::path& path) {
#if defined(_WIN32)
    return QString::fromStdWString(path.wstring());
#else
    return QString::fromStdString(path.string());
#endif
}

QString file_summary(const fs::path& path) {
    const DirectPathInspection inspection = inspect_direct_path(path);
    if (inspection.kind == DirectPathKind::Missing)
        return "Not created yet.";
    if (inspection.kind != DirectPathKind::RegularFile)
        return "Not a direct regular file.";

    const QFileInfo info(path_to_qstring(path));
    const QString size = QLocale().formattedDataSize(info.size());
    const QString modified = info.lastModified().toString("yyyy-MM-dd HH:mm:ss");
    return QString("%1 | Modified %2").arg(size, modified);
}

bool direct_regular_file(const fs::path& path) {
    return inspect_direct_path(path).kind == DirectPathKind::RegularFile;
}

QLabel* path_label(const fs::path& path) {
    auto* label = new QLabel(path_to_qstring(path));
    label->setTextFormat(Qt::PlainText);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    label->setWordWrap(true);
    return label;
}

} // namespace

LoggingDialog::LoggingDialog(
        bool sessionVerbose,
        bool configuredVerbose,
        AppPaths paths,
        std::function<bool()> jgrfProcessActive,
        QWidget* parent)
    : QDialog(parent),
      m_paths(std::move(paths)),
      m_frontendLog(m_paths.base_dir / "goliath-qt-debug.log"),
      m_jgrfLog(m_paths.base_dir / "jollygood.log"),
      m_jgrfProcessActive(std::move(jgrfProcessActive)),
      m_configuredVerbose(configuredVerbose) {
    resize(760, 520);

    QVBoxLayout* layout = nullptr;
    setupFramelessDialog(this, "Diagnostics & Logs", &layout, true, false);

    auto* verboseBox = new QGroupBox("Per-launch diagnostics");
    auto* verboseLayout = new QVBoxLayout(verboseBox);
    m_verboseCheck = new QCheckBox(
        "Enable verbose JGRF/core logging for normal launches (-v)");
    m_verboseCheck->setChecked(sessionVerbose || configuredVerbose);
    m_verboseCheck->setEnabled(!configuredVerbose);
    verboseLayout->addWidget(m_verboseCheck);

    auto* verboseNote = new QLabel();
    verboseNote->setWordWrap(true);
    verboseNote->setTextFormat(Qt::PlainText);
    if (configuredVerbose) {
        verboseNote->setText(
            "Verbose logging is already enabled by the configured JGRF "
            "arguments. Close Goliath and remove -v/--verbose from "
            "[Paths] jollygood_args in goliath.ini to disable it. Goliath "
            "will not add a duplicate option.");
    } else {
        verboseNote->setText(
            "This toggle lasts only for the current Goliath session. It adds "
            "--verbose to normal and Random launches without modifying global "
            "INI files or exact-media profiles. The session toggle is not "
            "applied to benchmarks, so their results stay comparable.");
    }
    verboseLayout->addWidget(verboseNote);
    layout->addWidget(verboseBox);

    auto* frontendBox = new QGroupBox("Goliath frontend log");
    auto* frontendLayout = new QVBoxLayout(frontendBox);
    frontendLayout->addWidget(path_label(m_frontendLog));
    m_frontendStatus = new QLabel();
    frontendLayout->addWidget(m_frontendStatus);
    auto* frontendButtons = new QHBoxLayout();
    m_openFrontendButton = new QPushButton("Open Frontend Log");
    m_clearFrontendButton = new QPushButton("Clear Frontend Log");
    frontendButtons->addWidget(m_openFrontendButton);
    frontendButtons->addWidget(m_clearFrontendButton);
    frontendButtons->addStretch();
    frontendLayout->addLayout(frontendButtons);
    layout->addWidget(frontendBox);

    auto* jgrfBox = new QGroupBox("JGRF / Geolith output log");
    auto* jgrfLayout = new QVBoxLayout(jgrfBox);
    jgrfLayout->addWidget(path_label(m_jgrfLog));
    m_jgrfStatus = new QLabel();
    jgrfLayout->addWidget(m_jgrfStatus);
    m_processNote = new QLabel();
    m_processNote->setWordWrap(true);
    m_processNote->setTextFormat(Qt::PlainText);
    jgrfLayout->addWidget(m_processNote);
    auto* jgrfButtons = new QHBoxLayout();
    m_openJgrfButton = new QPushButton("Open JGRF Log");
    m_clearJgrfButton = new QPushButton("Clear JGRF Log");
    jgrfButtons->addWidget(m_openJgrfButton);
    jgrfButtons->addWidget(m_clearJgrfButton);
    jgrfButtons->addStretch();
    jgrfLayout->addLayout(jgrfButtons);
    layout->addWidget(jgrfBox);

    auto* footer = new QHBoxLayout();
    auto* openFolderButton = new QPushButton("Open Log Folder");
    auto* refreshButton = new QPushButton("Refresh");
    auto* closeButton = new QPushButton("Close");
    footer->addWidget(openFolderButton);
    footer->addStretch();
    footer->addWidget(refreshButton);
    footer->addWidget(closeButton);
    layout->addLayout(footer);

    connect(m_openFrontendButton, &QPushButton::clicked, this, [this]() {
        openLog(m_frontendLog, "Goliath frontend log");
    });
    connect(m_clearFrontendButton, &QPushButton::clicked,
            this, [this]() { clearFrontendLog(); });
    connect(m_openJgrfButton, &QPushButton::clicked, this, [this]() {
        openLog(m_jgrfLog, "JGRF log");
    });
    connect(m_clearJgrfButton, &QPushButton::clicked,
            this, [this]() { clearJgrfLog(); });
    connect(openFolderButton, &QPushButton::clicked,
            this, [this]() { openLogFolder(); });
    connect(refreshButton, &QPushButton::clicked,
            this, [this]() { refresh(); });
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);

    refresh();
}

bool LoggingDialog::sessionVerboseLoggingEnabled() const {
    // A configured -v remains owned by JGRF Arguments rather than becoming a
    // hidden session override.
    return !m_configuredVerbose && m_verboseCheck->isChecked();
}

void LoggingDialog::refresh() {
    m_frontendStatus->setText(file_summary(m_frontendLog));
    m_jgrfStatus->setText(file_summary(m_jgrfLog));

    const bool active = m_jgrfProcessActive && m_jgrfProcessActive();
    m_processNote->setText(
        active
            ? "Clear is disabled because a JGRF process launched by this "
              "Goliath session is still active."
            : "Close any JGRF process started externally or left running "
              "after a Goliath restart before clearing this log.");

    m_openFrontendButton->setEnabled(direct_regular_file(m_frontendLog));
    m_clearFrontendButton->setEnabled(direct_regular_file(m_frontendLog));
    m_openJgrfButton->setEnabled(direct_regular_file(m_jgrfLog));
    m_clearJgrfButton->setEnabled(
        !active && direct_regular_file(m_jgrfLog));
}

void LoggingDialog::openLog(const fs::path& path, const QString& name) {
    if (!direct_regular_file(path)) {
        QMessageBox::information(
            this, name, "The selected log does not exist as a regular file.");
        refresh();
        return;
    }
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(path_to_qstring(path)))) {
        QMessageBox::warning(
            this, name, "Could not open the selected log file.");
    }
}

void LoggingDialog::openLogFolder() {
    if (!QDesktopServices::openUrl(
            QUrl::fromLocalFile(path_to_qstring(m_paths.base_dir)))) {
        QMessageBox::warning(
            this, "Open Log Folder", "Could not open the log folder.");
    }
}

void LoggingDialog::clearFrontendLog() {
    if (QMessageBox::question(
            this, "Clear Frontend Log",
            "Permanently clear goliath-qt-debug.log?\n\n"
            "Future frontend events will continue to be written to it.") !=
        QMessageBox::Yes) {
        return;
    }

    QString error;
    if (!DebugLogger::clear(&error)) {
        QMessageBox::critical(
            this, "Clear Frontend Log",
            QString("Could not clear the frontend log.\n\n%1").arg(error));
    }
    refresh();
}

void LoggingDialog::clearJgrfLog() {
    if (m_jgrfProcessActive && m_jgrfProcessActive()) {
        QMessageBox::warning(
            this, "Clear JGRF Log",
            "A JGRF process launched by Goliath is still active. Close every "
            "running game, then press Refresh.");
        refresh();
        return;
    }
    if (QMessageBox::question(
            this, "Clear JGRF Log",
            "Permanently clear jollygood.log?\n\n"
            "Close any externally launched JGRF process first.") !=
        QMessageBox::Yes) {
        return;
    }

    const LogFileClearResult result = clear_regular_log_file(m_jgrfLog);
    if (!result.success) {
        QMessageBox::critical(
            this, "Clear JGRF Log",
            QString("Could not clear the JGRF log.\n\n%1")
                .arg(QString::fromStdString(result.error)));
    } else {
        DebugLogger::logInfo("jollygood.log cleared by user");
    }
    refresh();
}

} // namespace goliath
