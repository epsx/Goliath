#include "ui/lithogen_dialog.hpp"
#include "ui/widgets/title_bar.hpp"

#include <QCheckBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QTableWidget>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>

namespace goliath {

namespace {

QString absoluteDirectory(const QString& path) {
    return QDir(path).absolutePath();
}

QString expectedOutput(const QString& directory, const QString& zipPath) {
    return QDir(directory).filePath(QFileInfo(zipPath).completeBaseName() + ".neo");
}

} // namespace

LithogenDialog::LithogenDialog(Config& config, const QString& romFolder,
                               QWidget* parent)
    : QDialog(parent), m_config(config), m_romFolder(absoluteDirectory(romFolder)) {
    resize(850, 680);

    QVBoxLayout* layout = nullptr;
    setupFramelessDialog(this, "Convert ZIP to .neo with Lithogen", &layout);
    auto* note = new QLabel(
        "Use a separately installed Lithogen executable to convert one ZIP or "
        "all ZIPs directly inside a folder. Goliath processes them one at a "
        "time and shows the result for every archive.", this);
    note->setObjectName("themed_note");
    note->setWordWrap(true);
    layout->addWidget(note);

    m_controls = new QWidget(this);
    auto* controlsLayout = new QVBoxLayout(m_controls);
    controlsLayout->setContentsMargins(0, 0, 0, 0);
    auto* modeRow = new QHBoxLayout();
    m_single = new QRadioButton("One ZIP", m_controls);
    auto* folderMode = new QRadioButton("All ZIPs in a folder", m_controls);
    m_single->setChecked(true);
    modeRow->addWidget(m_single);
    modeRow->addWidget(folderMode);
    modeRow->addStretch();
    controlsLayout->addLayout(modeRow);

    m_form = new QFormLayout();
    const auto addPath = [this](const QString& label, QLineEdit*& edit,
                                      const QString& initial, bool directory,
                                      const QString& filter,
                                      bool persistExecutable = false) {
        auto* row = new QWidget(m_controls);
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        edit = new QLineEdit(initial, row);
        auto* browse = new QPushButton("Browse...", row);
        rowLayout->addWidget(edit, 1);
        rowLayout->addWidget(browse);
        connect(browse, &QPushButton::clicked, this,
                [this, edit, directory, filter, persistExecutable]() {
            const QString start = edit->text().trimmed();
            const QString chosen = directory
                ? QFileDialog::getExistingDirectory(this, "Choose folder", start)
                : QFileDialog::getOpenFileName(this, "Choose file", start, filter);
            if (chosen.isEmpty()) return;
            edit->setText(QDir::toNativeSeparators(chosen));
            if (persistExecutable) persistExecutablePath(chosen);
        });
        m_form->addRow(label, row);
        return row;
    };
    addPath("Lithogen executable:", m_executable,
            QString::fromStdString(m_config.get("Tools", "lithogen_executable", "")),
            false, "Executables (*.exe *);;All files (*)", true);
    connect(m_executable, &QLineEdit::editingFinished, this, [this]() {
        persistExecutablePath(m_executable->text());
    });
    m_inputRow = addPath("Input ZIP:", m_input, {}, false,
                         "ZIP archives (*.zip);;All files (*)");
    m_folderRow = addPath("ZIP folder:", m_folder, {}, true, {});
    addPath("Output directory:", m_output, QDir::toNativeSeparators(m_romFolder),
            true, {});
    controlsLayout->addLayout(m_form);

    auto* outputNote = new QLabel(
        "ZIPs are read from the chosen folder only, without subfolders. "
        "Existing .neo files are skipped and never overwritten. Split clones "
        "can use their parent ZIP from that same folder automatically.", m_controls);
    outputNote->setWordWrap(true);
    outputNote->setObjectName("themed_note");
    controlsLayout->addWidget(outputNote);

    m_advanced = new QCheckBox("Advanced options", m_controls);
    controlsLayout->addWidget(m_advanced);
    m_advancedFields = new QWidget(m_controls);
    auto* advancedLayout = new QVBoxLayout(m_advancedFields);
    advancedLayout->setContentsMargins(12, 0, 0, 0);
    m_strict = new QCheckBox("Strict CRC checks (--strict)", m_advancedFields);
    m_strict->setToolTip("Treat ROM CRC mismatches as errors instead of warnings.");
    advancedLayout->addWidget(m_strict);
    m_autoParent = new QCheckBox("Find split clone parent ZIP automatically",
                                  m_advancedFields);
    m_autoParent->setChecked(true);
    m_autoParent->setToolTip(
        "Search beside a split clone ZIP for its parent ZIP. Turn off to pass "
        "--no-auto-parent to Lithogen.");
    advancedLayout->addWidget(m_autoParent);

    m_singleExtras = new QWidget(m_advancedFields);
    auto* singleForm = new QFormLayout(m_singleExtras);
    m_extraParent = new QLineEdit(m_singleExtras);
    auto* parentRow = new QWidget(m_singleExtras);
    auto* parentLayout = new QHBoxLayout(parentRow);
    parentLayout->setContentsMargins(0, 0, 0, 0);
    auto* browseParent = new QPushButton("Browse...", parentRow);
    parentLayout->addWidget(m_extraParent, 1);
    parentLayout->addWidget(browseParent);
    connect(browseParent, &QPushButton::clicked, this, [this]() {
        const QString chosen = QFileDialog::getOpenFileName(
            this, "Choose additional parent ZIP", m_extraParent->text().trimmed(),
            "ZIP archives (*.zip);;All files (*)");
        if (!chosen.isEmpty()) m_extraParent->setText(QDir::toNativeSeparators(chosen));
    });
    singleForm->addRow("Extra parent ZIP (-p):", parentRow);
    m_extraParent->setToolTip(
        "Only for unusual split sets. Lithogen uses this ZIP in addition to "
        "the selected game ZIP.");
    m_set = new QLineEdit(m_singleExtras);
    m_set->setPlaceholderText("e.g. kof2002");
    m_set->setToolTip(
        "Override the ROM set identifier when the ZIP filename is not the "
        "MAME set ID (--set). Leave empty for automatic identification.");
    singleForm->addRow("Set ID override (--set):", m_set);
    advancedLayout->addWidget(m_singleExtras);
    auto* advancedNote = new QLabel(
        "The extra parent and set ID apply only to one ZIP. In folder mode, "
        "keep parent ZIPs in the chosen folder; Lithogen finds them for "
        "split clones automatically.", m_advancedFields);
    advancedNote->setWordWrap(true);
    advancedNote->setObjectName("themed_note");
    advancedLayout->addWidget(advancedNote);
    controlsLayout->addWidget(m_advancedFields);
    layout->addWidget(m_controls);

    m_summary = new QLabel("Ready to convert.", this);
    layout->addWidget(m_summary);
    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 1);
    m_progress->setValue(0);
    m_progress->setFormat("%v / %m ZIPs checked");
    layout->addWidget(m_progress);
    m_results = new QTableWidget(this);
    m_results->setColumnCount(2);
    m_results->setHorizontalHeaderLabels({"ZIP", "Result"});
    m_results->horizontalHeader()->setStretchLastSection(true);
    m_results->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_results->verticalHeader()->hide();
    m_results->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(m_results, 1);
    m_log = new QTextEdit(this);
    m_log->setReadOnly(true);
    m_log->document()->setMaximumBlockCount(1000);
    layout->addWidget(m_log, 1);

