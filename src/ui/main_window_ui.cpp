#include "ui/main_window.hpp"

#include "common/theme.hpp"
#include "ui/widgets/title_bar.hpp"

#include <QAction>
#include <QActionGroup>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QFont>
#include <QFormLayout>
#include <QFrame>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QModelIndex>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QPolygon>
#include <QScrollArea>
#include <QShortcut>
#include <QSize>
#include <QSplitter>
#include <QStatusBar>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QTextEdit>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <utility>

namespace fs = std::filesystem;

namespace goliath {

namespace {

// Draw the small triangle PNGs used for QTreeWidget expand/collapse
// indicators in the active theme colors.
std::pair<QString, QString> ensureBranchAssets(
    const fs::path& assetsDir,
    const std::string& themeName,
    const QString& colorHex,
    const std::string& suffixIn = "") {
    std::error_code ec;
    fs::create_directories(assetsDir, ec);

    std::string safeName = themeName;
    std::replace(safeName.begin(), safeName.end(), ' ', '_');
    std::replace(safeName.begin(), safeName.end(), '-', '_');

    const std::string suffix = suffixIn.empty() ? "" : ("-" + suffixIn);

    const fs::path closedPath = assetsDir / ("branch-closed-" + safeName + suffix + ".png");
    const fs::path openPath = assetsDir / ("branch-open-" + safeName + suffix + ".png");

    constexpr int size = 16;
    QPixmap closed(size, size);
    closed.fill(Qt::transparent);
    QPixmap open(size, size);
    open.fill(Qt::transparent);

    const QColor color(colorHex);

    {
        QPainter painter(&closed);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setBrush(color);
        painter.setPen(Qt::NoPen);
        QPolygon poly;
        poly << QPoint(3, 2) << QPoint(13, 8) << QPoint(3, 14);
        painter.drawPolygon(poly);
    }
    {
        QPainter painter(&open);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setBrush(color);
        painter.setPen(Qt::NoPen);
        QPolygon poly;
        poly << QPoint(2, 4) << QPoint(14, 4) << QPoint(8, 13);
        painter.drawPolygon(poly);
    }

    closed.save(QString::fromStdString(closedPath.string()));
    open.save(QString::fromStdString(openPath.string()));

    return {QString::fromStdString(closedPath.string()), QString::fromStdString(openPath.string())};
}

QString toUrlPath(const QString& path) {
    const fs::path abs = fs::absolute(path.toStdString());
    QString result = QString::fromStdString(abs.string());
    return result.replace('\\', '/');
}

class ThemeSelectorItemDelegate final : public QStyledItemDelegate {
public:
    explicit ThemeSelectorItemDelegate(QComboBox* combo)
        : QStyledItemDelegate(combo->view()), m_combo(combo) {}

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override {
        QStyleOptionViewItem themedOption(option);
        initStyleOption(&themedOption, index);
        const std::string themeKey =
            m_combo->currentText().toStdString();
        const Theme& theme = find_theme(themeKey);
        const bool selected =
            themedOption.state.testFlag(QStyle::State_Selected);
        const bool hovered =
            themedOption.state.testFlag(QStyle::State_MouseOver);
        const std::string& background = selected
            ? theme.accent
            : (hovered ? theme.bg_tertiary : theme.bg_hover);

        painter->save();
        painter->setClipRect(themedOption.rect);
        painter->fillRect(themedOption.rect,
                          QColor(QString::fromStdString(background)));
        painter->setFont(themedOption.font);
        painter->setPen(QColor(QString::fromStdString(
            theme_selector_popup_text(theme, selected))));
        painter->drawText(themedOption.rect.adjusted(8, 0, -8, 0),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          themedOption.text);
        painter->restore();
    }

private:
    QComboBox* m_combo;
};

} // namespace

void MainWindow::buildUi() {
    auto* central = new QWidget(this);
    setCentralWidget(central);
    auto* outerLayout = new QVBoxLayout(central);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    // --- Custom Title Bar ---
    m_titleBar = new TitleBar(this);
    outerLayout->addWidget(m_titleBar);

    auto* contentWidget = new QWidget();
    outerLayout->addWidget(contentWidget, 1);
    auto* mainLayout = new QVBoxLayout(contentWidget);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    // --- Modern Top Toolbar ---
    auto* toolbarFrame = new QFrame();
    toolbarFrame->setObjectName("toolbar_frame");
    auto* toolbar = new QHBoxLayout(toolbarFrame);
    toolbar->setSpacing(10);

    auto* themeLabel = new QLabel(QString::fromUtf8("\xF0\x9F\x8E\xA8 Theme:")); // 🎨 Theme:
    themeLabel->setObjectName("toolbar_label");
    toolbar->addWidget(themeLabel);

    m_themeCombo = new QComboBox();
    m_themeCombo->setObjectName("theme_selector");
    m_themeCombo->view()->setObjectName("theme_selector_popup");
    m_themeCombo->view()->setMouseTracking(true);
    m_themeCombo->view()->setItemDelegate(
        new ThemeSelectorItemDelegate(m_themeCombo));
    for (const Theme& t : all_themes()) {
        m_themeCombo->addItem(QString::fromStdString(t.key));
    }
    const std::string configuredThemeKey =
        m_config.get("UI", "theme", "Dark Modern");
    const Theme& configuredTheme = find_theme(configuredThemeKey);
    m_themeCombo->setCurrentText(QString::fromStdString(configuredTheme.key));
    connect(m_themeCombo, &QComboBox::currentTextChanged,
            this, &MainWindow::applyTheme);
    m_themeCombo->setFixedWidth(190);
    toolbar->addWidget(m_themeCombo);

    toolbar->addWidget(new QLabel("  "));

    auto* sortLabel = new QLabel("Sort:");
    sortLabel->setObjectName("toolbar_label");
    toolbar->addWidget(sortLabel);

    m_sortCombo = new QComboBox();
    m_sortCombo->setObjectName("sort_selector");
    struct SortOption { const char* key; const char* label; };
    static const SortOption sortOptions[] = {
        {"display", "Name (A → Z)"},
        {"display_desc", "Name (Z → A)"},
        {"year_desc", "Year (Newest → Oldest)"},
        {"year", "Year (Oldest → Newest)"},
        {"rating_desc", "Rating (5 → 1)"},
        {"rating", "Rating (1 → 5)"},
        {"playtime_desc", "Playtime (Most → Least)"},
        {"playtime", "Playtime (Least → Most)"},
    };
    int sortIndex = 0;
    for (int i = 0; i < (int)(sizeof(sortOptions) / sizeof(sortOptions[0])); ++i) {
        m_sortCombo->addItem(sortOptions[i].label, QString(sortOptions[i].key));
        if (m_sortKey == QString(sortOptions[i].key)) sortIndex = i;
    }
    m_sortCombo->setCurrentIndex(sortIndex);
    connect(m_sortCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onSortChanged);
    m_sortCombo->setFixedWidth(205);
    toolbar->addWidget(m_sortCombo);

    m_filtersButton = new QPushButton("Filters");
    m_filtersButton->setObjectName("filters_btn");
    m_filtersButton->setMinimumWidth(90);
    auto* filtersMenu = new QMenu(m_filtersButton);

    QAction* showVariantsAction = filtersMenu->addAction("Show variants");
    showVariantsAction->setCheckable(true);
    showVariantsAction->setChecked(m_showVariants);
    connect(showVariantsAction, &QAction::toggled,
            this, &MainWindow::onShowVariantsChanged);
    filtersMenu->addSeparator();

    auto* ratingMenu = filtersMenu->addMenu("Rating");
    m_ratingFilterGroup = new QActionGroup(filtersMenu);
    m_ratingFilterGroup->setExclusive(true);
    struct RatingFilterOption {
        LibraryRatingFilter filter;
        const char* label;
    };
    static const RatingFilterOption ratingFilterOptions[] = {
        {LibraryRatingFilter::Any, "Any rating"},
        {LibraryRatingFilter::Rated, "Rated"},
        {LibraryRatingFilter::Unrated, "Unrated"},
        {LibraryRatingFilter::AtLeast1, "At least 1 star"},
        {LibraryRatingFilter::AtLeast2, "At least 2 stars"},
        {LibraryRatingFilter::AtLeast3, "At least 3 stars"},
        {LibraryRatingFilter::AtLeast4, "At least 4 stars"},
        {LibraryRatingFilter::AtLeast5, "5 stars"},
    };
    for (const RatingFilterOption& option : ratingFilterOptions) {
        QAction* action = ratingMenu->addAction(option.label);
        action->setCheckable(true);
        action->setData(static_cast<int>(option.filter));
        action->setChecked(m_ratingFilter == option.filter);
        m_ratingFilterGroup->addAction(action);
    }
    connect(m_ratingFilterGroup, &QActionGroup::triggered, this,
            [this](QAction* action) {
                if (!action) return;
                setRatingFilter(static_cast<LibraryRatingFilter>(
                    action->data().toInt()));
            });

    auto* playtimeMenu = filtersMenu->addMenu("Playtime");
    m_playtimeFilterGroup = new QActionGroup(filtersMenu);
    m_playtimeFilterGroup->setExclusive(true);
    struct PlaytimeFilterOption {
        LibraryPlaytimeFilter filter;
        const char* label;
    };
    static const PlaytimeFilterOption playtimeFilterOptions[] = {
        {LibraryPlaytimeFilter::Any, "Any"},
        {LibraryPlaytimeFilter::Played, "Played"},
        {LibraryPlaytimeFilter::NotPlayed, "Not played"},
    };
    for (const PlaytimeFilterOption& option : playtimeFilterOptions) {
        QAction* action = playtimeMenu->addAction(option.label);
        action->setCheckable(true);
        action->setData(static_cast<int>(option.filter));
        action->setChecked(m_playtimeFilter == option.filter);
        m_playtimeFilterGroup->addAction(action);
    }
    connect(m_playtimeFilterGroup, &QActionGroup::triggered, this,
            [this](QAction* action) {
                if (!action) return;
                setPlaytimeFilter(static_cast<LibraryPlaytimeFilter>(
                    action->data().toInt()));
            });

    filtersMenu->addSeparator();
    m_clearFiltersAction = filtersMenu->addAction(
        "Clear rating/playtime filters", this,
        &MainWindow::clearLibraryFilters);
    m_filtersButton->setMenu(filtersMenu);
    toolbar->addWidget(m_filtersButton);
    updateFiltersButton();

    auto* commandOverlayCheckbox = new QCheckBox("Command overlay");
    commandOverlayCheckbox->setChecked(m_commandOverlayEnabled);
    commandOverlayCheckbox->setToolTip(
        "Open matching command.dat overlays automatically for future game "
        "launches");
    connect(commandOverlayCheckbox, &QCheckBox::toggled,
            this, &MainWindow::onCommandOverlayChanged);
    toolbar->addWidget(commandOverlayCheckbox);

    auto* favoritesOnlyCheckbox = new QCheckBox(
        QString::fromUtf8("\xE2\x98\x85 Favorites only")); // ★
    favoritesOnlyCheckbox->setChecked(m_favoritesOnly);
    favoritesOnlyCheckbox->setToolTip(
        "Show only favorite games in the current Neo Geo library");
    connect(favoritesOnlyCheckbox, &QCheckBox::toggled,
            this, &MainWindow::onFavoritesOnlyChanged);
    toolbar->addWidget(favoritesOnlyCheckbox);

    toolbar->addWidget(new QLabel("  "));

    auto* randomBtn = new QPushButton(QString::fromUtf8("\xF0\x9F\x8E\xB2 Random")); // 🎲 Random
    randomBtn->setObjectName("random_btn");
    randomBtn->setToolTip("Launch random game (Ctrl+R)");
    connect(randomBtn, &QPushButton::clicked, this, &MainWindow::launchRandomGame);
    toolbar->addWidget(randomBtn);

    toolbar->addStretch();

    auto* toolsBtn = new QPushButton("Tools \xE2\x96\xBC"); // Tools ▼
    toolsBtn->setObjectName("tools_btn");
    auto* toolsMenu = new QMenu(toolsBtn);
    m_selectionActions.clear();
    m_selectionActions.push_back(toolsMenu->addAction(
        QString::fromUtf8("\xE2\x9A\x99 Per-game Settings..."),
        this, &MainWindow::openGameSettings));
    m_selectionActions.push_back(toolsMenu->addAction(
        QString::fromUtf8("\xF0\x9F\x92\xBE Manage Save Data..."),
        this, &MainWindow::manageSaveData));
    m_selectionActions.push_back(toolsMenu->addAction(
        QString::fromUtf8("\xE2\x8F\xB1 Benchmark Selected Game..."),
        this, &MainWindow::benchmarkSelected));
    m_selectionActions.push_back(toolsMenu->addAction(
        QString::fromUtf8("\xF0\x9F\x8E\xB5 Export Selected Audio WAV..."),
        this, &MainWindow::exportSelectedAudio));
    toolsMenu->addSeparator();
    m_rescanAction = toolsMenu->addAction(QString::fromUtf8("\xF0\x9F\x94\x84 Rescan ROMs (F5)"), this, &MainWindow::rescanRoms);
    toolsMenu->addAction(QString::fromUtf8("\xF0\x9F\x94\x8D Verify BIOS"),
                         this, &MainWindow::verifyBios); // 🔍
    toolsMenu->addSeparator();
    toolsMenu->addAction(QString::fromUtf8(
                             "\xF0\x9F\xA9\xBA Diagnostics & Logs..."), // 🩺
                         this, &MainWindow::openLogging);
    toolsBtn->setMenu(toolsMenu);
    toolbar->addWidget(toolsBtn);

    auto* settingsBtn = new QPushButton(QString::fromUtf8("\xE2\x9A\x99 Settings")); // ⚙ Settings
    settingsBtn->setObjectName("settings_btn");
    connect(settingsBtn, &QPushButton::clicked, this, &MainWindow::openSettings);
    toolbar->addWidget(settingsBtn);

    auto* aboutBtn = new QPushButton("About");
    aboutBtn->setObjectName("about_btn");
    aboutBtn->setIcon(aboutBtn->style()->standardIcon(
        QStyle::SP_MessageBoxInformation, nullptr, aboutBtn));
    aboutBtn->setIconSize(QSize(16, 16));
    aboutBtn->setToolTip("About Goliath");
    connect(aboutBtn, &QPushButton::clicked, this, &MainWindow::openAbout);
    toolbar->addWidget(aboutBtn);

    mainLayout->addWidget(toolbarFrame);

    // --- Splitter ---
    m_splitter = new QSplitter(Qt::Horizontal);
    m_splitter->setObjectName("library_details_splitter");
    m_splitter->setHandleWidth(6);
    mainLayout->addWidget(m_splitter, 1);

    // Left: Game List + Search
    auto* leftPanel = new QWidget();
    auto* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(10);

    auto* systemSelector = new QFrame();
    systemSelector->setObjectName("system_selector_frame");
    auto* systemLayout = new QHBoxLayout(systemSelector);
    systemLayout->setContentsMargins(2, 2, 2, 2);
    systemLayout->setSpacing(2);

    m_mvsAesButton = new QPushButton("Neo Geo MVS/AES");
    m_mvsAesButton->setObjectName("system_selector_button");
    m_mvsAesButton->setCheckable(true);
    m_mvsAesButton->setAutoExclusive(true);
    m_mvsAesButton->setChecked(m_librarySystem == "neogeo");
    m_mvsAesButton->setToolTip("Show Neo Geo cartridge games (MVS / AES)");
    connect(m_mvsAesButton, &QPushButton::clicked, this, [this] {
        setLibrarySystem("neogeo");
    });
    systemLayout->addWidget(m_mvsAesButton);

    m_cdButton = new QPushButton("Neo Geo CD");
    m_cdButton->setObjectName("system_selector_button");
    m_cdButton->setCheckable(true);
    m_cdButton->setAutoExclusive(true);
    m_cdButton->setChecked(m_librarySystem == "neogeocd");
    m_cdButton->setToolTip("Show Neo Geo CD games (CD / CDZ modes use the same game library)");
    connect(m_cdButton, &QPushButton::clicked, this, [this] {
        setLibrarySystem("neogeocd");
    });
    systemLayout->addWidget(m_cdButton);

    leftLayout->addWidget(systemSelector);

    m_tree = new QTreeWidget();
    m_tree->setColumnCount(2);
    m_tree->setHeaderHidden(true);
    // QTreeView enables stretchLastSection by default.  That would make the
    // CD-only verification column consume a large part of the game list and
    // visually continue the 50/50 system-selector split into the rows below.
    // Keep the game-title column flexible and the badge column content-sized.
    m_tree->header()->setStretchLastSection(false);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_tree->setRootIsDecorated(true);
    m_tree->setIconSize(QSize(32, 32));
    // Rows mix text-only entries with 32 px game icons.  Uniform row heights can
    // make icon rows overlap when the first visible item has no icon, so let
    // Qt size each row from its actual content.
    m_tree->setUniformRowHeights(false);
    m_tree->setAlternatingRowColors(true);
    m_tree->setIndentation(18);
    connect(m_tree, &QTreeWidget::itemSelectionChanged, this, &MainWindow::updateSelection);
    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, &MainWindow::launchSelectedItem);
    connect(m_tree, &QTreeWidget::itemCollapsed, this,
            [this](QTreeWidgetItem*) { ensureVisibleSelection(false); });
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tree, &QTreeWidget::customContextMenuRequested, this, &MainWindow::showTreeContextMenu);
    leftLayout->addWidget(m_tree, 1);

    m_searchEntry = new QLineEdit();
    m_searchEntry->setPlaceholderText(QString::fromUtf8("\xF0\x9F\x94\x8D Search games...")); // 🔍
    m_searchEntry->setClearButtonEnabled(true);
    m_searchEntry->setToolTip(
        "Filter the current library. Use the clear button or press Escape "
        "to restore the complete list. Ctrl+F focuses and selects the current query.");
    connect(m_searchEntry, &QLineEdit::textChanged, this, &MainWindow::filterGames);

    auto* clearSearchShortcut =
        new QShortcut(QKeySequence(Qt::Key_Escape), m_searchEntry);
    clearSearchShortcut->setContext(Qt::WidgetShortcut);
    connect(clearSearchShortcut, &QShortcut::activated,
            m_searchEntry, &QLineEdit::clear);
    leftLayout->addWidget(m_searchEntry);

    m_splitter->addWidget(leftPanel);

    // Right: Details Panel
    auto* rightWidget = new QWidget();
    rightWidget->setMinimumWidth(720);
    auto* rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(4, 4, 4, 4);
    rightLayout->setSpacing(14);

    m_detailsScroll = new QScrollArea();
    m_detailsScroll->setWidgetResizable(true);
    m_detailsScroll->setFrameShape(QFrame::NoFrame);

    auto* detailsWidget = new QWidget();
    auto* detailsLayout = new QVBoxLayout(detailsWidget);
    detailsLayout->setContentsMargins(8, 8, 8, 8);
    detailsLayout->setSpacing(6);

    auto* detailsHeaderLayout = new QVBoxLayout();
    detailsHeaderLayout->setContentsMargins(0, 0, 0, 0);
    detailsHeaderLayout->setSpacing(0);

    auto* detailsTitleRow = new QHBoxLayout();
    detailsTitleRow->setContentsMargins(0, 0, 0, 0);
    detailsTitleRow->setSpacing(10);

    m_detailsTitleLabel = new QLabel();
    m_detailsTitleLabel->setObjectName("details_title");
    m_detailsTitleLabel->setFont(QFont("Sans", 20, QFont::Bold));
    m_detailsTitleLabel->setWordWrap(true);
    detailsTitleRow->addWidget(m_detailsTitleLabel, 1, Qt::AlignTop);

    m_favoriteButton = new QPushButton(
        QString::fromUtf8("\xE2\x98\x86 Favorite")); // ☆
    m_favoriteButton->setObjectName("favorite_btn");
    m_favoriteButton->setCheckable(true);
    m_favoriteButton->setEnabled(false);
    m_favoriteButton->setToolTip(
        "Add the selected parent, variant, CUE, or CHD to Favorites");
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
    auto* ratingLabel = new QLabel("Rating:");
    ratingLabel->setObjectName("secondary_text");
    ratingRow->addWidget(ratingLabel, 0, Qt::AlignVCenter);

    for (std::size_t index = 0; index < m_ratingButtons.size(); ++index) {
        auto* button = new QPushButton(
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
    detailsLayout->addLayout(detailsHeaderLayout);

    auto* separator = new QFrame();
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Plain);
    separator->setObjectName("details_separator");
    detailsLayout->addWidget(separator);

    auto* mediaRow = new QHBoxLayout();
    mediaRow->setSpacing(8);
    mediaRow->setContentsMargins(0, 0, 0, 0);

    auto* infoGrid = new QFrame();
    auto* infoGridLayout = new QFormLayout(infoGrid);

    infoGridLayout->setContentsMargins(0, 2, 0, 4);
    infoGridLayout->setSpacing(4);
    infoGridLayout->setHorizontalSpacing(14);
    infoGridLayout->setLabelAlignment(Qt::AlignRight);
    infoGridLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    infoGridLayout->setRowWrapPolicy(QFormLayout::DontWrapRows);

    static const std::pair<const char*, const char*> infoRows[] = {
        {"year", "Year"},
        {"manufacturer", "Manufacturer"},
        {"genre", "Genre"},
        {"players", "Players"},
        {"series", "Series"},
        {"playtime", "Playtime"},
        {"sessions", "Sessions"},
        {"last_played", "Last Played"},
    };

    for (const auto& [key, label] : infoRows) {
        auto* lbl = new QLabel();
        lbl->setObjectName("details_value");
        lbl->setWordWrap(true);

        auto* fieldLabel = new QLabel(
            QString("%1:").arg(label)
        );

        fieldLabel->setMinimumWidth(110);
        fieldLabel->setAlignment(
            Qt::AlignRight | Qt::AlignVCenter
        );

        m_infoLabels[key] = lbl;

        infoGridLayout->addRow(fieldLabel, lbl);
    }

    mediaRow->addWidget(infoGrid, 1);

    auto* snapshotFrame = new QFrame();
    snapshotFrame->setObjectName("snapshot_card");
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

    mediaRow->addWidget(snapshotFrame, 0, Qt::AlignTop);
    detailsLayout->addLayout(mediaRow);

    auto* historyLabel = new QLabel("Description");
    historyLabel->setObjectName("details_section_title");
    historyLabel->setFont(QFont("Sans", 11, QFont::Bold));
    detailsLayout->addWidget(historyLabel);

    m_historyText = new QTextEdit();
    m_historyText->setReadOnly(true);
    m_historyText->setFrameShape(QFrame::NoFrame);
    m_historyText->setAcceptRichText(false);
    m_historyText->setTextInteractionFlags(
        Qt::TextSelectableByMouse |
        Qt::TextSelectableByKeyboard
    );
    m_historyText->setUndoRedoEnabled(false);
    m_historyText->setMinimumHeight(240);
    m_historyText->setContentsMargins(4, 4, 4, 4);

    detailsLayout->addWidget(m_historyText, 1);

    m_detailsScroll->setWidget(detailsWidget);
    rightLayout->addWidget(m_detailsScroll, 1);

    m_splitter->addWidget(rightWidget);

    m_splitter->setCollapsible(0, false);
    m_splitter->setCollapsible(1, true);

    m_splitter->setSizes({400, 760});

    // Status bar with Launch button in the bottom-right corner
    auto* statusBar_ = statusBar();
    // ResizeFilter already provides all-edge resizing for this frameless
    // window, so the native QSizeGrip would only add a stray corner square.
    statusBar_->setSizeGripEnabled(false);
    m_launchButton = new QPushButton(QString::fromUtf8("\xE2\x96\xB6 Launch")); // ▶ Launch
    m_launchButton->setObjectName("launch_btn");
    m_launchButton->setToolTip("Launch selected game (Enter)");
    connect(m_launchButton, &QPushButton::clicked,
            this, &MainWindow::launchSelected);
    statusBar_->addPermanentWidget(m_launchButton);
    setSelectionActionsEnabled(false);

    // Keyboard shortcuts
    connect(new QShortcut(QKeySequence("F5"), this), &QShortcut::activated, this, &MainWindow::rescanRoms);
    connect(new QShortcut(QKeySequence("Ctrl+F"), this), &QShortcut::activated, this, &MainWindow::focusSearch);
    connect(new QShortcut(QKeySequence("Ctrl+R"), this), &QShortcut::activated, this, &MainWindow::launchRandomGame);
    connect(new QShortcut(QKeySequence("Ctrl+Shift+E"), this), &QShortcut::activated, this, &MainWindow::expandAll);
    connect(new QShortcut(QKeySequence("Ctrl+Shift+C"), this), &QShortcut::activated, this, &MainWindow::collapseAll);

    auto* launchShortcut = new QShortcut(QKeySequence("Return"), m_tree);
    launchShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(launchShortcut, &QShortcut::activated, this, &MainWindow::launchSelected);
}

