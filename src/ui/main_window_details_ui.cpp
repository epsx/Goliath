#include "ui/main_window.hpp"

#include "ui/gallery_frame.hpp"
#include "ui/widgets/engraved_controls.hpp"
#include "ui/widgets/main_window_panel_widgets.hpp"

#include <QButtonGroup>
#include <QDesktopServices>
#include <QFont>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMovie>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSize>
#include <QSizePolicy>
#include <QSplitter>
#include <QStatusBar>
#include <QStyle>
#include <QTextEdit>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

#include <cstddef>
#include <string_view>
#include <utility>

namespace goliath {

QWidget* MainWindow::buildDetailsHeaderCard() {
    auto* detailsHeaderCard = new QFrame();
    detailsHeaderCard->setObjectName("details_header_card");
    addPanelElevation(detailsHeaderCard);
    auto* detailsHeaderLayout = new QVBoxLayout(detailsHeaderCard);
    detailsHeaderLayout->setContentsMargins(14, 10, 14, 10);
    detailsHeaderLayout->setSpacing(0);

    auto* detailsTitleRow = new QHBoxLayout();
    detailsTitleRow->setContentsMargins(0, 0, 0, 0);
    detailsTitleRow->setSpacing(10);

    m_detailsTitleLabel = new EngravedTextLabel();
    m_detailsTitleLabel->setObjectName("details_title");
    m_detailsTitleLabel->setFont(QFont("Sans", 20, QFont::Bold));
    m_detailsTitleLabel->setWordWrap(true);
    detailsTitleRow->addWidget(m_detailsTitleLabel, 1, Qt::AlignTop);

    m_favoriteButton = new EngravedTextButton("Favorite");
    m_favoriteButton->setObjectName("favorite_btn");
    m_favoriteButton->setProperty("goliathCircledStar", true);
    m_favoriteButton->setCheckable(true);
    m_favoriteButton->setEnabled(false);
    connect(m_favoriteButton, &QPushButton::toggled,
            this, &MainWindow::toggleSelectedFavorite);
    detailsTitleRow->addWidget(m_favoriteButton, 0, Qt::AlignTop);

    detailsHeaderLayout->addLayout(detailsTitleRow);

    auto* detailsVariantRow = new QHBoxLayout();
    detailsVariantRow->setContentsMargins(0, 0, 0, 0);
    detailsVariantRow->setSpacing(10);

    m_variantLabel = new QLabel();
    m_variantLabel->setObjectName("variant_label");
    m_variantLabel->setFixedHeight(16);
    m_variantLabel->clear();
    detailsVariantRow->addWidget(m_variantLabel, 1, Qt::AlignVCenter);

    auto* ratingRow = new QHBoxLayout();
    ratingRow->setContentsMargins(0, 0, 0, 0);
    ratingRow->setSpacing(0);
    auto* ratingLabel = new EngravedTextLabel("Rate:");
    ratingLabel->setObjectName("secondary_text");
    ratingLabel->setProperty("goliathEngravedRole", "secondary");
    ratingLabel->setFont(QFont("Sans", 15, QFont::DemiBold));
    ratingLabel->setFixedHeight(24);
    ratingRow->addWidget(ratingLabel, 0, Qt::AlignVCenter);
    for (std::size_t index = 0; index < m_ratingButtons.size(); ++index) {
        auto* button = new EngravedTextButton(
            QString::fromUtf8("\xE2\x98\x86")); // ☆
        button->setObjectName("rating_star_btn");
        button->setFixedSize(28, 24);
        button->setEnabled(false);
        const int rating = static_cast<int>(index) + 1;
        button->setAccessibleName(QString("%1-star rating").arg(rating));
        button->setToolTip(
            "Select a parent, variant, CUE, or CHD to rate it");
        connect(button, &QPushButton::clicked, this,
                [this, rating]() { setSelectedRating(rating); });
        m_ratingButtons[index] = button;
        ratingRow->addWidget(button);
    }
    detailsVariantRow->addLayout(ratingRow);
    detailsHeaderLayout->addLayout(detailsVariantRow);

    return detailsHeaderCard;
}

QWidget* MainWindow::buildDetailsInfoCard() {
    auto* infoGrid = new QFrame();
    infoGrid->setObjectName("details_info_card");
    addPanelElevation(infoGrid);
    auto* infoCardLayout = new QGridLayout(infoGrid);
    infoCardLayout->setContentsMargins(16, 14, 16, 14);
    infoCardLayout->setHorizontalSpacing(16);
    infoCardLayout->setVerticalSpacing(8);

    auto makeSectionHeading = [](const QString& text) {
        auto* heading = new EngravedTextLabel(text);
        heading->setObjectName("details_subsection_title");
        heading->setFont(QFont("Sans", 11, QFont::DemiBold));
        return heading;
    };

    auto configureForm = [](QFormLayout* form) {
        form->setContentsMargins(0, 0, 0, 0);
        form->setSpacing(4);
        form->setHorizontalSpacing(12);
        form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
        form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
        form->setRowWrapPolicy(QFormLayout::DontWrapRows);
    };

    auto makeHorizontalDivider = []() {
        auto* divider = new QFrame();
        divider->setObjectName("details_info_horizontal_divider");
        divider->setFrameShape(QFrame::NoFrame);
        divider->setFixedHeight(2);
        divider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        return divider;
    };

    auto addInfoRow = [this](QFormLayout* form, const char* key,
                             const char* label, int minimumLabelWidth) {
        QLabel* value = nullptr;
        if (std::string_view(key) == "catalog_id") {
            value = new CatalogLinkLabel(
                [this](const QString& link) {
                    const QUrl url(link);
                    if (url.scheme() != QStringLiteral("https") ||
                        url.host() != QStringLiteral("redump.info")) {
                        return;
                    }
                    if (!QDesktopServices::openUrl(url)) {
                        statusBar()->showMessage(
                            "Could not open the Redump page in the default browser.",
                            5000);
                    }
                });
            value->setTextFormat(Qt::RichText);
            value->setTextInteractionFlags(Qt::NoTextInteraction);
        } else if (std::string_view(key) == "verification") {
            value = new VerificationDetailsLabel(
                [this]() { showVerificationDetails(); });
            value->setTextFormat(Qt::RichText);
            value->setTextInteractionFlags(Qt::NoTextInteraction);
        } else {
            value = new EngravedTextLabel();
        }
        value->setObjectName("details_value");
        value->setProperty("goliathEngravedRole", "secondary");
        value->setWordWrap(true);
        value->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        value->setSizePolicy(QSizePolicy::Expanding,
                             QSizePolicy::Preferred);

        auto* fieldLabel = new QWidget();
        fieldLabel->setObjectName("details_field_label");
        fieldLabel->setMinimumWidth(minimumLabelWidth);
        fieldLabel->setSizePolicy(QSizePolicy::Fixed,
                                  QSizePolicy::Preferred);

        auto* fieldLabelLayout = new QHBoxLayout(fieldLabel);
        fieldLabelLayout->setContentsMargins(0, 0, 0, 0);
        fieldLabelLayout->setSpacing(0);

        auto* fieldName = new EngravedTextLabel(QString::fromUtf8(label));
        fieldName->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        auto* fieldColon = new EngravedTextLabel(":");
        fieldColon->setAlignment(Qt::AlignRight | Qt::AlignTop);
        fieldLabelLayout->addWidget(fieldName, 0, Qt::AlignTop);
        fieldLabelLayout->addStretch();
        fieldLabelLayout->addWidget(fieldColon, 0, Qt::AlignTop);

        m_infoLabels[key] = value;
        m_infoFieldNameLabels[key] = fieldName;
        m_infoFieldLabels[key] = fieldLabel;
        form->addRow(fieldLabel, value);
    };

    auto* mainDataSection = new QVBoxLayout();
    mainDataSection->setContentsMargins(0, 0, 0, 0);
    mainDataSection->setSpacing(4);
    mainDataSection->addWidget(makeSectionHeading("Main Data"));

    auto* mainDataForm = new QFormLayout();
    configureForm(mainDataForm);

    static const std::pair<const char*, const char*> infoRows[] = {
        {"year", "Year"},
        {"manufacturer", "Manufacturer"},
        {"alt_title", "Alternate title"},
        {"genre", "Genre"},
        {"players", "Players"},
        {"series", "Series"},
    };

    constexpr int primaryLabelWidth = 108;
    for (const auto& [key, label] : infoRows)
        addInfoRow(mainDataForm, key, label, primaryLabelWidth);
    mainDataSection->addLayout(mainDataForm);
    mainDataSection->addStretch();
    infoCardLayout->addLayout(mainDataSection, 0, 0);

    auto* technicalSection = new QVBoxLayout();
    technicalSection->setContentsMargins(0, 0, 0, 0);
    technicalSection->setSpacing(4);
    technicalSection->addWidget(makeSectionHeading("Technical Details"));

    auto* technicalForm = new QFormLayout();
    configureForm(technicalForm);
    static const std::pair<const char*, const char*> technicalRows[] = {
        {"catalog_id", "MAME ID"},
        {"serial", "Serial"},
        {"release", "Release"},
        {"media_format", "Format"},
        {"program_rom", "ROM regions"},
        {"verification", "Verification"},
    };
    for (const auto& [key, label] : technicalRows)
        addInfoRow(technicalForm, key, label, primaryLabelWidth);
    technicalSection->addLayout(technicalForm);
    technicalSection->addStretch();
    infoCardLayout->addLayout(technicalSection, 2, 0);

    infoCardLayout->addWidget(makeHorizontalDivider(), 1, 0);

    auto* divider = new QFrame();
    divider->setObjectName("details_info_vertical_divider");
    divider->setFrameShape(QFrame::NoFrame);
    divider->setFixedWidth(2);
    divider->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    infoCardLayout->addWidget(divider, 0, 1, 3, 1);

    auto* profileSection = new QVBoxLayout();
    profileSection->setContentsMargins(0, 0, 0, 0);
    profileSection->setSpacing(6);

    auto* profileHeader = new QHBoxLayout();
    profileHeader->setContentsMargins(0, 0, 0, 0);
    profileHeader->setSpacing(8);
    profileHeader->addWidget(makeSectionHeading("Launch Profile"));
    profileHeader->addStretch();

    m_gameSettingsButton = new QPushButton("Configure profile");
    m_gameSettingsButton->setObjectName("profile_configure_btn");
    m_gameSettingsButton->setToolTip(
        "Open Per-game Settings for the selected ROM or disc image");
    connect(m_gameSettingsButton, &QPushButton::clicked,
            this, &MainWindow::openGameSettings);
    profileHeader->addWidget(m_gameSettingsButton, 0, Qt::AlignVCenter);
    profileSection->addLayout(profileHeader);

    auto* profileForm = new QFormLayout();
    configureForm(profileForm);
    static const std::pair<const char*, const char*> profileRows[] = {
        {"profile_status", "Profile"},
        {"profile_system", "System"},
        {"profile_video", "Video"},
        {"profile_input", "Input"},
    };
    constexpr int profileLabelWidth = 96;
    for (const auto& [key, label] : profileRows)
        addInfoRow(profileForm, key, label, profileLabelWidth);
    profileSection->addLayout(profileForm);
    profileSection->addStretch();
    infoCardLayout->addLayout(profileSection, 0, 2);

    auto* gameplaySection = new QVBoxLayout();
    gameplaySection->setContentsMargins(0, 0, 0, 0);
    gameplaySection->setSpacing(4);
    gameplaySection->addWidget(makeSectionHeading("Gameplay Stats"));

    auto* gameplayForm = new QFormLayout();
    configureForm(gameplayForm);
    static const std::pair<const char*, const char*> gameplayRows[] = {
        {"playtime", "Playtime"},
        {"sessions", "Sessions"},
        {"last_started", "Last Started"},
        {"last_ended", "Last Ended"},
        {"last_session", "Last Session"},
    };
    for (const auto& [key, label] : gameplayRows)
        addInfoRow(gameplayForm, key, label, profileLabelWidth);
    gameplaySection->addLayout(gameplayForm);
    gameplaySection->addStretch();
    infoCardLayout->addLayout(gameplaySection, 2, 2);

    infoCardLayout->addWidget(makeHorizontalDivider(), 1, 2);
    infoCardLayout->setColumnStretch(0, 4);
    infoCardLayout->setColumnStretch(2, 5);
    infoCardLayout->setRowStretch(2, 1);

    // 390 px preserves the gallery rhythm when every value fits on one line.
    // Treat it as a floor, not a hard clipping boundary, for DPI-dependent
    // wrapped values such as alt titles, profile input, and big-endian.
    infoGrid->setMinimumHeight(390);

    return infoGrid;
}

QWidget* MainWindow::buildDetailsGalleryPanel() {
    auto* snapshotFrame = new QFrame();
    snapshotFrame->setObjectName("snapshot_card");
    addPanelElevation(snapshotFrame);
    snapshotFrame->setFixedSize(520, 390);

    auto* snapshotFrameLayout = new QVBoxLayout(snapshotFrame);
    snapshotFrameLayout->setContentsMargins(10, 4, 10, 10);

    m_snapshotLabel = new QLabel();
    m_snapshotLabel->setObjectName("snapshot_label");
    m_snapshotLabel->setFixedSize(500, 370);
    m_snapshotLabel->setAlignment(Qt::AlignCenter);

    snapshotFrameLayout->addWidget(
        m_snapshotLabel,
        0,
        Qt::AlignTop | Qt::AlignHCenter
    );

    // Keep the original image area. The gallery controls live next to the
    // card, so they take no height from either the image or Description.
    auto* galleryRail = new QWidget();
    galleryRail->setObjectName("gallery_rail");
    galleryRail->setFixedSize(92, 390);
    auto* galleryControls = new QVBoxLayout(galleryRail);
    galleryControls->setContentsMargins(4, 8, 4, 8);
    galleryControls->setSpacing(6);
    auto* galleryModes = new QButtonGroup(this);
    galleryModes->setExclusive(true);
    m_gallerySnaps = new QPushButton("Snaps", galleryRail);
    m_galleryGifs = new QPushButton("GIFs (0)", galleryRail);
    for (QPushButton* mode : {m_gallerySnaps, m_galleryGifs}) {
        mode->setObjectName("gallery_mode_button");
        mode->setCheckable(true);
        mode->setFixedSize(84, 32);
        galleryModes->addButton(mode);
        galleryControls->addWidget(mode, 0, Qt::AlignHCenter);
    }
    m_gallerySnaps->setChecked(true);
    galleryControls->addSpacing(6);
    m_galleryPrevious = new QPushButton(galleryRail);
    m_galleryPlay = new QPushButton(galleryRail);
    m_galleryNext = new QPushButton(galleryRail);
    m_galleryCountLabel = new QLabel(galleryRail);
    m_galleryPrevious->setIcon(m_galleryPrevious->style()->standardIcon(
        QStyle::SP_ArrowUp, nullptr, m_galleryPrevious));
    m_galleryPrevious->setAccessibleName("Previous GIF");
    m_galleryPlay->setIcon(m_galleryPlay->style()->standardIcon(
        QStyle::SP_MediaPlay, nullptr, m_galleryPlay));
    m_galleryPlay->setAccessibleName("Play GIF");
    m_galleryNext->setIcon(m_galleryNext->style()->standardIcon(
        QStyle::SP_ArrowDown, nullptr, m_galleryNext));
    m_galleryNext->setAccessibleName("Next GIF");
    for (QPushButton* button : {m_galleryPrevious, m_galleryPlay, m_galleryNext}) {
        button->setFixedSize(34, 30);
        button->setIconSize(QSize(16, 16));
    }
    m_galleryCountLabel->setFixedWidth(54);
    m_galleryCountLabel->setAlignment(Qt::AlignCenter);
    galleryControls->addWidget(m_galleryPrevious, 0, Qt::AlignHCenter);
    galleryControls->addWidget(m_galleryCountLabel, 0, Qt::AlignHCenter);
    galleryControls->addWidget(m_galleryNext, 0, Qt::AlignHCenter);
    galleryControls->addWidget(m_galleryPlay, 0, Qt::AlignHCenter);
    galleryControls->addStretch();
    for (QPushButton* mode : {m_gallerySnaps, m_galleryGifs}) {
        connect(mode, &QPushButton::toggled, this,
                [this](bool selected) {
                    if (selected) refreshGameGallery();
                });
    }
    connect(m_galleryPrevious, &QPushButton::clicked, this, [this]() {
        stopGalleryMovie();
        if (m_galleryIndex > 0) --m_galleryIndex;
        showGalleryItem();
    });
    connect(m_galleryNext, &QPushButton::clicked, this, [this]() {
        stopGalleryMovie();
        if (m_galleryIndex + 1 < static_cast<int>(m_galleryGifPaths.size()))
            ++m_galleryIndex;
        showGalleryItem();
    });
    connect(m_galleryPlay, &QPushButton::clicked, this, [this]() {
        if (!m_galleryMovie) {
            if (!m_galleryGifs->isChecked() || m_galleryGifPaths.isEmpty()) return;
            const QString path = m_galleryGifPaths.at(m_galleryIndex);
            auto* movie = new QMovie(path, QByteArray("gif"), this);
            if (!movie->isValid()) {
                delete movie;
                m_snapshotLabel->setPixmap(QPixmap());
                m_snapshotLabel->setText("Could not play GIF");
                return;
            }
            m_galleryMovie = movie;
            connect(movie, &QMovie::frameChanged, this, [this, movie](int) {
                if (m_galleryMovie != movie) return;
                m_snapshotLabel->setPixmap(galleryGifFrame(
                    movie->currentImage(), m_snapshotLabel->size()));
            });
            m_galleryPlay->setIcon(m_galleryPlay->style()->standardIcon(
                QStyle::SP_MediaPause, nullptr, m_galleryPlay));
            m_galleryPlay->setAccessibleName("Pause GIF");
            connect(movie, &QMovie::finished, this, [this, movie]() {
                if (m_galleryMovie != movie) return;
                stopGalleryMovie();
                showGalleryItem();
            });
            movie->start();
        } else if (m_galleryMovie->state() == QMovie::Running) {
            m_galleryMovie->setPaused(true);
            m_galleryPlay->setIcon(m_galleryPlay->style()->standardIcon(
                QStyle::SP_MediaPlay, nullptr, m_galleryPlay));
            m_galleryPlay->setAccessibleName("Resume GIF");
        } else {
            m_galleryMovie->setPaused(false);
            m_galleryPlay->setIcon(m_galleryPlay->style()->standardIcon(
                QStyle::SP_MediaPause, nullptr, m_galleryPlay));
            m_galleryPlay->setAccessibleName("Pause GIF");
        }
    });

    auto* galleryPanel = new QWidget();
    galleryPanel->setObjectName("gallery_panel");
    galleryPanel->setFixedSize(620, 406);
    auto* galleryPanelLayout = new QHBoxLayout(galleryPanel);
    // Child effects are clipped by their parent. Reserve paint room inside the
    // gallery container so all four sides of the snapshot shadow stay visible.
    galleryPanelLayout->setContentsMargins(2, 8, 6, 8);
    galleryPanelLayout->setSpacing(0);
    galleryPanelLayout->addWidget(galleryRail, 0, Qt::AlignVCenter);
    galleryPanelLayout->addWidget(snapshotFrame, 0, Qt::AlignVCenter);

    return galleryPanel;
}

void MainWindow::buildDetailsDescription(QVBoxLayout* detailsLayout) {
    auto* historyLabel = new QLabel("Description");
    historyLabel->setObjectName("details_section_title");
    historyLabel->setFont(QFont("Sans", 11, QFont::Bold));
    detailsLayout->addWidget(historyLabel);

    m_historyText = new QTextEdit();
    m_historyText->setObjectName("details_description_card");
    addPanelElevation(m_historyText);
    m_historyText->setReadOnly(true);
    m_historyText->setFrameShape(QFrame::NoFrame);
    m_historyText->setAcceptRichText(false);
    m_historyText->setTextInteractionFlags(
        Qt::TextSelectableByMouse |
        Qt::TextSelectableByKeyboard
    );
    m_historyText->setUndoRedoEnabled(false);
    m_historyText->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_historyText->setLineWrapMode(QTextEdit::WidgetWidth);
    m_historyText->setMinimumHeight(240);
    m_historyText->setContentsMargins(4, 4, 4, 4);

    detailsLayout->addWidget(m_historyText, 1);
}

void MainWindow::buildDetailsPanel() {
    // Right: Details Panel
    auto* rightWidget = new HatchedBackgroundWidget();
    rightWidget->setMinimumWidth(720);
    auto* rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(4, 0, 4, 4);
    rightLayout->setSpacing(14);

    m_detailsScroll = new QScrollArea();
    m_detailsScroll->setWidgetResizable(true);
    m_detailsScroll->setFrameShape(QFrame::NoFrame);
    // The media row intentionally has a useful minimum width. If the library
    // pane is widened on a 1080p display, let the details pane scroll instead
    // of clipping its gallery beyond the window edge.
    m_detailsScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    auto* detailsWidget = new HatchedBackgroundWidget();
    auto* detailsLayout = new QVBoxLayout(detailsWidget);
    detailsLayout->setContentsMargins(8, 0, 8, 8);
    detailsLayout->setSpacing(6);

    detailsLayout->addWidget(buildDetailsHeaderCard());
    detailsLayout->addSpacing(12);

    auto* mediaRow = new QHBoxLayout();
    mediaRow->setSpacing(8);
    // The gallery container establishes the row's vertical shadow room. Keep
    // the row itself margin-free so its total height remains unchanged.
    mediaRow->setContentsMargins(0, 0, 0, 0);
    mediaRow->addWidget(buildDetailsInfoCard(), 1, Qt::AlignVCenter);
    mediaRow->addWidget(buildDetailsGalleryPanel(), 0, Qt::AlignTop);
    detailsLayout->addLayout(mediaRow);

    buildDetailsDescription(detailsLayout);

    m_detailsScroll->setWidget(detailsWidget);
    rightLayout->addWidget(m_detailsScroll, 1);

    m_splitter->addWidget(rightWidget);

}


} // namespace goliath
