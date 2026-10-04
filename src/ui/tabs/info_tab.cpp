#include "info_tab.hpp"

#include "game/jollygood_capabilities.hpp"
#include "game/jollygood_executable.hpp"
#include "game/geolith_capabilities.hpp"
#include "common/goliath_common.hpp"
#include "ui/update_check.hpp"
#include "update/update_schedule.hpp"

#include <QCryptographicHash>
#include <QComboBox>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLibrary>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QThread>
#include <QStringList>
#include <QTimer>
#include <QVBoxLayout>

#include <cstdint>
#include <memory>
#include <utility>

namespace fs = std::filesystem;

namespace goliath {
namespace {

QLabel* valueLabel(const QString& initial = "Not checked") {
    auto* label = new QLabel(initial);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    label->setWordWrap(true);
    return label;
}

#if defined(_WIN32)
// These hashes identify the Windows JGRF/libepoxy pair validated with
// OpenGL ES on MVS/AES and Neo Geo CD CHD. --help alone cannot report the
// runtime BGRA fix; a different build remains unverified.
bool matchesSha256(const fs::path& path, const char* expected) {
    QFile file(QString::fromStdString(path.string()));
    if (!file.open(QIODevice::ReadOnly)) return false;
    QCryptographicHash hash(QCryptographicHash::Sha256);
    return hash.addData(&file) && hash.result().toHex() == expected;
}

bool hasVerifiedEsBgraPair(const fs::path& jgrfExe) {
    return !jgrfExe.empty() &&
           matchesSha256(jgrfExe,
                         "af010059dac1696873e9f9c5dd46554c924ffe8b80f9ee43218c689640732011") &&
           matchesSha256(jgrfExe.parent_path() / "libepoxy-0.dll",
                         "50ff6e67ccc5d76bd3bc157eb26a7fbe1a15743574b3f0fa1de50eb997c79da3");
}
#endif

} // namespace

InfoTab::InfoTab(fs::path jollygoodExe, Config& config, QWidget* parent)
    : QWidget(parent), m_jollygoodExe(std::move(jollygoodExe)),
      m_config(config) {
    setupUi();
}

void InfoTab::setupUi() {
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* content = new QWidget(scroll);
    auto* layout = new QVBoxLayout(content);
    scroll->setWidget(content);
    rootLayout->addWidget(scroll);

    auto* updatesGroup = new QGroupBox("Goliath Updates");
    auto* updatesLayout = new QVBoxLayout(updatesGroup);
    auto* updatesForm = new QFormLayout();
    m_updateInterval = new QComboBox(updatesGroup);
    m_updateInterval->addItem(
        "Disabled (manual checks only)", QStringLiteral("disabled"));
    m_updateInterval->addItem("Daily", QStringLiteral("daily"));
    m_updateInterval->addItem("Weekly", QStringLiteral("weekly"));
    const QString savedInterval = QString::fromStdString(
        std::string(update_check_interval_key(parse_update_check_interval(
            m_config.get("Updates", "check_interval", "disabled")))));
    const int intervalIndex = m_updateInterval->findData(savedInterval);
    m_updateInterval->setCurrentIndex(intervalIndex < 0 ? 0 : intervalIndex);
    updatesForm->addRow("Automatic check:", m_updateInterval);

    m_lastUpdateCheck = valueLabel("Never");
    updatesForm->addRow("Last successful check:", m_lastUpdateCheck);
    updatesLayout->addLayout(updatesForm);
    refreshLastUpdateCheck();

    auto* updateNote = new QLabel(
        "Automatic checks are opt-in. They run silently when Goliath is up "
        "to date or the network is unavailable, and show a dialog only when "
        "a newer release exists. Manual checks remain available at any time.",
        updatesGroup);
    updateNote->setWordWrap(true);
    updateNote->setObjectName("themed_note");
    updatesLayout->addWidget(updateNote);

    auto* updateButtons = new QHBoxLayout();
    m_checkUpdatesButton = new QPushButton("Check Now...", updatesGroup);
    auto* saveUpdatePreference = new QPushButton(
        "Save Update Preference", updatesGroup);
    updateButtons->addWidget(m_checkUpdatesButton);
    updateButtons->addStretch();
    updateButtons->addWidget(saveUpdatePreference);
    updatesLayout->addLayout(updateButtons);

    m_updatePreferenceStatus = new QLabel(updatesGroup);
    m_updatePreferenceStatus->setWordWrap(true);
    m_updatePreferenceStatus->setObjectName("secondary_text");
    updatesLayout->addWidget(m_updatePreferenceStatus);

    connect(m_checkUpdatesButton, &QPushButton::clicked,
            this, &InfoTab::checkUpdatesNow);
    connect(saveUpdatePreference, &QPushButton::clicked,
            this, &InfoTab::saveUpdatePreference);
    layout->addWidget(updatesGroup);

    auto* jgrfGroup = new QGroupBox("JollyGood Reference Frontend (JGRF)");
    auto* jgrfForm = new QFormLayout(jgrfGroup);
    m_jgrfVersion = valueLabel();
    m_jgrfExecutable = valueLabel();
    m_vulkanRenderer = valueLabel();
    m_esBgraRenderer = valueLabel();
    m_esBgraRenderer->setToolTip(
        "Checks the exact Windows JGRF and libepoxy builds tested with OpenGL ES. "
        "Other builds require a game test; file identity alone cannot verify a PC's graphics driver.");
    jgrfForm->addRow("Version:", m_jgrfVersion);
    jgrfForm->addRow("Executable:", m_jgrfExecutable);
    jgrfForm->addRow("Vulkan Renderer:", m_vulkanRenderer);
    jgrfForm->addRow("OpenGL ES/BGRA:", m_esBgraRenderer);
    layout->addWidget(jgrfGroup);

    auto* coreGroup = new QGroupBox("Geolith Core");
    auto* coreForm = new QFormLayout(coreGroup);
    m_geolithVersion = valueLabel();
    m_coreLibrary = valueLabel();
    m_neocdFormats = valueLabel();
    m_chdSupport = valueLabel();
    coreForm->addRow("Version:", m_geolithVersion);
    coreForm->addRow("Core Library:", m_coreLibrary);
    coreForm->addRow("Neo Geo CD Formats:", m_neocdFormats);
    coreForm->addRow("CHD Support:", m_chdSupport);
    layout->addWidget(coreGroup);

    auto* apiGroup = new QGroupBox("Jolly Good API (JG)");
    auto* apiForm = new QFormLayout(apiGroup);
    m_jgApiVersion = valueLabel();
    apiForm->addRow("API Version:", m_jgApiVersion);
    layout->addWidget(apiGroup);

    auto* lithogenGroup = new QGroupBox("Lithogen (external ZIP to .neo converter)");
    auto* lithogenForm = new QFormLayout(lithogenGroup);
    m_lithogenExecutable = valueLabel();
    m_lithogenAvailability = valueLabel();
    m_lithogenVersion = valueLabel();
    lithogenForm->addRow("Executable:", m_lithogenExecutable);
    lithogenForm->addRow("Availability:", m_lithogenAvailability);
    lithogenForm->addRow("Version:", m_lithogenVersion);
    layout->addWidget(lithogenGroup);

    m_status = new QLabel(
        "Information is read from the JGRF executable and Geolith core installed for this Goliath setup.");
    m_status->setWordWrap(true);
    m_status->setObjectName("secondary_text");
    layout->addWidget(m_status);

    layout->addStretch();

    m_refreshButton = new QPushButton(QString::fromUtf8("\xE2\x86\xBB Refresh Information"));
    connect(m_refreshButton, &QPushButton::clicked, this, &InfoTab::refresh);
    layout->addWidget(m_refreshButton);
}

void InfoTab::activate() {
    if (m_activated) return;
    m_activated = true;
    refresh();
}

void InfoTab::saveUpdatePreference() {
    const QString key = m_updateInterval
        ? m_updateInterval->currentData().toString()
        : QStringLiteral("disabled");
    const UpdateCheckInterval interval = parse_update_check_interval(
        key.toStdString());
    m_config.set("Updates", "check_interval",
                 std::string(update_check_interval_key(interval)));
    save_config(m_config);
    if (m_updatePreferenceStatus) {
        m_updatePreferenceStatus->setText(
            interval == UpdateCheckInterval::Disabled
                ? "Automatic update checks are disabled."
                : QString("Automatic update checks saved: %1.")
                      .arg(interval == UpdateCheckInterval::Daily
                               ? "daily"
                               : "weekly"));
    }
}

void InfoTab::checkUpdatesNow() {
    if (m_checkUpdatesButton) m_checkUpdatesButton->setEnabled(false);
    const bool successful = checkForUpdates(this);
    if (successful) rememberSuccessfulUpdateCheck();
    if (m_checkUpdatesButton) m_checkUpdatesButton->setEnabled(true);
}

void InfoTab::rememberSuccessfulUpdateCheck() {
    recordSuccessfulUpdateCheck(m_config);
    refreshLastUpdateCheck();
}

void InfoTab::refreshLastUpdateCheck() {
    if (!m_lastUpdateCheck) return;
    const auto epoch = parse_update_check_epoch(m_config.get(
        "Updates", "last_successful_check_epoch", "0"));
    if (!epoch) {
        m_lastUpdateCheck->setText("Never");
        return;
    }
    m_lastUpdateCheck->setText(
        QDateTime::fromSecsSinceEpoch(*epoch)
            .toLocalTime()
            .toString("yyyy-MM-dd HH:mm"));
}

void InfoTab::refresh() {
    if (m_jgrfPending || m_corePending || m_lithogenPending) return;

    m_jgrfVersion->setText("Detecting...");
    m_jgrfExecutable->setText("Detecting...");
    m_vulkanRenderer->setText("Detecting...");
    m_esBgraRenderer->setText("Detecting...");
    m_geolithVersion->setText("Detecting...");
    m_coreLibrary->setText("Detecting...");
    m_neocdFormats->setText("Detecting...");
    m_chdSupport->setText("Detecting...");
    m_jgApiVersion->setText("Detecting...");
    m_lithogenVersion->setText("Detecting...");
    m_lithogenAvailability->setText("Detecting...");
    m_status->setText("Reading installed JGRF / Geolith information...");
    m_jgrfProbeOk = false;
    m_coreProbeOk = false;
    m_esBgraPairVerified = false;
    m_jgrfError.clear();
    m_coreError.clear();

    startJgrfProbe();
    startCoreProbe();
    startLithogenProbe();
    updateRefreshState();
}

void InfoTab::startLithogenProbe() {
    // Read the live config on each Refresh: the converter may have saved a
    // different executable since this Settings dialog was opened.
    const QString configured = QString::fromStdString(
        m_config.get("Tools", "lithogen_executable", "")).trimmed();
    m_lithogenExecutable->setText(configured.isEmpty() ? "Not configured" : configured);
    if (configured.isEmpty()) {
        m_lithogenAvailability->setText("Not configured");
        m_lithogenVersion->setText("Unknown");
        return;
    }
    const QFileInfo exe(configured);
    if (!exe.isFile() || !exe.isExecutable()) {
        m_lithogenAvailability->setText("Unavailable");
        m_lithogenVersion->setText("Unknown");
        return;
    }
    m_lithogenAvailability->setText("Available");
    m_lithogenExecutable->setText(exe.absoluteFilePath());
    m_lithogenPending = true;
    probeLithogenVersion("--version");
}

void InfoTab::probeLithogenVersion(const QString& argument) {
    auto* process = new QProcess(this);
    auto timedOut = std::make_shared<bool>(false);
    m_lithogenProcess = process;
    process->setProcessChannelMode(QProcess::MergedChannels);
    connect(process, &QProcess::finished, this,
            [this, process, argument, timedOut](int, QProcess::ExitStatus) {
        if (m_lithogenProcess != process) return;
        const QString output = QString::fromLocal8Bit(process->readAll().left(4096));
        const QRegularExpression version(
            R"(\blithogen(?:\s+version)?\s+v?(\d+(?:\.\d+){1,3})\b)",
            QRegularExpression::CaseInsensitiveOption);
        const auto match = version.match(output);
        m_lithogenProcess = nullptr;
        process->deleteLater();
        if (*timedOut) {
            m_lithogenVersion->setText("Timed out");
            finishLithogenProbe();
        } else if (match.hasMatch()) {
            m_lithogenVersion->setText(match.captured(1));
            finishLithogenProbe();
        } else if (argument == "--version") {
            probeLithogenVersion("--help");
        } else {
            m_lithogenVersion->setText("Unknown (not reported by executable)");
            finishLithogenProbe();
        }
    });
    connect(process, &QProcess::errorOccurred, this,
            [this, process](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart || m_lithogenProcess != process) return;
        m_lithogenProcess = nullptr;
        m_lithogenAvailability->setText("Could not start");
        m_lithogenVersion->setText("Unknown");
        finishLithogenProbe();
        process->deleteLater();
    });
    process->start(m_lithogenExecutable->text(), {argument});
    QTimer::singleShot(3000, process, [process, timedOut]() {
        if (process->state() == QProcess::NotRunning) return;
        *timedOut = true;
        process->kill();
    });
}

