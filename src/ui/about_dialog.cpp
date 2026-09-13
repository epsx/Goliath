#include "ui/about_dialog.hpp"

#include "common/project_legal.hpp"
#include "ui/widgets/title_bar.hpp"

#include <QDesktopServices>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSize>
#include <QUrl>
#include <QVBoxLayout>

#ifndef GOLIATH_VERSION
#define GOLIATH_VERSION "development"
#endif

#ifndef GOLIATH_BUILD_DATE
#define GOLIATH_BUILD_DATE "unknown"
#endif

#ifndef GOLIATH_BUILD_REVISION
#define GOLIATH_BUILD_REVISION "source-archive"
#endif

#ifndef GOLIATH_PROJECT_URL
#define GOLIATH_PROJECT_URL ""
#endif

#ifndef GOLIATH_LICENSE_URL
#define GOLIATH_LICENSE_URL "https://www.gnu.org/licenses/gpl-3.0.html"
#endif

namespace goliath {

namespace {

QLabel* makeWrappedLabel(const QString& text, const char* objectName,
                         QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setObjectName(objectName);
    label->setWordWrap(true);
    return label;
}

QPushButton* makeExternalLinkButton(const QString& label,
                                    const QString& url,
                                    const QString& unavailableReason,
                                    QWidget* parent) {
    auto* button = new QPushButton(label, parent);
    button->setObjectName("about_link_button");
    button->setMinimumWidth(132);

    if (url.isEmpty()) {
        button->setEnabled(false);
        button->setToolTip(unavailableReason);
        return button;
    }

    button->setCursor(Qt::PointingHandCursor);
    button->setToolTip(QString("Open %1 in the default browser").arg(url));
    QObject::connect(button, &QPushButton::clicked, parent,
                     [parent, label, url]() {
        if (QDesktopServices::openUrl(QUrl(url))) return;
        QMessageBox::warning(
            parent, "Open Link",
            QString("Goliath could not open the %1 link in the default "
                    "browser.\n\n%2")
                .arg(label, url));
    });
    return button;
}

} // namespace

AboutDialog::AboutDialog(QWidget* parent)
    : QDialog(parent) {
    // Give the complete project and license summary room at the normal
    // desktop DPI while retaining scrolling as a fallback on smaller screens.
    resize(960, 680);
    setMinimumSize(720, 520);
    setModal(true);
    setSizeGripEnabled(true);

    QVBoxLayout* rootLayout = nullptr;
    setupFramelessDialog(this, "About Goliath", &rootLayout, true, false);

    auto* cardsScroll = new QScrollArea(this);
    cardsScroll->setWidgetResizable(true);
    cardsScroll->setFrameShape(QFrame::NoFrame);
    auto* cardsContainer = new QWidget(cardsScroll);
    auto* cardsLayout = new QHBoxLayout(cardsContainer);
    cardsLayout->setSpacing(12);
    cardsScroll->setWidget(cardsContainer);
    rootLayout->addWidget(cardsScroll, 1);

    auto* identityCard = new QFrame(cardsContainer);
    identityCard->setObjectName("about_identity_card");
    identityCard->setMinimumWidth(246);
    identityCard->setMaximumWidth(272);
    auto* identityLayout = new QVBoxLayout(identityCard);
    identityLayout->setContentsMargins(24, 26, 24, 24);
    identityLayout->setSpacing(9);

    auto* iconLabel = new QLabel(identityCard);
    iconLabel->setObjectName("about_logo");
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setAccessibleName("Goliath application icon");
    iconLabel->setPixmap(
        QIcon(":/icons/goliath-qt.ico").pixmap(QSize(164, 164)));
    identityLayout->addWidget(iconLabel, 0, Qt::AlignHCenter);
    identityLayout->addSpacing(8);

    auto* productName = new QLabel("Goliath", identityCard);
    productName->setObjectName("about_product_name");
    productName->setAlignment(Qt::AlignCenter);
    identityLayout->addWidget(productName);

    auto* tagline = new QLabel(QString::fromUtf8(
        "Launcher  \xC2\xB7  Library  \xC2\xB7  Manager"), identityCard);
    tagline->setObjectName("about_tagline");
    tagline->setAlignment(Qt::AlignCenter);
    identityLayout->addWidget(tagline);
    identityLayout->addStretch();

    auto* version = new QLabel(
        QString("Development version %1")
            .arg(QString::fromUtf8(GOLIATH_VERSION)), identityCard);
    version->setObjectName("about_version");
    version->setAlignment(Qt::AlignCenter);
    version->setTextInteractionFlags(Qt::TextSelectableByMouse);
    identityLayout->addWidget(version);

    auto* build = new QLabel(
        QString("Build: %1\nDate: %2")
            .arg(QString::fromUtf8(GOLIATH_BUILD_REVISION),
                 QString::fromUtf8(GOLIATH_BUILD_DATE)),
        identityCard);
    build->setObjectName("about_build");
    build->setAlignment(Qt::AlignCenter);
    build->setWordWrap(true);
    build->setTextInteractionFlags(Qt::TextSelectableByMouse);
    identityLayout->addWidget(build);

    cardsLayout->addWidget(identityCard);

    auto* informationCard = new QFrame(cardsContainer);
    informationCard->setObjectName("about_information_card");
    auto* informationLayout = new QVBoxLayout(informationCard);
    informationLayout->setContentsMargins(24, 22, 24, 22);
    informationLayout->setSpacing(9);

    auto* heading = makeWrappedLabel(
        "A frontend for Geolith a Neo Geo / CD Emulator",
        "about_heading", informationCard);
    informationLayout->addWidget(heading);

    auto* description = makeWrappedLabel(
        "Goliath organizes and launches Neo Geo MVS/AES and Neo Geo CD "
        "media through the Jolly Good Reference Frontend and Geolith. "
        "It keeps the library, launch profiles, input mappings, diagnostics, "
        "and supporting tools together without modifying the emulation "
        "projects.",
        "about_body", informationCard);
    informationLayout->addWidget(description);

    auto* upstreamCredit = makeWrappedLabel(
        "Thanks to the Jolly Good Emulation author for "
        "the Jolly Good API (JG), JGRF, and Geolith.",
        "about_body", informationCard);
    informationLayout->addWidget(upstreamCredit);

    auto* upstreamLinks = new QGridLayout();
    upstreamLinks->setHorizontalSpacing(8);
    upstreamLinks->setVerticalSpacing(8);
    upstreamLinks->addWidget(makeExternalLinkButton(
        "Jolly Good Emulation", "https://jgemu.gitlab.io/", {}, informationCard), 0, 0);
    upstreamLinks->addWidget(makeExternalLinkButton(
        "JG API", "https://gitlab.com/jgemu/jg", {}, informationCard),
        0, 1);
    upstreamLinks->addWidget(makeExternalLinkButton(
        "JGRF", "https://gitlab.com/jgemu/jgrf", {}, informationCard), 1, 0);
    upstreamLinks->addWidget(makeExternalLinkButton(
        "Geolith", "https://gitlab.com/jgemu/geolith", {}, informationCard),
        1, 1);
    informationLayout->addLayout(upstreamLinks);

    auto* projectHeading = new QLabel("Goliath project", informationCard);
    projectHeading->setObjectName("about_section_heading");
    informationLayout->addWidget(projectHeading);

    auto* projectLinks = new QGridLayout();
    projectLinks->setHorizontalSpacing(8);
    projectLinks->setVerticalSpacing(8);
    const QString configuredProjectUrl = QString::fromUtf8(GOLIATH_PROJECT_URL);
    const QString projectUrl = configuredProjectUrl.isEmpty()
        ? QString::fromUtf8(kProjectUrl.data())
        : configuredProjectUrl;
    const QString configuredLicenseUrl = QString::fromUtf8(GOLIATH_LICENSE_URL);
    const QString licenseUrl = configuredLicenseUrl.isEmpty()
        ? QString::fromUtf8(kProjectLicenseUrl.data())
        : configuredLicenseUrl;
    projectLinks->addWidget(makeExternalLinkButton(
        "Goliath GitHub", projectUrl,
        "The Goliath repository URL has not been published or configured yet.",
        informationCard), 0, 0);
    projectLinks->addWidget(makeExternalLinkButton(
        "Goliath License", licenseUrl,
        "The Goliath license URL is not configured for this build.",
        informationCard), 0, 1);
    informationLayout->addLayout(projectLinks);

    auto* legalSummary = makeWrappedLabel(
        QString("%1. Goliath is free software licensed under %2 (%3): you "
                "may redistribute and modify it under those terms. It comes "
                "with absolutely no warranty. See LICENSE beside the "
                "application or use Goliath License above to view the terms.")
            .arg(QString::fromUtf8(kProjectCopyright.data()),
                 QString::fromUtf8(kProjectLicenseName.data()),
                 QString::fromUtf8(kProjectLicenseSpdx.data())),
        "about_body", informationCard);
    informationLayout->addWidget(legalSummary);

    auto* thirdPartySummary = makeWrappedLabel(
        QString("Qt is used dynamically under LGPL-3.0-only. Third-party "
                "components retain their own licenses; see %1 in the "
                "distribution.")
            .arg(QString::fromUtf8(kThirdPartyNoticesFile.data())),
        "about_body", informationCard);
    informationLayout->addWidget(thirdPartySummary);
    informationLayout->addStretch();

    auto* credit = makeWrappedLabel(
        QString::fromUtf8(
            "Created by epsx  \xC2\xB7  Developed with LLM (A.I.) assistance"),
        "about_credit", informationCard);
    credit->setAlignment(Qt::AlignCenter);
    informationLayout->addWidget(credit);

    cardsLayout->addWidget(informationCard, 1);

    auto* actionRow = new QHBoxLayout();
    actionRow->setSpacing(8);
    actionRow->addStretch();

    auto* qtWebsite = makeExternalLinkButton(
        "Qt Website", "https://www.qt.io/", {}, this);
    actionRow->addWidget(qtWebsite);

    auto* aboutQt = new QPushButton("About Qt", this);
    aboutQt->setObjectName("about_link_button");
    connect(aboutQt, &QPushButton::clicked, this,
            [this]() { QMessageBox::aboutQt(this, "About Qt"); });
    actionRow->addWidget(aboutQt);

    auto* closeButton = new QPushButton("Close", this);
    closeButton->setObjectName("about_close_button");
    closeButton->setDefault(true);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    actionRow->addWidget(closeButton);

    rootLayout->addLayout(actionRow);
}

} // namespace goliath