    auto* buttons = new QHBoxLayout();
    m_convert = new QPushButton("Convert", this);
    m_stop = new QPushButton("Stop conversion", this);
    m_rescan = new QPushButton("Rescan ROMs", this);
    auto* close = new QPushButton("Close", this);
    m_stop->setEnabled(false);
    m_rescan->setEnabled(false);
    buttons->addWidget(m_convert);
    buttons->addWidget(m_stop);
    buttons->addStretch();
    buttons->addWidget(m_rescan);
    buttons->addWidget(close);
    layout->addLayout(buttons);

    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    m_process->setStandardInputFile(QProcess::nullDevice());
    connect(m_process, &QProcess::readyReadStandardOutput, this, [this]() {
        appendOutput(QString::fromLocal8Bit(m_process->readAllStandardOutput()));
    });
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart || !m_active) return;
        appendOutput("Could not start Lithogen: " + m_process->errorString() + "\n");
        setJobResult(m_current, "Could not start executable");
        ++m_failed;
        ++m_current;
        m_progress->setValue(m_current);
        finishRun();
    });
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int code, QProcess::ExitStatus status) {
                conversionFinished(code, status);
            });
    connect(m_single, &QRadioButton::toggled, this, [this]() { updateInputMode(); });
    connect(m_advanced, &QCheckBox::toggled, m_advancedFields,
            &QWidget::setVisible);
    connect(m_convert, &QPushButton::clicked, this, [this]() { startConversion(); });
    connect(m_stop, &QPushButton::clicked, this, [this]() { stopConversion(); });
    connect(m_rescan, &QPushButton::clicked, this, [this]() { accept(); });
    connect(close, &QPushButton::clicked, this, [this]() { reject(); });
    m_advancedFields->hide();
    updateInputMode();
}