void InfoTab::finishLithogenProbe() {
    m_lithogenPending = false;
    updateRefreshState();
}

void InfoTab::startJgrfProbe() {
    const fs::path exe = resolve_jollygood_executable(m_jollygoodExe);
    if (exe.empty()) {
        m_jgrfVersion->setText("Unavailable");
        m_jgrfExecutable->setText(QString::fromStdString(m_jollygoodExe.string()));
        m_vulkanRenderer->setText("Unknown");
        m_esBgraRenderer->setText("Unavailable");
        m_jgrfError = "Configured JGRF executable was not found.";
        return;
    }

    m_jgrfExecutable->setText(QString::fromStdString(exe.string()));
    auto* process = new QProcess(this);
    auto timedOut = std::make_shared<bool>(false);
    m_jgrfProcess = process;
    m_jgrfPending = true;
    process->setProcessChannelMode(QProcess::MergedChannels);

    connect(process, &QProcess::finished, this,
            [this, process, timedOut](int, QProcess::ExitStatus) {
        if (m_jgrfProcess != process) return;

        const QByteArray output = process->readAll();
        const std::string help(output.constData(), static_cast<std::size_t>(output.size()));
        if (*timedOut) {
            m_jgrfVersion->setText("Timed out");
            m_vulkanRenderer->setText("Unknown");
            m_jgrfProbeOk = false;
            m_jgrfError = "JGRF --help did not finish within 3 seconds.";
        } else {
            const auto version = jgrf_version_from_help(help);
            m_jgrfVersion->setText(
                version ? QString::fromStdString(*version) : "Unknown");
            m_vulkanRenderer->setText(jgrf_help_reports_vulkan(help)
                                          ? "Available (Experimental)"
                                          : "Not compiled");
            m_jgrfProbeOk = version.has_value();
            if (!m_jgrfProbeOk) {
                m_jgrfError =
                    "JGRF --help did not report a recognizable frontend version.";
            }
        }

        m_jgrfProcess = nullptr;
        finishJgrfProbe();
        process->deleteLater();
    });

    connect(process, &QProcess::errorOccurred, this,
            [this, process](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart || m_jgrfProcess != process) return;
        m_jgrfVersion->setText("Unavailable");
        m_vulkanRenderer->setText("Unknown");
        m_jgrfError = process->errorString();
        m_jgrfProcess = nullptr;
        finishJgrfProbe();
        process->deleteLater();
    });

    process->start(QString::fromStdString(exe.string()), {"--help"});

    // A broken executable should never leave the Info tab permanently busy.
    QTimer::singleShot(kJgrfHelpProbeTimeoutMs, process,
                       [process, timedOut]() {
        if (process->state() == QProcess::NotRunning) return;
        *timedOut = true;
        process->kill();
    });
}

