#include "ui/bios_verification_dialog.hpp"

#include "common/qt_path.hpp"
#include "ui/widgets/title_bar.hpp"

#include <QFontDatabase>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLayout>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QStringList>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

namespace goliath {

namespace {

constexpr int kCompactHeight = 610;
constexpr int kDetailedHeight = 810;

QString countText(std::size_t value) {
    return QString::number(static_cast<qulonglong>(value));
}

QString archiveStatusText(const BiosArchiveVerification& archive) {
    switch (archive.status) {
    case BiosArchiveStatus::Ready:
        if (archive.optional_missing_members > 0) {
            return archive.optional_missing_members == 1
                ? "Ready · 1 optional component unavailable"
                : QString("Ready · %1 optional components unavailable")
                      .arg(countText(archive.optional_missing_members));
        }
        return "Ready";
    case BiosArchiveStatus::MissingRequired:
        return "Required archive missing";
    case BiosArchiveStatus::MissingOptional:
        return "Optional archive not installed";
    case BiosArchiveStatus::InvalidArchive:
        return "Invalid ZIP archive";
    case BiosArchiveStatus::MemberProblems:
        return archive.problems == 1
            ? "1 member problem"
            : QString("%1 member problems").arg(archive.problems);
    }
    return "Unknown";
}

QString archiveStatusProperty(BiosArchiveStatus status) {
    switch (status) {
    case BiosArchiveStatus::Ready:
        return "ready";
    case BiosArchiveStatus::MissingOptional:
        return "optional";
    case BiosArchiveStatus::MissingRequired:
    case BiosArchiveStatus::InvalidArchive:
    case BiosArchiveStatus::MemberProblems:
        return "problem";
    }
    return "problem";
}

QString memberStatusText(BiosMemberStatus status) {
    switch (status) {
    case BiosMemberStatus::Verified:
        return "OK";
    case BiosMemberStatus::Present:
        return "FOUND";
    case BiosMemberStatus::Missing:
        return "MISSING";
    case BiosMemberStatus::MissingOptional:
        return "OPTIONAL ABSENT";
    case BiosMemberStatus::ReadError:
        return "READ ERROR";
    case BiosMemberStatus::CrcMismatch:
        return "BAD CRC";
    }
    return "UNKNOWN";
}

QString memberStatusProperty(BiosMemberStatus status) {
    switch (status) {
    case BiosMemberStatus::Verified:
    case BiosMemberStatus::Present:
        return "ready";
    case BiosMemberStatus::MissingOptional:
        return "optional";
    case BiosMemberStatus::Missing:
    case BiosMemberStatus::ReadError:
    case BiosMemberStatus::CrcMismatch:
        return "problem";
    }
    return "problem";
}

QString memberCrcText(const BiosMemberVerification& member) {
    const QString expected = member.expected_crc.has_value()
        ? QString::fromStdString(*member.expected_crc)
        : QString();
    const QString actual = member.actual_crc.has_value()
        ? QString::fromStdString(*member.actual_crc)
        : QString();

    if (!expected.isEmpty() && !actual.isEmpty()) {
        if (expected == actual) return QString("CRC %1").arg(actual);
        return QString("Expected %1 · Found %2").arg(expected, actual);
    }
    if (!expected.isEmpty()) return QString("Expected %1").arg(expected);
    if (!actual.isEmpty()) return QString("CRC %1").arg(actual);
    return {};
}

QString technicalDetailsText(const BiosVerifyResult& result) {
    QStringList lines;
    for (const BiosArchiveVerification& archive : result.archives) {
        if (!lines.isEmpty()) lines.append(QString());
        lines.append(QString("%1 — %2 [%3]")
                         .arg(QString::fromStdString(archive.zip_name),
                              QString::fromStdString(archive.purpose),
                              archive.required ? "required" : "optional"));
        lines.append(QString("  Status: %1").arg(archiveStatusText(archive)));

        for (const BiosMemberVerification& member : archive.members) {
            lines.append(QString("  [%1] %2 — %3")
                             .arg(memberStatusText(member.status),
                                  QString::fromStdString(member.member),
                                  QString::fromStdString(member.description) +
                                      (member.required ? QString()
                                                       : " [optional]")));
            if (member.expected_crc.has_value()) {
                lines.append(QString("       Expected CRC-32: %1")
                                 .arg(QString::fromStdString(
                                     *member.expected_crc)));
            } else if (member.actual_crc.has_value()) {
                lines.append("       Expected CRC-32: presence check only");
            }
            if (member.actual_crc.has_value()) {
                lines.append(QString("       Calculated CRC-32: %1")
                                 .arg(QString::fromStdString(
                                     *member.actual_crc)));
            }
        }
    }
    return lines.join('\n');
}

QFrame* makeMetricCard(const QString& value, const QString& label,
                       const QString& kind) {
    auto* card = new QFrame();
    card->setObjectName("bios_metric_card");
    card->setProperty("biosMetric", kind);
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(14, 9, 14, 9);
    layout->setSpacing(0);

    auto* amount = new QLabel(value, card);
    amount->setObjectName("bios_metric_value");
    amount->setAlignment(Qt::AlignCenter);
    layout->addWidget(amount);

    auto* name = new QLabel(label, card);
    name->setObjectName("bios_metric_label");
    name->setAlignment(Qt::AlignCenter);
    layout->addWidget(name);
    return card;
}

class ArchiveCard final : public QFrame {
public:
    explicit ArchiveCard(const BiosArchiveVerification& archive,
                         QWidget* parent = nullptr)
        : QFrame(parent) {
        setObjectName("bios_archive_row");
        setProperty("biosStatus", archiveStatusProperty(archive.status));
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::StrongFocus);
        setToolTip("Click to view checks");
        setAccessibleName(
            QString("%1, %2")
                .arg(QString::fromStdString(archive.zip_name),
                     archiveStatusText(archive)));