void LithogenDialog::updateInputMode() {
    m_inputRow->setVisible(m_single->isChecked());
    m_folderRow->setVisible(!m_single->isChecked());
    m_form->labelForField(m_inputRow)->setVisible(m_single->isChecked());
    m_form->labelForField(m_folderRow)->setVisible(!m_single->isChecked());
    m_singleExtras->setEnabled(m_single->isChecked());
}

void LithogenDialog::appendOutput(const QString& text) {
    if (text.isEmpty()) return;
    m_log->moveCursor(QTextCursor::End);
    m_log->insertPlainText(text);
}

void LithogenDialog::setJobResult(int row, const QString& result) {
    if (row < 0 || row >= m_results->rowCount()) return;
    m_results->item(row, 1)->setText(result);
    m_results->scrollToItem(m_results->item(row, 1));
}

void LithogenDialog::persistExecutablePath(const QString& path) {
    const QFileInfo executable(path.trimmed());
    if (!executable.isFile()) return;

    const QString absolutePath = executable.absoluteFilePath();
    const std::string storedPath = absolutePath.toStdString();
    if (m_config.get("Tools", "lithogen_executable", "") == storedPath)
        return;

    m_config.set("Tools", "lithogen_executable", storedPath);
    save_config(m_config);
}

void LithogenDialog::startConversion() {
    if (m_active || m_process->state() != QProcess::NotRunning) return;
    const QFileInfo executable(m_executable->text().trimmed());
    if (!executable.isFile() || !executable.isExecutable()) {
        QMessageBox::warning(this, "Lithogen", "Choose an executable Lithogen file.");
        return;
    }
    const QString output = m_output->text().trimmed();
    if (output.isEmpty() || !QDir(output).exists()) {
        QMessageBox::warning(this, "Lithogen", "Choose an existing output directory.");
        return;
    }
    const QString targetDir = QDir(output).canonicalPath();
    if (targetDir.isEmpty()) {
        QMessageBox::warning(this, "Lithogen", "Cannot access the output directory.");
        return;
    }

    QStringList jobs;
    const bool batch = !m_single->isChecked();
    if (batch) {
        QDir folder(m_folder->text().trimmed());
        if (m_folder->text().trimmed().isEmpty() || !folder.exists()) {
            QMessageBox::warning(this, "Lithogen", "Choose an existing ZIP folder.");
            return;
        }
        const QFileInfoList entries = folder.entryInfoList(QDir::Files | QDir::Readable,
                                                            QDir::Name | QDir::IgnoreCase);
        for (const QFileInfo& entry : entries) {
            if (entry.suffix().compare("zip", Qt::CaseInsensitive) == 0)
                jobs.append(entry.absoluteFilePath());
        }
        if (jobs.size() > 2000) {
            QMessageBox::warning(this, "Lithogen",
                                 "This folder contains over 2000 ZIPs. Choose a smaller folder.");
            return;
        }
        if (jobs.isEmpty()) {
            QMessageBox::warning(this, "Lithogen",
                                 "No .zip archives were found directly in this folder.");
            return;
        }
    } else {
        const QFileInfo input(m_input->text().trimmed());
        if (!input.isFile() || input.suffix().compare("zip", Qt::CaseInsensitive) != 0) {
            QMessageBox::warning(this, "Lithogen", "Choose one existing .zip archive.");
            return;
        }
        jobs.append(input.absoluteFilePath());
    }

    QString extraParent;
    QString setId;
    if (m_advanced->isChecked() && !batch) {
        extraParent = m_extraParent->text().trimmed();
        if (!extraParent.isEmpty()) {
            const QFileInfo parentInfo(extraParent);
            if (!parentInfo.isFile() ||
                parentInfo.suffix().compare("zip", Qt::CaseInsensitive) != 0) {
                QMessageBox::warning(this, "Lithogen",
                                     "The additional parent must be an existing .zip file.");
                return;
            }
            extraParent = parentInfo.absoluteFilePath();
        }
        setId = m_set->text().trimmed();
        static const QRegularExpression validId("^[a-zA-Z0-9_]+$");
        if (!setId.isEmpty() && !validId.match(setId).hasMatch()) {
            QMessageBox::warning(this, "Lithogen",
                "Use only ASCII letters, numbers and underscore for the set ID.");
            return;
        }
    }

    persistExecutablePath(executable.absoluteFilePath());
    m_executablePath = executable.absoluteFilePath();
    m_targetDir = targetDir;
    m_parentPath = extraParent;
    m_setId = setId;
    m_useStrict = m_advanced->isChecked() && m_strict->isChecked();
    m_useAutoParent = !m_advanced->isChecked() || m_autoParent->isChecked();
    m_batch = batch;
    m_jobs = jobs;
    m_current = m_created = m_failed = m_skipped = 0;
    m_stopRequested = false;
    m_canRescan = false;
    m_active = true;
    m_controls->setEnabled(false);
    m_convert->setEnabled(false);
    m_stop->setEnabled(true);
    m_rescan->setEnabled(false);
    m_log->clear();
    m_results->setRowCount(m_jobs.size());
    for (int i = 0; i < m_jobs.size(); ++i) {
        m_results->setItem(i, 0, new QTableWidgetItem(QFileInfo(m_jobs[i]).fileName()));
        m_results->setItem(i, 1, new QTableWidgetItem("Waiting"));
    }
    m_progress->setRange(0, m_jobs.size());
    m_progress->setValue(0);
    m_summary->setText(QString("Queued %1 ZIP(s).").arg(m_jobs.size()));
    startNextJob();
}

