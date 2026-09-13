#include "ui/rescan_dialog.hpp"
#include "game/rescan_worker.hpp"
#include "ui/widgets/title_bar.hpp"

#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

namespace goliath {

RescanDialog::RescanDialog(RescanWorker* worker, QWidget* parent) : QDialog(parent), m_worker(worker) {
    resize(640, 520);

    QVBoxLayout* layout = nullptr;
    setupFramelessDialog(this, "Rescan ROMs", &layout);

    m_statusLabel = new QLabel("Initializing scan...");
    layout->addWidget(m_statusLabel);

    m_progress = new QProgressBar();
    // Scanner progress is status-based rather than percentage-based.
    m_progress->setRange(0, 0);
    layout->addWidget(m_progress);

    m_summaryText = new QTextEdit();
    m_summaryText->setReadOnly(true);
    m_summaryText->setVisible(false);
    layout->addWidget(m_summaryText, 1);

    m_closeBtn = new QPushButton("OK");
    m_closeBtn->setEnabled(false);
    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    layout->addWidget(m_closeBtn);

    connect(m_worker, &RescanWorker::outputLine, this, &RescanDialog::onOutput);
    connect(m_worker, &RescanWorker::completed, this, &RescanDialog::onCompleted);
    connect(m_worker, &RescanWorker::error, this, &RescanDialog::onError);
}

void RescanDialog::onOutput(QString line) {
    m_outputLines.append(line);
    m_statusLabel->setText(line);

    // The summary arrives as one multiline message beginning with a newline.
    // trimmed() makes delimiter detection independent of that leading newline.
    if (line.trimmed().startsWith("==========")) {
        m_summaryStarted = true;
    }
    if (m_summaryStarted) {
        m_summaryLines.append(line);
        if (line.trimmed().endsWith("======================================")) {
            m_summaryStarted = false;
        }
    }
}

void RescanDialog::onCompleted() {
    m_statusLabel->setText("Rescan completed.");
    m_progress->setRange(0, 100);
    m_progress->setValue(100);

    if (!m_summaryLines.isEmpty()) {
        m_summaryText->setText(m_summaryLines.join("\n"));
    } else {
        QStringList lastLines = m_outputLines.mid(std::max(qsizetype(0), m_outputLines.size() - 10));
        m_summaryText->setText(lastLines.join("\n"));
    }
    m_summaryText->setVisible(true);
    m_closeBtn->setEnabled(true);
    m_closeBtn->setDefault(true);
    m_closeBtn->setFocus();
}

void RescanDialog::onError(QString msg) {
    m_statusLabel->setText("Rescan failed.");
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    m_summaryText->setText(msg);
    m_summaryText->setVisible(true);
    m_closeBtn->setEnabled(true);
}

} // namespace goliath
