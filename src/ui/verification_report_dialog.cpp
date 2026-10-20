// verification_report_dialog.cpp - readable verification totals and every
// affected exact-media entry, without burying problems in a tooltip.
#include "ui/verification_report_dialog.hpp"

#include "game/library_rescan_comparison.hpp"
#include "ui/widgets/engraved_controls.hpp"
#include "ui/widgets/title_bar.hpp"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

#include <limits>

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
    const std::vector<LibraryVerificationIssue> issues =
        collect_library_verification_issues(
            games, std::numeric_limits<std::size_t>::max());

    auto* heading = new QLabel("Current Library Verification");
    heading->setObjectName("verification_report_heading");
    content->addWidget(heading);

    auto* explanation = new QLabel(
        "Green totals are exact hash matches. Red entries require attention. "
        "Metadata-only items are recognized but cannot be hash-verified.");
    explanation->setObjectName("verification_report_explanation");
    explanation->setWordWrap(true);
    content->addWidget(explanation);

    auto* metrics = new QHBoxLayout();
    metrics->setSpacing(10);
    const auto addMetric = [&](const QString& title, std::size_t value,
                               const QString& status,
                               const QString& detail) {
        auto* card = new QFrame();
        card->setObjectName("verification_metric_card");
        card->setProperty("verificationStatus", status);
        auto* layout = new QVBoxLayout(card);
        layout->setContentsMargins(14, 10, 14, 10);
        layout->setSpacing(3);
        auto* titleLabel = new QLabel(title, card);
        titleLabel->setObjectName("verification_metric_title");
        layout->addWidget(titleLabel);
        auto* valueLabel = new QLabel(
            QString::number(static_cast<qulonglong>(value)), card);
        valueLabel->setObjectName("verification_metric_value");
        layout->addWidget(valueLabel);
        auto* detailLabel = new QLabel(detail, card);
        detailLabel->setObjectName("verification_metric_detail");
        detailLabel->setWordWrap(true);
        layout->addWidget(detailLabel);
        metrics->addWidget(card, 1);
    };

    addMetric("Verified", summary.verified, "good", "Exact hash matches");
    addMetric(
        "Mismatches", summary.mismatch_count(), "bad",
        QString("CUE %1  ·  CHD %2  ·  CRC-32 %3")
            .arg(static_cast<qulonglong>(summary.cue_mismatches))
            .arg(static_cast<qulonglong>(summary.chd_mismatches))
            .arg(static_cast<qulonglong>(summary.crc32_mismatches)));
    addMetric("Metadata only", summary.metadata_only, "neutral",
              "Recognized, no exact hash proof");
    addMetric("Unknown", summary.unknown,
              summary.unknown == 0 ? "neutral" : "bad",
              "Images not identified");
    content->addLayout(metrics);

    auto* issueHeading = new QLabel(
        summary.issue_count() == 0
            ? QStringLiteral("No verification problems")
            : QString("Problems requiring attention (%1)")
                  .arg(static_cast<qulonglong>(summary.issue_count())));
    issueHeading->setObjectName("verification_issue_heading");
    content->addWidget(issueHeading);

    auto* scroll = new QScrollArea();
    scroll->setObjectName("verification_report_scroll");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* scrollContents = new QWidget();
    scrollContents->setObjectName("verification_report_contents");
    auto* issueLayout = new QVBoxLayout(scrollContents);
    issueLayout->setContentsMargins(0, 0, 0, 0);
    issueLayout->setSpacing(8);

    if (issues.empty()) {
        auto* allGood = new QFrame(scrollContents);
        allGood->setObjectName("verification_issue_card");
        allGood->setProperty("verificationStatus", "good");
        auto* layout = new QVBoxLayout(allGood);
        auto* title = new QLabel(
            QString::fromUtf8("\xE2\x9C\x93 No mismatches or unknown images"),
            allGood);
        title->setObjectName("verification_issue_title");
        layout->addWidget(title);
        issueLayout->addWidget(allGood);
    } else {
        for (const LibraryVerificationIssue& issue : issues) {
            auto* card = new QFrame(scrollContents);
            card->setObjectName("verification_issue_card");
            card->setProperty("verificationStatus", "bad");
            auto* layout = new QVBoxLayout(card);
            layout->setContentsMargins(14, 10, 14, 10);
            layout->setSpacing(4);

            auto* title = new QLabel(
                QString::fromStdString(issue.display), card);
            title->setObjectName("verification_issue_title");
            title->setWordWrap(true);
            layout->addWidget(title);

            auto* problem = new QLabel(
                QString::fromUtf8("\xE2\x9A\xA0 ") +
                    QString::fromStdString(issue.problem),
                card);
            problem->setObjectName("verification_issue_problem");
            problem->setWordWrap(true);
            layout->addWidget(problem);

            if (!issue.media.empty()) {
                auto* media = new QLabel(
                    QString("Media: %1")
                        .arg(QString::fromStdString(issue.media)),
                    card);
                media->setObjectName("verification_issue_media");
                media->setWordWrap(true);
                media->setTextInteractionFlags(Qt::TextSelectableByMouse);
                layout->addWidget(media);
            }
            issueLayout->addWidget(card);
        }
    }
    issueLayout->addStretch();
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
}

} // namespace goliath