        auto* cardLayout = new QVBoxLayout(this);
        cardLayout->setContentsMargins(0, 0, 0, 0);
        cardLayout->setSpacing(0);

        m_header = new QWidget(this);
        m_header->setObjectName("bios_archive_header");
        m_header->setAttribute(Qt::WA_TransparentForMouseEvents);
        auto* headerLayout = new QGridLayout(m_header);
        headerLayout->setContentsMargins(14, 9, 14, 9);
        headerLayout->setHorizontalSpacing(14);
        headerLayout->setVerticalSpacing(2);
        headerLayout->setColumnStretch(1, 1);

        auto* name = new QLabel(
            QString::fromStdString(archive.zip_name), m_header);
        name->setObjectName("bios_archive_name");
        headerLayout->addWidget(name, 0, 0, 2, 1);

        auto* purpose = new QLabel(
            QString::fromStdString(archive.purpose), m_header);
        purpose->setObjectName("bios_archive_purpose");
        headerLayout->addWidget(purpose, 0, 1);

        auto* requirement = new QLabel(
            archive.required ? "Required" : "Optional", m_header);
        requirement->setObjectName("bios_archive_requirement");
        requirement->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        headerLayout->addWidget(requirement, 0, 2);

        auto* status = new QLabel(archiveStatusText(archive), m_header);
        status->setObjectName("bios_archive_status");
        status->setProperty(
            "biosStatus", archiveStatusProperty(archive.status));
        status->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        headerLayout->addWidget(status, 1, 1, 1, 2);

        m_chevron = new QLabel(QString::fromUtf8("›"), m_header);
        m_chevron->setObjectName("bios_archive_chevron");
        m_chevron->setAlignment(Qt::AlignCenter);
        headerLayout->addWidget(m_chevron, 0, 3, 2, 1);
        cardLayout->addWidget(m_header);

        m_details = new QFrame(this);
        m_details->setObjectName("bios_archive_details");
        auto* detailsLayout = new QVBoxLayout(m_details);
        detailsLayout->setContentsMargins(14, 8, 14, 10);
        detailsLayout->setSpacing(5);

