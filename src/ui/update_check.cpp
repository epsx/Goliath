#include "ui/update_check.hpp"

#include "common/debug_logger.hpp"
#include "common/goliath_common.hpp"
#include "ui/widgets/title_bar.hpp"
#include "update/release_feed.hpp"

#include <QDesktopServices>
#include <QDateTime>
#include <QDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProgressBar>
#include <QPushButton>
#include <QSize>
#include <QTextBrowser>
#include <QTextDocument>
#include <QUrl>
#include <QVariant>
#include <QVBoxLayout>

#include <utility>

#ifndef GOLIATH_VERSION
#define GOLIATH_VERSION "development"
#endif

#ifndef GOLIATH_RELEASES_API_URL
#define GOLIATH_RELEASES_API_URL ""
#endif

namespace goliath {

namespace {

constexpr qint64 kMaximumReleaseResponseBytes = 1024 * 1024;
constexpr qsizetype kMaximumDisplayedReleaseNotesCharacters = 32 * 1024;

QString display_release_date(const std::string& publishedAt) {
    const QString value = QString::fromStdString(publishedAt);
    return value.size() >= 10 ? value.left(10) : value;
}

bool is_trusted_release_url(const QUrl& url) {
    return url.isValid() && url.scheme() == "https" &&
           url.host().compare("github.com", Qt::CaseInsensitive) == 0;
}

QUrl releases_api_url() {
    return QUrl(QString::fromUtf8(GOLIATH_RELEASES_API_URL));
}

bool is_valid_releases_api_url(const QUrl& url) {
    return url.isValid() && url.scheme() == "https" &&
           url.host().compare("api.github.com", Qt::CaseInsensitive) == 0;
}

QNetworkRequest release_request(const QUrl& apiUrl) {
    QNetworkRequest request(apiUrl);
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
    request.setRawHeader(
        "User-Agent", QByteArray("Goliath/") + QByteArray(GOLIATH_VERSION));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(15000);
    return request;
}

std::optional<PublishedRelease> release_from_reply(
        QNetworkReply* reply, QString* error) {
    if (error) error->clear();
    if (!reply) {
        if (error) *error = "The update request did not return a reply.";
        return std::nullopt;
    }

    const int statusCode = reply->attribute(
        QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->error() != QNetworkReply::NoError || statusCode != 200) {
        if (error) {
            *error = QString("HTTP status: %1\n%2")
                         .arg(statusCode)
                         .arg(reply->errorString());
        }
        return std::nullopt;
    }

    const QByteArray response = reply->readAll();
    if (response.isEmpty() || response.size() > kMaximumReleaseResponseBytes) {
        if (error) {
            *error = "GitHub returned an empty or unexpectedly large "
                     "release list.";
        }
        return std::nullopt;
    }

    std::string parseError;
    auto release = select_newest_published_release(
        std::string_view(response.constData(),
                         static_cast<std::size_t>(response.size())),
        &parseError);
    if (!release && error) *error = QString::fromStdString(parseError);
    return release;
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

void open_changelog_link(QWidget* parent, const QUrl& releaseUrl,
                         const QUrl& selectedUrl) {
    const QUrl target = selectedUrl.isRelative()
        ? releaseUrl.resolved(selectedUrl)
        : selectedUrl;
    if (!target.isValid() || target.scheme() != "https") {
        show_update_notice(parent, "The changelog link could not be opened.",
                           "Only valid HTTPS links are allowed.");
        return;
    }
    if (!QDesktopServices::openUrl(target)) {
        show_update_notice(
            parent, "The changelog link could not be opened.",
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
    QString releaseNotes = QString::fromStdString(release.body).trimmed();
    const bool showReleaseNotes = !releaseNotes.isEmpty();
    if (releaseNotes.size() > kMaximumDisplayedReleaseNotesCharacters) {
        releaseNotes.truncate(kMaximumDisplayedReleaseNotesCharacters);
        releaseNotes += "\n\n_Release notes truncated. Open the release page "
                        "to read the complete changelog._";
    }

    QDialog dialog(parent);
    dialog.resize(showReleaseNotes ? QSize(720, 590) : QSize(560, 290));
    dialog.setMinimumWidth(460);
    dialog.setModal(true);
    dialog.setSizeGripEnabled(false);

    QVBoxLayout* layout = nullptr;
    setupFramelessDialog(
        &dialog, "Goliath Update", &layout, showReleaseNotes, false);

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

    if (showReleaseNotes) {
        auto* notesHeading = new QLabel("What's changed", card);
        notesHeading->setObjectName("about_section_heading");
        cardLayout->addWidget(notesHeading);

        auto* notes = new QTextBrowser(card);
        notes->setObjectName("update_release_notes");
        notes->setOpenLinks(false);
        notes->setOpenExternalLinks(false);
        notes->setTextInteractionFlags(Qt::TextBrowserInteraction);
        notes->setMinimumHeight(190);
        QTextDocument::MarkdownFeatures markdownFeatures(
            QTextDocument::MarkdownDialectGitHub);
        markdownFeatures.setFlag(QTextDocument::MarkdownNoHTML);
        notes->document()->setMarkdown(releaseNotes, markdownFeatures);
        QObject::connect(
            notes, &QTextBrowser::anchorClicked, &dialog,
            [&dialog, releaseUrl](const QUrl& selectedUrl) {
            open_changelog_link(&dialog, releaseUrl, selectedUrl);
        });
        cardLayout->addWidget(notes, 1);
    }
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

bool checkForUpdates(QWidget* parent) {
    const QUrl apiUrl = releases_api_url();
    if (!is_valid_releases_api_url(apiUrl)) {
        show_update_notice(parent, "Update check unavailable.",
                           "The Goliath release service is not configured.");
        return false;
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
    QNetworkReply* reply = network.get(release_request(apiUrl));
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
    if (canceled) return false;

    QString error;
    const auto release = release_from_reply(reply, &error);
    if (!release) {
        show_update_notice(parent, "The update check failed.",
                           error);
        return false;
    }

    show_release_result(
        parent, *release,
        parse_release_version(std::string_view(GOLIATH_VERSION)));
    return true;
}

void checkForUpdatesAutomatically(
        QWidget* parent, UpdateCheckCompletion completion) {
    const QUrl apiUrl = releases_api_url();
    if (!parent || !is_valid_releases_api_url(apiUrl)) {
        DebugLogger::logWarn(
            "automatic update check skipped: release service is not configured");
        if (completion) completion(false);
        return;
    }

    auto* network = new QNetworkAccessManager(parent);
    QNetworkReply* reply = network->get(release_request(apiUrl));
    QObject::connect(
        reply, &QNetworkReply::finished, parent,
        [parent, network, reply, completion = std::move(completion)]() {
        QString error;
        const auto release = release_from_reply(reply, &error);
        if (!release) {
            DebugLogger::logWarn(
                "automatic update check failed: " + error);
            if (completion) completion(false);
        } else {
            const auto current = parse_release_version(
                std::string_view(GOLIATH_VERSION));
            if (!current ||
                compare_release_versions(release->version, *current) > 0) {
                show_release_result(parent, *release, current);
            }
            DebugLogger::logInfo(
                QString("automatic update check completed: latest=%1")
                    .arg(QString::fromStdString(release->tag)));
            if (completion) completion(true);
        }
        reply->deleteLater();
        network->deleteLater();
    });
}

void recordSuccessfulUpdateCheck(Config& config) {
    config.set(
        "Updates", "last_successful_check_epoch",
        std::to_string(QDateTime::currentSecsSinceEpoch()));
    save_config(config);
}

} // namespace goliath
