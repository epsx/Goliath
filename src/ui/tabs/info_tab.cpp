#include "info_tab.hpp"

#include "game/jollygood_capabilities.hpp"
#include "game/jollygood_executable.hpp"
#include "game/geolith_capabilities.hpp"

#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLibrary>
#include <QProcess>
#include <QPushButton>
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

} // namespace

InfoTab::InfoTab(fs::path jollygoodExe, QWidget* parent)
    : QWidget(parent), m_jollygoodExe(std::move(jollygoodExe)) {
    setupUi();
}

void InfoTab::setupUi() {
    auto* layout = new QVBoxLayout(this);

    auto* jgrfGroup = new QGroupBox("JollyGood Reference Frontend (JGRF)");
    auto* jgrfForm = new QFormLayout(jgrfGroup);
    m_jgrfVersion = valueLabel();
    m_jgrfExecutable = valueLabel();
    m_vulkanRenderer = valueLabel();
    jgrfForm->addRow("Version:", m_jgrfVersion);
    jgrfForm->addRow("Executable:", m_jgrfExecutable);
    jgrfForm->addRow("Vulkan Renderer:", m_vulkanRenderer);
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

void InfoTab::refresh() {
    if (m_jgrfPending || m_corePending) return;

    m_jgrfVersion->setText("Detecting...");
    m_jgrfExecutable->setText("Detecting...");
    m_vulkanRenderer->setText("Detecting...");
    m_geolithVersion->setText("Detecting...");
    m_coreLibrary->setText("Detecting...");
    m_neocdFormats->setText("Detecting...");
    m_chdSupport->setText("Detecting...");
    m_jgApiVersion->setText("Detecting...");
    m_status->setText("Reading installed JGRF / Geolith information...");
    m_jgrfProbeOk = false;
    m_coreProbeOk = false;
    m_jgrfError.clear();
    m_coreError.clear();

    startJgrfProbe();
    startCoreProbe();
    updateRefreshState();
}

void InfoTab::startJgrfProbe() {
    const fs::path exe = resolve_jollygood_executable(m_jollygoodExe);
    if (exe.empty()) {
        m_jgrfVersion->setText("Unavailable");
        m_jgrfExecutable->setText(QString::fromStdString(m_jollygoodExe.string()));
        m_vulkanRenderer->setText("Unknown");
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
    QThread* worker = QThread::create(
        [result, configured = m_jollygoodExe]() {
            *result = probe_geolith_capabilities(configured);
        });

    // finished() is queued to this tab. Qt removes the connection if the tab
    // is destroyed first, so the worker never dereferences UI state directly.
    connect(worker, &QThread::finished, this, [this, result]() {
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
    const bool busy = m_jgrfPending || m_corePending;
    if (m_refreshButton) m_refreshButton->setEnabled(!busy);

    if (!busy) {
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
