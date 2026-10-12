#include "ui/rescan_dialog.hpp"

#include "ui/widgets/title_bar.hpp"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QProgressBar>
#include <QPushButton>
#include <QStringList>
#include <QTextEdit>
#include <QTimer>
#include <QVariant>
#include <QVBoxLayout>

#include <algorithm>
#include <cstddef>

namespace goliath {

namespace {

constexpr int kCompletedDialogHeight = 610;

void schedule_content_resize(QDialog* dialog) {
    QTimer::singleShot(0, dialog, [dialog]() {
        if (QLayout* dialogLayout = dialog->layout()) {
            dialogLayout->invalidate();
            dialogLayout->activate();
        }
        const int preferredHeight = std::max(
            kCompletedDialogHeight, dialog->sizeHint().height());
        if (dialog->height() != preferredHeight)
            dialog->resize(dialog->width(), preferredHeight);
    });
}

QString count_text(std::size_t value) {
    return QString::number(static_cast<qulonglong>(value));
}

QString milliseconds_text(std::int64_t value) {
    return QString::number(static_cast<qlonglong>(value));
}

void add_metric(QGridLayout* grid, int row, const QString& label,
                const QString& value, bool problem = false) {
    auto* name = new QLabel(label);
    name->setObjectName("rescan_metric_label");
    grid->addWidget(name, row, 0);

    auto* amount = new QLabel(value);
    amount->setObjectName(problem ? "rescan_metric_problem"
                                  : "rescan_metric_value");
    amount->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    amount->setTextInteractionFlags(Qt::TextSelectableByMouse);
    grid->addWidget(amount, row, 1);
}

QFrame* make_card(const QString& heading) {
    auto* card = new QFrame();
    card->setObjectName("rescan_summary_card");
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(8);

    auto* title = new QLabel(heading, card);
    title->setObjectName("rescan_card_heading");
    layout->addWidget(title);
    return card;
}

QGridLayout* add_metrics_grid(QFrame* card) {
    auto* layout = qobject_cast<QVBoxLayout*>(card->layout());
    auto* grid = new QGridLayout();
    grid->setContentsMargins(0, 2, 0, 0);
    grid->setHorizontalSpacing(18);
    grid->setVerticalSpacing(5);
    grid->setColumnStretch(0, 1);
    layout->addLayout(grid);
    layout->addStretch();
    return grid;
}

QFrame* make_change_card(const QString& label, std::size_t value,
                         const QString& kind) {
    auto* card = new QFrame();
    card->setObjectName("rescan_change_card");
    card->setProperty("changeKind", kind);
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(14, 10, 14, 10);
    layout->setSpacing(1);

    auto* amount = new QLabel(count_text(value), card);
    amount->setObjectName("rescan_change_value");
    amount->setAlignment(Qt::AlignCenter);
    layout->addWidget(amount);

    auto* name = new QLabel(label, card);
    name->setObjectName("rescan_change_label");
    name->setAlignment(Qt::AlignCenter);
    layout->addWidget(name);
    return card;
}

void append_changes(QStringList& lines, const QString& heading,
                    const std::vector<LibraryRescanChange>& changes) {
    if (changes.empty()) return;
    if (!lines.isEmpty()) lines.append(QString());
    lines.append(QString("%1 (%2)")
                     .arg(heading)
                     .arg(static_cast<qulonglong>(changes.size())));
    for (const LibraryRescanChange& change : changes) {
        const QString system = change.system == "neogeocd"
            ? "Neo Geo CD" : "MVS/AES";
        lines.append(QString("  %1 [%2] — %3")
                         .arg(QString::fromStdString(change.display), system,
                              QString::fromStdString(change.media)));
    }
}

} // namespace

RescanDialog::RescanDialog(RescanWorker* worker, QWidget* parent)
    : QDialog(parent),
      m_statusLabel(nullptr),
      m_progress(nullptr),
      m_reportContainer(nullptr),
      m_reportLayout(nullptr),
      m_errorText(nullptr),
      m_changesButton(nullptr),
      m_changesDetails(nullptr),
      m_technicalButton(nullptr),
      m_technicalDetails(nullptr),
      m_closeBtn(nullptr) {
    resize(640, 180);
    setMinimumWidth(560);

    QVBoxLayout* layout = nullptr;
    setupFramelessDialog(this, "Rescan ROMs", &layout);

    m_statusLabel = new QLabel("Initializing scan...", this);
    m_statusLabel->setObjectName("rescan_status");
    m_statusLabel->setWordWrap(true);
    layout->addWidget(m_statusLabel);

    m_progress = new QProgressBar(this);
    // Scanner progress is status-based rather than percentage-based.
    m_progress->setRange(0, 0);
    layout->addWidget(m_progress);

    m_reportContainer = new QWidget(this);
    m_reportLayout = new QVBoxLayout(m_reportContainer);
    m_reportLayout->setContentsMargins(0, 0, 0, 0);
    m_reportLayout->setSpacing(10);
    m_reportContainer->setVisible(false);
    layout->addWidget(m_reportContainer, 1);

    m_errorText = new QTextEdit(this);
    m_errorText->setReadOnly(true);
    m_errorText->setVisible(false);
    layout->addWidget(m_errorText, 1);

    auto* buttonRow = new QHBoxLayout();
    m_changesButton = new QPushButton("Show Changes", this);
    m_changesButton->setCheckable(true);
    m_changesButton->setVisible(false);
    buttonRow->addWidget(m_changesButton);

    m_technicalButton = new QPushButton("Technical Details", this);
    m_technicalButton->setCheckable(true);
    m_technicalButton->setVisible(false);
    buttonRow->addWidget(m_technicalButton);
    buttonRow->addStretch();

    m_closeBtn = new QPushButton("OK", this);
    m_closeBtn->setObjectName("about_close_button");
    m_closeBtn->setEnabled(false);
    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    buttonRow->addWidget(m_closeBtn);
    layout->addLayout(buttonRow);

    connect(worker, &RescanWorker::outputLine,
            this, &RescanDialog::onOutput);
    connect(worker, &RescanWorker::completed,
            this, &RescanDialog::onCompleted);
    connect(worker, &RescanWorker::error,
            this, &RescanDialog::onError);
}

void RescanDialog::onOutput(QString line) {
    // The final legacy text summary remains available to terminal callers,
    // but this dialog renders the structured result instead.
    if (line.trimmed().startsWith("==========")) return;
    m_statusLabel->setText(line.trimmed());
}

void RescanDialog::onCompleted(RescanReport report) {
    m_statusLabel->setText(
        QString("Rescan completed in %1 ms.")
            .arg(static_cast<qlonglong>(report.scan.total_ms)));
    m_progress->setRange(0, 100);
    m_progress->setValue(100);
    populateReport(report);
    setMinimumSize(780, 460);
    resize(980, kCompletedDialogHeight);
    m_reportContainer->setVisible(true);
    m_closeBtn->setEnabled(true);
    m_closeBtn->setDefault(true);
    m_closeBtn->setFocus();
}

void RescanDialog::populateReport(const RescanReport& report) {
    const ScanResult& scan = report.scan;
    const LibraryRescanComparison& comparison = report.comparison;

    auto* changesHeader = new QHBoxLayout();
    auto* changesHeading = new QLabel("Changes this rescan", m_reportContainer);
    changesHeading->setObjectName("rescan_section_heading");
    changesHeader->addWidget(changesHeading);
    changesHeader->addStretch();
    auto* libraryTotals = new QLabel(
        QString("Library media: %1 before  →  %2 now")
            .arg(static_cast<qulonglong>(comparison.previous_count))
            .arg(static_cast<qulonglong>(comparison.current_count)),
        m_reportContainer);
    libraryTotals->setObjectName("rescan_comparison_total");
    changesHeader->addWidget(libraryTotals);
    m_reportLayout->addLayout(changesHeader);

    auto* changesRow = new QHBoxLayout();
    changesRow->setSpacing(10);
    changesRow->addWidget(make_change_card(
        "Added", comparison.added.size(), "added"), 1);
    changesRow->addWidget(make_change_card(
        "Removed", comparison.removed.size(), "removed"), 1);
    changesRow->addWidget(make_change_card(
        "Updated", comparison.updated.size(), "updated"), 1);
    changesRow->addWidget(make_change_card(
        "Unchanged", comparison.unchanged_count, "unchanged"), 1);
    m_reportLayout->addLayout(changesRow);

    auto* cards = new QHBoxLayout();
    cards->setSpacing(10);

    QFrame* mvsCard = make_card("Neo Geo MVS/AES");
    QGridLayout* mvs = add_metrics_grid(mvsCard);
    add_metric(mvs, 0, "ROM files", count_text(scan.rom_file_count));
    add_metric(mvs, 1, "Parent ROMs", count_text(scan.parent_games));
    add_metric(mvs, 2, "Variants", count_text(scan.variant_count));
    add_metric(mvs, 3, "Homebrew", count_text(scan.homebrew_games));
    add_metric(mvs, 4, "CRC-32 verified",
               count_text(scan.neo_geolith_crc32_verified_files));
    add_metric(mvs, 5, "CRC-32 mismatches",
               count_text(scan.neo_geolith_crc32_mismatch_files),
               scan.neo_geolith_crc32_mismatch_files > 0);
    add_metric(mvs, 6, "Metadata only",
               count_text(scan.neo_metadata_only_files));
    cards->addWidget(mvsCard, 1);

    QFrame* cdCard = make_card("Neo Geo CD");
    QGridLayout* cd = add_metrics_grid(cdCard);
    add_metric(cd, 0, "Images", count_text(scan.cd_image_count));
    add_metric(cd, 1, "Games", count_text(scan.cd_game_count));
    add_metric(cd, 2, "Identified", count_text(scan.cd_identified_games));
    add_metric(cd, 3, "Redump verified",
               count_text(scan.cd_redump_cue_verified_games));
    add_metric(cd, 4, "Redump tracks only",
               count_text(scan.cd_redump_tracks_only_games));
    add_metric(cd, 5, "MAME sets matched",
               count_text(scan.cd_mame_chd_matched_games));
    add_metric(cd, 6, "CHD mismatches",
               count_text(scan.cd_mame_chd_mismatch_games),
               scan.cd_mame_chd_mismatch_games > 0);
    add_metric(cd, 7, "Metadata only",
               count_text(scan.cd_metadata_only_games));
    add_metric(cd, 8, "Unknown", count_text(scan.cd_unknown_games),
               scan.cd_unknown_games > 0);
    cards->addWidget(cdCard, 1);

    QFrame* timingCard = make_card("Scan timing (ms)");
    QGridLayout* timing = add_metrics_grid(timingCard);
    add_metric(timing, 0, "Metadata / indexes",
               milliseconds_text(scan.metadata_and_indexes_ms));
    add_metric(timing, 1, "File discovery",
               milliseconds_text(scan.file_discovery_ms));
    add_metric(timing, 2, "MVS/AES verification",
               milliseconds_text(scan.cartridge_verification_ms));
    add_metric(timing, 3, "Neo Geo CD verification",
               milliseconds_text(scan.cd_verification_ms));
    add_metric(timing, 4, "Database write",
               milliseconds_text(scan.database_build_write_ms));
    add_metric(timing, 5, "Statistics",
               milliseconds_text(scan.statistics_ms));
    add_metric(timing, 6, "Total", milliseconds_text(scan.total_ms));
    cards->addWidget(timingCard, 1);
    m_reportLayout->addLayout(cards, 1);

    QStringList changeLines;
    append_changes(changeLines, "Added", comparison.added);
    append_changes(changeLines, "Removed", comparison.removed);
    append_changes(changeLines, "Updated", comparison.updated);
    if (!changeLines.isEmpty()) {
        auto* details = new QTextEdit(m_reportContainer);
        details->setReadOnly(true);
        details->setPlainText(changeLines.join('\n'));
        details->setMaximumHeight(150);
        details->setVisible(false);
        m_reportLayout->addWidget(details);
        m_changesDetails = details;
        m_changesButton->setVisible(true);
        connect(m_changesButton, &QPushButton::toggled, details,
                [this, details](bool visible) {
                    details->setVisible(visible);
                    m_changesButton->setText(
                        visible ? "Hide Changes" : "Show Changes");
                    schedule_content_resize(this);
                });
    }

    auto* technical = new QFrame(m_reportContainer);
    technical->setObjectName("rescan_summary_card");
    auto* technicalLayout = new QHBoxLayout(technical);
    technicalLayout->setContentsMargins(16, 12, 16, 12);
    technicalLayout->setSpacing(28);

    auto* mvsCache = new QVBoxLayout();
    auto* mvsCacheHeading = new QLabel("MVS/AES CRC-32 cache", technical);
    mvsCacheHeading->setObjectName("rescan_card_heading");
    mvsCache->addWidget(mvsCacheHeading);
    auto* mvsCacheGrid = new QGridLayout();
    mvsCacheGrid->setColumnStretch(0, 1);
    add_metric(mvsCacheGrid, 0, "Cache hits",
               count_text(scan.neo_geolith_crc32_cache_hits));
    add_metric(mvsCacheGrid, 1, "Calculated",
               count_text(scan.neo_geolith_crc32_calculated_files));
    add_metric(mvsCacheGrid, 2, "Cache rows",
               count_text(scan.neo_geolith_crc32_cache_entries));
    add_metric(mvsCacheGrid, 3, "Pruned",
               count_text(scan.neo_geolith_crc32_cache_pruned));
    mvsCache->addLayout(mvsCacheGrid);
    technicalLayout->addLayout(mvsCache, 1);

    auto* cdCache = new QVBoxLayout();
    auto* cdCacheHeading = new QLabel("Neo Geo CD SHA-1 cache", technical);
    cdCacheHeading->setObjectName("rescan_card_heading");
    cdCache->addWidget(cdCacheHeading);
    auto* cdCacheGrid = new QGridLayout();
    cdCacheGrid->setColumnStretch(0, 1);
    add_metric(cdCacheGrid, 0, "Cache hits",
               count_text(scan.cd_redump_sha1_cache_hits));
    add_metric(cdCacheGrid, 1, "Calculated",
               count_text(scan.cd_redump_sha1_calculated_files));
    add_metric(cdCacheGrid, 2, "Cache rows",
               count_text(scan.cd_redump_sha1_cache_entries));
    add_metric(cdCacheGrid, 3, "Pruned",
               count_text(scan.cd_redump_sha1_cache_pruned));
    cdCache->addLayout(cdCacheGrid);
    technicalLayout->addLayout(cdCache, 1);

    technical->setVisible(false);
    m_reportLayout->addWidget(technical);
    m_technicalDetails = technical;
    m_technicalButton->setVisible(true);
    connect(m_technicalButton, &QPushButton::toggled, technical,
            [this, technical](bool visible) {
                technical->setVisible(visible);
                m_technicalButton->setText(
                    visible ? "Hide Technical Details" : "Technical Details");
                schedule_content_resize(this);
            });
}

void RescanDialog::onError(QString msg) {
    m_statusLabel->setText("Rescan failed.");
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    m_errorText->setPlainText(msg);
    m_errorText->setVisible(true);
    m_closeBtn->setEnabled(true);
    m_closeBtn->setDefault(true);
}

} // namespace goliath