void InfoTab::startCoreProbe() {
    fs::path exe = resolve_jollygood_executable(m_jollygoodExe);
    if (exe.empty()) exe = m_jollygoodExe;
    const fs::path coreLibrary = geolith_library_for_jgrf(exe);

    m_coreLibrary->setText(coreLibrary.empty()
                               ? "Unavailable"
                               : QString::fromStdString(coreLibrary.string()));

    m_corePending = true;
    auto result = std::make_shared<GeolithCapabilities>();
    auto verifiedEsBgra = std::make_shared<bool>(false);
    QThread* worker = QThread::create(
        [result, verifiedEsBgra, configured = m_jollygoodExe, exe]() {
            *result = probe_geolith_capabilities(configured);
#if defined(_WIN32)
            *verifiedEsBgra = hasVerifiedEsBgraPair(exe);
#else
            (void)exe;
#endif
        });

    // finished() is queued to this tab. Qt removes the connection if the tab
    // is destroyed first, so the worker never dereferences UI state directly.
    connect(worker, &QThread::finished, this, [this, result, verifiedEsBgra]() {
        m_esBgraPairVerified = *verifiedEsBgra;
        if (result->success) {
            m_geolithVersion->setText(QString::fromStdString(result->version));
            m_jgApiVersion->setText(QString::fromStdString(result->api_version));

            const std::string formats =
                geolith_extensions_display(*result, "neogeocd");
            m_neocdFormats->setText(
                formats.empty() ? "Not advertised"
                                : QString::fromStdString(formats));
            m_chdSupport->setText(
                geolith_supports_extension(*result, "neogeocd", "chd")
                    ? "Available"
                    : "Not compiled");
            m_coreProbeOk = true;
        } else {
            m_geolithVersion->setText("Unavailable");
            m_jgApiVersion->setText("Unavailable");
            m_neocdFormats->setText("Unknown");
            m_chdSupport->setText("Unknown");
            m_coreProbeOk = false;
            m_coreError = QString::fromStdString(result->error);
        }
        finishCoreProbe();
    });

    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void InfoTab::finishJgrfProbe() {
    m_jgrfPending = false;
    updateRefreshState();
}

void InfoTab::finishCoreProbe() {
    m_corePending = false;
    updateRefreshState();
}

void InfoTab::updateRefreshState() {
    const bool busy = m_jgrfPending || m_corePending || m_lithogenPending;
    if (m_refreshButton) m_refreshButton->setEnabled(!busy);

    if (!busy) {
#if defined(_WIN32)
        m_esBgraRenderer->setText(!m_jgrfProbeOk
                                      ? "Unknown"
                                      : m_esBgraPairVerified
                                            ? "Compatible (verified build)"
                                            : "Unverified build");
#else
        m_esBgraRenderer->setText("Windows fix not applicable");
#endif
        if (m_jgrfProbeOk && m_coreProbeOk) {
            m_status->setText(
                "Information was read from the installed components. Use Refresh after replacing JGRF or Geolith files.");
        } else {
            QStringList problems;
            if (!m_jgrfProbeOk && !m_jgrfError.isEmpty())
                problems << "JGRF: " + m_jgrfError;
            if (!m_coreProbeOk && !m_coreError.isEmpty())
                problems << "Geolith: " + m_coreError;
            m_status->setText(problems.isEmpty()
                                  ? "Some installed-component information could not be detected."
                                  : problems.join("\n"));
        }
    }
}

} // namespace goliath