void LithogenDialog::startNextJob() {
    while (m_current < m_jobs.size() && !m_stopRequested) {
        const QString& zip = m_jobs[m_current];
        const QString outputPath = expectedOutput(m_targetDir, zip);
        if (QFileInfo::exists(outputPath)) {
            setJobResult(m_current, "Skipped: .neo already exists");
            appendOutput(QString("[%1/%2] Skipped %3: .neo already exists.\n")
                .arg(m_current + 1).arg(m_jobs.size()).arg(QFileInfo(zip).fileName()));
            ++m_skipped;
            m_progress->setValue(++m_current);
            continue;
        }
        m_summary->setText(QString("Converting %1 of %2: %3")
            .arg(m_current + 1).arg(m_jobs.size()).arg(QFileInfo(zip).fileName()));
        setJobResult(m_current, "Converting...");
        appendOutput(QString("\n[%1/%2] %3\n")
            .arg(m_current + 1).arg(m_jobs.size()).arg(QFileInfo(zip).fileName()));
        QStringList args;
        if (m_useStrict) args << "--strict";
        if (!m_useAutoParent) args << "--no-auto-parent";
        if (!m_batch && !m_parentPath.isEmpty()) args << "-p" << m_parentPath;
        if (!m_batch && !m_setId.isEmpty()) args << "--set" << m_setId;
        args << "-o" << (m_targetDir + "/") << zip;
        m_process->setProgram(m_executablePath);
        m_process->setArguments(args);
        m_process->setWorkingDirectory(QFileInfo(zip).absolutePath());
        m_process->start();
        return;
    }
    finishRun();
}

