#include "ui/update_check.hpp"

#include "ui/widgets/title_bar.hpp"
#include "update/release_feed.hpp"

#include <QDesktopServices>
#include <QDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProgressBar>
#include <QPushButton>
#include <QUrl>
#include <QVariant>
#include <QVBoxLayout>

#ifndef GOLIATH_VERSION
#define GOLIATH_VERSION "development"
#endif

#ifndef GOLIATH_RELEASES_API_URL
#define GOLIATH_RELEASES_API_URL ""
#endif

namespace goliath {

namespace {

constexpr qint64 kMaximumReleaseResponseBytes = 1024 * 1024;

QString display_release_date(const std::string& publishedAt) {
    const QString value = QString::fromStdString(publishedAt);
    return value.size() >= 10 ? value.left(10) : value;
}

bool is_trusted_release_url(const QUrl& url) {
    return url.isValid() && url.scheme() == "https" &&
           url.host().compare("github.com", Qt::CaseInsensitive) == 0;
}

void show_update_notice(QWidget* parent, const QString& heading,
                        const QString& detail) {
    QDialog dialog(parent);
    dialog.resize(520, 230);
    dialog.setMinimumWidth(440);
    dialog.setModal(true);
    dialog.setSizeGripEnabled(false);

    QVBoxLayout* layout = nullptr;
    setupFramelessDialog(&dialog, "Goliath Update", &layout, false, false);

    auto* card = new QFrame(&dialog);
    card->setObjectName("about_information_card");
    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(22, 20, 22, 20);
    cardLayout->setSpacing(10);

    auto* headingLabel = new QLabel(heading, card);
    headingLabel->setObjectName("about_heading");
    headingLabel->setWordWrap(true);
    cardLayout->addWidget(headingLabel);

    auto* detailLabel = new QLabel(detail, card);
    detailLabel->setObjectName("about_body");
    detailLabel->setWordWrap(true);
    detailLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    cardLayout->addWidget(detailLabel);
    layout->addWidget(card);

    auto* buttonRow = new QHBoxLayout();
    buttonRow->addStretch();
    auto* closeButton = new QPushButton("Close", &dialog);
    closeButton->setObjectName("about_close_button");
    closeButton->setDefault(true);
    QObject::connect(closeButton, &QPushButton::clicked,
                     &dialog, &QDialog::accept);
    buttonRow->addWidget(closeButton);
    layout->addLayout(buttonRow);

    dialog.exec();
}

void open_release_page(QWidget* parent, const std::string& pageUrl) {
    const QUrl url(QString::fromStdString(pageUrl));
    if (!is_trusted_release_url(url)) {
        show_update_notice(parent, "The release page could not be opened.",
                           "GitHub returned an invalid release URL.");
        return;
    }
    if (!QDesktopServices::openUrl(url)) {
        show_update_notice(
            parent, "The release page could not be opened.",
            "Goliath could not open it in the default browser.");
    }
}

void show_release_result(QWidget* parent, const PublishedRelease& release,
                         const std::optional<ReleaseVersion>& current) {
    const QString currentText = QString::fromUtf8(GOLIATH_VERSION);
    const QString tag = QString::fromStdString(release.tag);
    const QString name = QString::fromStdString(release.name);
    const QString date = display_release_date(release.published_at);
    const QUrl releaseUrl(QString::fromStdString(release.page_url));
    const bool trustedUrl = is_trusted_release_url(releaseUrl);
    const bool upToDate = current &&
        compare_release_versions(release.version, *current) <= 0;

    QDialog dialog(parent);
    dialog.resize(560, 290);
    dialog.setMinimumWidth(460);
    dialog.setModal(true);
    dialog.setSizeGripEnabled(false);

    QVBoxLayout* layout = nullptr;
    setupFramelessDialog(&dialog, "Goliath Update", &layout, false, false);

    auto* card = new QFrame(&dialog);
    card->setObjectName("about_information_card");
    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(22, 20, 22, 20);
    cardLayout->setSpacing(10);

    auto* heading = new QLabel(
        upToDate
            ? "Goliath is up to date."
            : current
                ? "A newer Goliath release is available."
                : "The latest published Goliath release was found.",
        card);
    heading->setObjectName("about_heading");
    heading->setWordWrap(true);
    cardLayout->addWidget(heading);

    const QString releaseText = trustedUrl
        ? QString("<a href=\"%1\" style=\"color:inherit; "
                  "text-decoration:underline\">%2</a>")
              .arg(releaseUrl.toString(QUrl::FullyEncoded).toHtmlEscaped(),
                   tag.toHtmlEscaped())
        : tag.toHtmlEscaped();
    QString detail = QString("Installed: <b>%1</b><br>"
                             "%2: <b>%3</b>")
                         .arg(currentText.toHtmlEscaped(),
                              upToDate ? "Latest published release"
                                       : "Available",
                              releaseText);
    if (!name.isEmpty() && name != tag) {
        detail += QString("<br>Release: %1").arg(name.toHtmlEscaped());
    }
    if (!date.isEmpty()) {
        detail += QString("<br>Published: %1").arg(date.toHtmlEscaped());
    }

    auto* detailLabel = new QLabel(detail, card);
    detailLabel->setObjectName("about_body");
    detailLabel->setWordWrap(true);
    detailLabel->setTextFormat(Qt::RichText);
    detailLabel->setTextInteractionFlags(
        trustedUrl ? Qt::TextBrowserInteraction : Qt::TextSelectableByMouse);
    detailLabel->setOpenExternalLinks(false);
    if (trustedUrl) {
        QObject::connect(detailLabel, &QLabel::linkActivated, &dialog,
                         [&dialog, pageUrl = release.page_url](const QString&) {
            open_release_page(&dialog, pageUrl);
        });
    }
    cardLayout->addWidget(detailLabel);
    layout->addWidget(card);

    auto* buttonRow = new QHBoxLayout();
    buttonRow->addStretch();
    if (trustedUrl) {
        auto* openButton = new QPushButton("Open Release Page", &dialog);
        openButton->setObjectName("about_link_button");
        QObject::connect(openButton, &QPushButton::clicked, &dialog,
                         [&dialog, pageUrl = release.page_url]() {
            open_release_page(&dialog, pageUrl);
        });
        buttonRow->addWidget(openButton);
    }

    auto* closeButton = new QPushButton(upToDate ? "Close" : "Later", &dialog);
    closeButton->setObjectName("about_close_button");
    closeButton->setDefault(true);
    QObject::connect(closeButton, &QPushButton::clicked,
                     &dialog, &QDialog::accept);
    buttonRow->addWidget(closeButton);
    layout->addLayout(buttonRow);

    dialog.exec();
}

} // namespace

void checkForUpdates(QWidget* parent) {
    const QUrl apiUrl(QString::fromUtf8(GOLIATH_RELEASES_API_URL));
    if (!apiUrl.isValid() || apiUrl.scheme() != "https" ||
        apiUrl.host().compare("api.github.com", Qt::CaseInsensitive) != 0) {
        show_update_notice(parent, "Update check unavailable.",
                           "The Goliath release service is not configured.");
        return;
    }

    QDialog progress(parent);
    progress.resize(440, 170);
    progress.setModal(true);
    progress.setSizeGripEnabled(false);

    QVBoxLayout* layout = nullptr;
    setupFramelessDialog(
        &progress, "Check for Updates", &layout, false, false);

    auto* status = new QLabel("Checking published Goliath releases...", &progress);
    status->setObjectName("about_body");
    status->setWordWrap(true);
    layout->addWidget(status);

    auto* activity = new QProgressBar(&progress);
    activity->setRange(0, 0);
    layout->addWidget(activity);

    auto* buttonRow = new QHBoxLayout();
    buttonRow->addStretch();
    auto* cancelButton = new QPushButton("Cancel", &progress);
    cancelButton->setObjectName("about_link_button");
    buttonRow->addWidget(cancelButton);
    layout->addLayout(buttonRow);

    QNetworkAccessManager network(&progress);
    QNetworkRequest request(apiUrl);
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
    request.setRawHeader(
        "User-Agent", QByteArray("Goliath/") + QByteArray(GOLIATH_VERSION));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(15000);

    QNetworkReply* reply = network.get(request);
    bool canceled = false;
    QObject::connect(reply, &QNetworkReply::finished,
                     &progress, &QDialog::accept);
    QObject::connect(cancelButton, &QPushButton::clicked, &progress, [&]() {
        canceled = true;
        reply->abort();
        progress.reject();
    });

    const int dialogResult = progress.exec();
    if (dialogResult != QDialog::Accepted && !reply->isFinished()) {
        canceled = true;
        reply->abort();
    }
    if (canceled) return;

    const int statusCode = reply->attribute(
        QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->error() != QNetworkReply::NoError || statusCode != 200) {
        show_update_notice(
            parent, "The update check failed.",
            QString("HTTP status: %1\n%2")
                .arg(statusCode)
                .arg(reply->errorString()));
        return;
    }

    const QByteArray response = reply->readAll();
    if (response.isEmpty() || response.size() > kMaximumReleaseResponseBytes) {
        show_update_notice(
            parent, "The update check failed.",
            "GitHub returned an empty or unexpectedly large release list.");
        return;
    }

    std::string parseError;
    const auto release = select_newest_published_release(
        std::string_view(response.constData(),
                         static_cast<std::size_t>(response.size())),
        &parseError);
    if (!release) {
        show_update_notice(parent, "The update check failed.",
                           QString::fromStdString(parseError));
        return;
    }

    show_release_result(
        parent, *release,
        parse_release_version(std::string_view(GOLIATH_VERSION)));
}

} // namespace goliath