        if (archive.members.empty()) {
            auto* unavailable = new QLabel(
                archive.status == BiosArchiveStatus::MissingOptional
                    ? "Install this optional archive to enable its BIOS modes."
                    : "No member checks are available for this archive.",
                m_details);
            unavailable->setObjectName("bios_member_description");
            detailsLayout->addWidget(unavailable);
        } else {
            for (const BiosMemberVerification& member : archive.members) {
                auto* memberRow = new QWidget(m_details);
                memberRow->setObjectName("bios_member_row");
                auto* memberLayout = new QGridLayout(memberRow);
                memberLayout->setContentsMargins(0, 0, 0, 0);
                memberLayout->setHorizontalSpacing(9);
                memberLayout->setVerticalSpacing(0);
                memberLayout->setColumnStretch(2, 1);

                auto* memberStatus = new QLabel(
                    memberStatusText(member.status), memberRow);
                memberStatus->setObjectName("bios_member_status");
                memberStatus->setProperty(
                    "biosStatus", memberStatusProperty(member.status));
                memberLayout->addWidget(memberStatus, 0, 0);

                auto* memberName = new QLabel(
                    QString::fromStdString(member.member), memberRow);
                memberName->setObjectName("bios_member_name");
                memberName->setTextInteractionFlags(Qt::TextSelectableByMouse);
                memberLayout->addWidget(memberName, 0, 1);

                QString description =
                    QString::fromStdString(member.description);
                if (!member.required) description += " (optional)";
                auto* memberDescription = new QLabel(description, memberRow);
                memberDescription->setObjectName("bios_member_description");
                memberDescription->setTextInteractionFlags(
                    Qt::TextSelectableByMouse);
                memberLayout->addWidget(memberDescription, 0, 2);

                auto* crc = new QLabel(memberCrcText(member), memberRow);
                crc->setObjectName("bios_member_crc");
                crc->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                crc->setTextInteractionFlags(Qt::TextSelectableByMouse);
                memberLayout->addWidget(crc, 0, 3);

                detailsLayout->addWidget(memberRow);
            }
        }
        m_details->setVisible(false);
        cardLayout->addWidget(m_details);
    }

    void setActivationHandler(std::function<void(ArchiveCard*)> handler) {
        m_activationHandler = std::move(handler);
    }

    void setExpanded(bool expanded) {
        m_expanded = expanded;
        m_details->setVisible(expanded);
        m_chevron->setText(QString::fromUtf8(expanded ? "⌄" : "›"));
        setToolTip(expanded ? "Click to hide checks" : "Click to view checks");
        updateGeometry();
    }

    bool isExpanded() const { return m_expanded; }

protected:
    void mouseReleaseEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton &&
            m_header->geometry().contains(event->position().toPoint())) {
            activate();
            event->accept();
            return;
        }
        QFrame::mouseReleaseEvent(event);
    }

    void keyPressEvent(QKeyEvent* event) override {
        if (event->key() == Qt::Key_Return ||
            event->key() == Qt::Key_Enter ||
            event->key() == Qt::Key_Space) {
            activate();
            event->accept();
            return;
        }
        QFrame::keyPressEvent(event);
    }

private:
    void activate() {
        if (m_activationHandler) m_activationHandler(this);
    }

    QWidget* m_header = nullptr;
    QFrame* m_details = nullptr;
    QLabel* m_chevron = nullptr;
    std::function<void(ArchiveCard*)> m_activationHandler;
    bool m_expanded = false;
};

} // namespace

