// verification_report_dialog.cpp - readable verification totals and every
// affected exact-media entry, without burying problems in a tooltip.
#include "ui/verification_report_dialog.hpp"

#include "game/library_rescan_comparison.hpp"
#include "ui/widgets/engraved_controls.hpp"
#include "ui/widgets/title_bar.hpp"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSizePolicy>
#include <QStyle>
#include <QVBoxLayout>
#include <QWidget>

namespace goliath {

VerificationReportDialog::VerificationReportDialog(
        const std::vector<Game>& games, QWidget* parent)
    : QDialog(parent) {
    setWindowTitle("Library Verification Report");
    setModal(true);
    resize(900, 680);
    setMinimumSize(700, 500);

    QVBoxLayout* content = nullptr;
    setupFramelessDialog(this, windowTitle(), &content, true, true);
    content->setSpacing(12);

    const LibraryVerificationSummary summary =
        summarize_library_verification(games);
    m_entries = collect_library_verification_entries(games);

    auto* heading = new QLabel("Current Library Verification");
    heading->setObjectName("verification_report_heading");
    content->addWidget(heading);

    auto* explanation = new QLabel(
        "Green totals are exact hash matches. Red entries require attention. "
        "Metadata-only items are recognized but cannot be hash-verified. "
        "Select Mismatches, Metadata only, or Unknown to filter the list below.");
    explanation->setObjectName("verification_report_explanation");
    explanation->setWordWrap(true);
    content->addWidget(explanation);

    auto* metrics = new QHBoxLayout();
    metrics->setSpacing(10);
    const auto addMetric = [&](const QString& title, std::size_t value,
                               const QString& status,
                               const QString& filter,
                               const QString& detail) {
        QPushButton* button = nullptr;
        QWidget* card = nullptr;
        if (filter.isEmpty()) {
            card = new QFrame();
        } else {
            button = new QPushButton();
            card = button;
        }
        card->setObjectName("verification_metric_card");
        card->setProperty("verificationStatus", status);
        card->setProperty("verificationFilter", filter);
        if (button != nullptr) {
            button->setProperty("verificationInteractive", true);
            button->setCursor(Qt::PointingHandCursor);
            button->setToolTip(
                QString("Show only %1 entries").arg(title));
            button->setAccessibleName(QString("Filter by %1").arg(title));
        }
        card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        auto* layout = new QVBoxLayout(card);
        layout->setContentsMargins(14, 10, 14, 10);
        layout->setSpacing(3);
        auto* titleLabel = new QLabel(title, card);
        titleLabel->setObjectName("verification_metric_title");
        titleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        layout->addWidget(titleLabel);
        auto* valueLabel = new QLabel(
            QString::number(static_cast<qulonglong>(value)), card);
        valueLabel->setObjectName("verification_metric_value");
        valueLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        layout->addWidget(valueLabel);
        auto* detailLabel = new QLabel(detail, card);
        detailLabel->setObjectName("verification_metric_detail");
        detailLabel->setWordWrap(true);
        detailLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        layout->addWidget(detailLabel);
        metrics->addWidget(card, 1);
        return button;
    };

    addMetric("Verified", summary.verified, "good", {},
              "Exact hash matches");
    auto* mismatchCard = addMetric(
        "Mismatches", summary.mismatch_count(), "bad", "mismatch",
        QString("CUE %1  ·  CHD %2  ·  CRC-32 %3")
            .arg(static_cast<qulonglong>(summary.cue_mismatches))
            .arg(static_cast<qulonglong>(summary.chd_mismatches))
            .arg(static_cast<qulonglong>(summary.crc32_mismatches)));
    auto* metadataCard = addMetric(
        "Metadata only", summary.metadata_only, "neutral", "metadata",
        "Recognized, no exact hash proof");
    auto* unknownCard = addMetric(
        "Unknown", summary.unknown,
        summary.unknown == 0 ? "neutral" : "bad", "unknown",
        "Images not identified");
    connect(mismatchCard, &QPushButton::clicked, this, [this]() {
        showEntries(LibraryVerificationStatus::Mismatch);
    });
    connect(metadataCard, &QPushButton::clicked, this, [this]() {
        showEntries(LibraryVerificationStatus::MetadataOnly);
    });
    connect(unknownCard, &QPushButton::clicked, this, [this]() {
        showEntries(LibraryVerificationStatus::Unknown);
    });
    content->addLayout(metrics);

    m_entryHeading = new QLabel();
    m_entryHeading->setObjectName("verification_issue_heading");
    content->addWidget(m_entryHeading);

    auto* scroll = new QScrollArea();
    scroll->setObjectName("verification_report_scroll");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* scrollContents = new QWidget();
    scrollContents->setObjectName("verification_report_contents");
    m_entryLayout = new QVBoxLayout(scrollContents);
    m_entryLayout->setContentsMargins(0, 0, 0, 0);
    m_entryLayout->setSpacing(8);
    scroll->setWidget(scrollContents);
    content->addWidget(scroll, 1);

    auto* footer = new QHBoxLayout();
    footer->addStretch();
    auto* closeButton = new EngravedActionButton(
        "Close", EngravedActionButton::Glyph::Cancel, this);
    connect(closeButton, &QPushButton::clicked,
            this, &QDialog::accept);
    footer->addWidget(closeButton);
    content->addLayout(footer);

    showEntries(LibraryVerificationStatus::Mismatch);
}

void VerificationReportDialog::showEntries(
        LibraryVerificationStatus status) {
    QString filter;
    QString heading;
    QString emptyMessage;
    QString emptyStatus = "neutral";
    switch (status) {
    case LibraryVerificationStatus::Verified:
        filter = "verified";
        heading = "Verified entries";
        emptyMessage = "No exact hash matches";
        emptyStatus = "good";
        break;
    case LibraryVerificationStatus::Mismatch:
        filter = "mismatch";
        heading = "Mismatches requiring attention";
        emptyMessage = "No mismatches";
        emptyStatus = "good";
        break;
    case LibraryVerificationStatus::MetadataOnly:
        filter = "metadata";
        heading = "Metadata-only entries";
        emptyMessage = "No metadata-only entries";
        break;
    case LibraryVerificationStatus::Unknown:
        filter = "unknown";
        heading = "Unknown images";
        emptyMessage = "No unknown images";
        break;
    }

    std::size_t count = 0;
    for (const LibraryVerificationEntry& entry : m_entries) {
        if (entry.status == status) ++count;
    }
    m_entryHeading->setText(
        QString("%1 (%2)")
            .arg(heading)
            .arg(static_cast<qulonglong>(count)));

    const auto metricCards =
        findChildren<QPushButton*>("verification_metric_card");
    for (QPushButton* card : metricCards) {
        card->setProperty(
            "verificationSelected",
            card->property("verificationFilter").toString() == filter);
        card->style()->unpolish(card);
        card->style()->polish(card);
    }

    while (QLayoutItem* item = m_entryLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }

    if (count == 0) {
        auto* emptyCard = new QFrame();
        emptyCard->setObjectName("verification_issue_card");
        emptyCard->setProperty("verificationStatus", emptyStatus);
        auto* layout = new QVBoxLayout(emptyCard);
        auto* title = new QLabel(
            QString::fromUtf8("\xE2\x9C\x93 ") + emptyMessage,
            emptyCard);
        title->setObjectName("verification_issue_title");
        layout->addWidget(title);
        m_entryLayout->addWidget(emptyCard);
    } else {
        for (const LibraryVerificationEntry& entry : m_entries) {
            if (entry.status != status) continue;

            auto* card = new QFrame();
            card->setObjectName("verification_issue_card");
            const bool needsAttention =
                status == LibraryVerificationStatus::Mismatch ||
                status == LibraryVerificationStatus::Unknown;
            card->setProperty(
                "verificationStatus",
                status == LibraryVerificationStatus::Verified
                    ? "good"
                    : (needsAttention ? "bad" : "neutral"));
            auto* layout = new QVBoxLayout(card);
            layout->setContentsMargins(14, 10, 14, 10);
            layout->setSpacing(4);

            auto* title = new QLabel(
                QString::fromStdString(entry.display), card);
            title->setObjectName("verification_issue_title");
            title->setWordWrap(true);
            layout->addWidget(title);

            QString prefix = QString::fromUtf8("\xE2\x84\xB9 ");
            QString detailObject = "verification_issue_detail";
            if (status == LibraryVerificationStatus::Verified) {
                prefix = QString::fromUtf8("\xE2\x9C\x93 ");
                detailObject = "verification_issue_success";
            } else if (needsAttention) {
                prefix = QString::fromUtf8("\xE2\x9A\xA0 ");
                detailObject = "verification_issue_problem";
            }
            auto* detail = new QLabel(
                prefix + QString::fromStdString(entry.detail), card);
            detail->setObjectName(detailObject);
            detail->setWordWrap(true);
            layout->addWidget(detail);

            if (!entry.media.empty()) {
                auto* media = new QLabel(
                    QString("Media: %1")
                        .arg(QString::fromStdString(entry.media)),
                    card);
                media->setObjectName("verification_issue_media");
                media->setWordWrap(true);
                media->setTextInteractionFlags(Qt::TextSelectableByMouse);
                layout->addWidget(media);
            }
            m_entryLayout->addWidget(card);
        }
    }
    m_entryLayout->addStretch();
    if (auto* scroll =
            findChild<QScrollArea*>("verification_report_scroll")) {
        scroll->verticalScrollBar()->setValue(0);
    }
}

} // namespace goliath