void LithogenDialog::stopConversion() {
    if (!m_active || m_stopRequested) return;
    m_stopRequested = true;
    m_stop->setEnabled(false);
    appendOutput("\nStopping conversion...\n");
    if (m_process->state() == QProcess::NotRunning) {
        finishRun();
        return;
    }
    m_process->terminate();
    QTimer::singleShot(3000, m_process, [this]() {
        if (m_stopRequested && m_process->state() != QProcess::NotRunning)
            m_process->kill();
    });
}

void LithogenDialog::conversionFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    if (!m_active) return;
    appendOutput(QString::fromLocal8Bit(m_process->readAllStandardOutput()));
    const QString output = expectedOutput(m_targetDir, m_jobs[m_current]);
    if (m_stopRequested) {
        setJobResult(m_current, "Canceled (check for partial output)");
        appendOutput("\nCanceled. Check the output folder for partial files.\n");
    } else if (exitStatus != QProcess::NormalExit || exitCode != 0) {
        setJobResult(m_current, QString("Failed (exit %1)").arg(exitCode));
        appendOutput(QString("\nFailed (exit %1). See Lithogen output above.\n").arg(exitCode));
        ++m_failed;
    } else if (!QFileInfo::exists(output) || QFileInfo(output).size() == 0) {
        setJobResult(m_current, "No nonempty .neo created");
        appendOutput("\nNo nonempty .neo was found for this ZIP. "
                     "The set may be unknown to Lithogen.\n");
        ++m_failed;
    } else {
        setJobResult(m_current, "Created");
        appendOutput("\nCreated: " + QDir::toNativeSeparators(output) + "\n");
        ++m_created;
        if (QDir(m_targetDir).canonicalPath() == QDir(m_romFolder).canonicalPath())
            m_canRescan = true;
    }
    m_progress->setValue(++m_current);
    startNextJob();
}

void LithogenDialog::finishRun() {
    if (!m_active) return;
    for (int i = m_current; i < m_jobs.size(); ++i)
        m_results->item(i, 1)->setText("Not started");
    m_active = false;
    m_controls->setEnabled(true);
    m_convert->setEnabled(true);
    m_stop->setEnabled(false);
    m_rescan->setEnabled(m_canRescan);
    const int unprocessed = m_jobs.size() - m_current;
    m_summary->setText(QString("Created: %1  |  Failed: %2  |  Existing: %3"
                                "  |  Not started: %4")
        .arg(m_created).arg(m_failed).arg(m_skipped).arg(unprocessed));
    if (m_canRescan)
        appendOutput("\nSelect Rescan ROMs to refresh Goliath's MVS/AES library.\n");
    else if (m_created > 0)
        appendOutput("\nTo find these games, select this output directory as "
                     "Goliath's MVS/AES ROM directory.\n");
}

void LithogenDialog::reject() {
    if (m_active) {
        stopConversion();
        return;
    }
    QDialog::reject();
}

} // namespace goliath