BiosVerificationDialog::BiosVerificationDialog(
    const BiosVerifyResult& result,
    const std::filesystem::path& biosDir,
    QWidget* parent)
    : QDialog(parent) {
    setModal(true);
    setMinimumSize(720, 520);
    resize(900, kCompactHeight);

    QVBoxLayout* layout = nullptr;
    setupFramelessDialog(this, "Verify BIOS", &layout, true, false);

    auto* summary = new QFrame(this);
    summary->setObjectName("bios_summary_card");
    summary->setProperty(
        "biosStatus",
        result.problems == 0 ? QStringLiteral("ready")
                             : QStringLiteral("problem"));
    auto* summaryLayout = new QVBoxLayout(summary);
    summaryLayout->setContentsMargins(16, 12, 16, 12);
    summaryLayout->setSpacing(3);

    const QString headingText = result.problems == 0
        ? QStringLiteral("BIOS verification completed successfully")
        : QString("BIOS verification found %1 problem(s)")
              .arg(result.problems);
    auto* heading = new QLabel(headingText, summary);
    heading->setObjectName("bios_status_heading");
    heading->setProperty(
        "biosStatus",
        result.problems == 0 ? QStringLiteral("ready")
                             : QStringLiteral("problem"));
    summaryLayout->addWidget(heading);

    auto* path = new QLabel(
        QString("Folder: %1").arg(path_to_qstring(biosDir)), summary);
    path->setObjectName("bios_folder_path");
    path->setTextInteractionFlags(Qt::TextSelectableByMouse);
    path->setToolTip(path_to_qstring(biosDir));
    summaryLayout->addWidget(path);
    layout->addWidget(summary);

    auto* metrics = new QHBoxLayout();
    metrics->setSpacing(10);
    metrics->addWidget(makeMetricCard(
        countText(result.archives.size()), "Archives", "neutral"), 1);
    metrics->addWidget(makeMetricCard(
        countText(result.ready_archives), "Ready", "ready"), 1);
    metrics->addWidget(makeMetricCard(
        QString::number(result.problems), "Problems", "problem"), 1);
    metrics->addWidget(makeMetricCard(
        countText(result.optional_missing_archives +
                  result.optional_missing_members),
        "Optional unavailable", "optional"), 1);
    layout->addLayout(metrics);

    auto* listHeading = new QLabel("BIOS archives", this);
    listHeading->setObjectName("bios_section_heading");
    layout->addWidget(listHeading);

    auto* archiveScroll = new QScrollArea(this);
    archiveScroll->setObjectName("bios_archive_scroll");
    archiveScroll->setWidgetResizable(true);
    archiveScroll->setFrameShape(QFrame::NoFrame);
    archiveScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    archiveScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    archiveScroll->setMinimumHeight(250);

    auto* archiveContainer = new QWidget(archiveScroll);
    archiveContainer->setObjectName("bios_archive_scroll_contents");
    auto* archiveList = new QVBoxLayout(archiveContainer);
    archiveList->setContentsMargins(0, 0, 4, 0);
    archiveList->setSpacing(6);
    std::vector<ArchiveCard*> archiveCards;
    archiveCards.reserve(result.archives.size());
    for (const BiosArchiveVerification& archive : result.archives) {
        auto* card = new ArchiveCard(archive, archiveContainer);
        archiveCards.push_back(card);
        archiveList->addWidget(card);
    }
    archiveList->addStretch();
    archiveScroll->setWidget(archiveContainer);
    layout->addWidget(archiveScroll, 1);

    for (ArchiveCard* card : archiveCards) {
        card->setActivationHandler(
            [archiveCards](ArchiveCard* activated) {
                const bool expand = !activated->isExpanded();
                for (ArchiveCard* other : archiveCards)
                    other->setExpanded(false);
                activated->setExpanded(expand);
            });
    }

    m_details = new QTextEdit(this);
    m_details->setObjectName("bios_technical_details");
    m_details->setReadOnly(true);
    m_details->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_details->setPlainText(technicalDetailsText(result));
    m_details->setMinimumHeight(190);
    m_details->setMaximumHeight(230);
    m_details->setVisible(false);
    layout->addWidget(m_details, 1);

    auto* buttonRow = new QHBoxLayout();
    m_detailsButton = new QPushButton("Technical Details", this);
    m_detailsButton->setCheckable(true);
    buttonRow->addWidget(m_detailsButton);
    buttonRow->addStretch();

    auto* closeButton = new QPushButton("OK", this);
    closeButton->setObjectName("about_close_button");
    closeButton->setDefault(true);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    buttonRow->addWidget(closeButton);
    layout->addLayout(buttonRow);

    connect(m_detailsButton, &QPushButton::toggled, this,
            [this](bool visible) {
                m_details->setVisible(visible);
                m_detailsButton->setText(
                    visible ? "Hide Technical Details" : "Technical Details");
                scheduleContentResize(visible);
            });
}

void BiosVerificationDialog::scheduleContentResize(bool detailsVisible) {
    QTimer::singleShot(0, this, [this, detailsVisible]() {
        if (QLayout* dialogLayout = layout()) {
            dialogLayout->invalidate();
            dialogLayout->activate();
        }
        const int targetHeight = detailsVisible
            ? std::max(kDetailedHeight, sizeHint().height())
            : kCompactHeight;
        if (height() != targetHeight) resize(width(), targetHeight);
    });
}

} // namespace goliath