void MainWindow::applyTheme(const QString& themeNameIn) {
    const std::string requestedThemeKey = themeNameIn.toStdString();
    const Theme& theme = find_theme(requestedThemeKey);
    const std::string& themeKey = theme.key;
    const QString themeName = QString::fromStdString(themeKey);
    std::string stylesheet = generate_theme_style(theme);

    fs::path assetsDir = m_paths.base_dir / "assets";
    auto [closedPath, openPath] = ensureBranchAssets(assetsDir, themeKey,
                                                       QString::fromStdString(theme.text_primary));
    auto [closedSelPath, openSelPath] = ensureBranchAssets(assetsDir, themeKey,
        QString::fromStdString(theme.text_secondary), "selected");

    QString qss = QString::fromStdString(stylesheet);
    qss.replace("<<BRANCH_CLOSED>>", toUrlPath(closedPath));
    qss.replace("<<BRANCH_OPEN>>", toUrlPath(openPath));
    qss.replace("<<BRANCH_CLOSED_SELECTED>>", toUrlPath(closedSelPath));
    qss.replace("<<BRANCH_OPEN_SELECTED>>", toUrlPath(openSelPath));

    setStyleSheet(qss);
    const QString comboPopupStyle = QString::fromStdString(
        generate_combo_popup_style(theme));
    applyComboPopupStyle(m_themeCombo, comboPopupStyle);
    applyComboPopupStyle(m_sortCombo, comboPopupStyle);

    m_config.set("UI", "theme", themeName.toStdString());
    save_config(m_config);
}

} // namespace goliath
