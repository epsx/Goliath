#include "ui/main_window.hpp"

#include "common/qt_path.hpp"
#include "common/theme.hpp"
#include "ui/application_shortcuts.hpp"
#include "ui/lithogen_dialog.hpp"
#include "ui/widgets/engraved_controls.hpp"
#include "ui/widgets/main_window_panel_widgets.hpp"

#include <QAction>
#include <QActionGroup>
#include <QAbstractItemView>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QDialog>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QModelIndex>
#include <QPainter>
#include <QPoint>
#include <QPushButton>
#include <QSizePolicy>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QVBoxLayout>
#include <QWidget>

namespace goliath {

namespace {

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

void MainWindow::buildToolbarFilters(QHBoxLayout* toolbar) {
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
    m_filtersButton = new EngravedActionButton(
        "Filters", EngravedActionButton::Glyph::Filters);
    m_filtersButton->setObjectName("filters_btn");
    m_filtersButton->setMinimumWidth(90);
    auto* filtersMenu = new QMenu(m_filtersButton);

    struct ViewOption {
        LibraryDisplayMode mode;
        const char* label;
    };
    static const ViewOption viewOptions[] = {
        {LibraryDisplayMode::List, libraryDisplayModeLabel(
             LibraryDisplayMode::List).data()},
        {LibraryDisplayMode::Grid, libraryDisplayModeLabel(
             LibraryDisplayMode::Grid).data()},
        {LibraryDisplayMode::BigIcons, libraryDisplayModeLabel(
             LibraryDisplayMode::BigIcons).data()},
    };
    auto* viewMenu = filtersMenu->addMenu("View");
    m_libraryDisplayGroup = new QActionGroup(viewMenu);
    m_libraryDisplayGroup->setExclusive(true);
    for (const ViewOption& option : viewOptions) {
        QAction* action = viewMenu->addAction(option.label);
        action->setCheckable(true);
        action->setData(static_cast<int>(option.mode));
        action->setChecked(m_libraryDisplayMode == option.mode);
        m_libraryDisplayGroup->addAction(action);
    }
    connect(m_libraryDisplayGroup, &QActionGroup::triggered, this,
            [this](QAction* action) {
                if (!action) return;
                setLibraryDisplayMode(static_cast<LibraryDisplayMode>(
                    action->data().toInt()));
            });
    filtersMenu->addSeparator();

    auto* sortMenu = filtersMenu->addMenu("Sort");
    m_sortGroup = new QActionGroup(sortMenu);
    m_sortGroup->setExclusive(true);
    for (const SortOption& option : sortOptions) {
        QAction* action = sortMenu->addAction(option.label);
        action->setCheckable(true);
        action->setData(QString::fromUtf8(option.key));
        action->setChecked(m_sortKey == QString::fromUtf8(option.key));
        m_sortGroup->addAction(action);
    }
    connect(m_sortGroup, &QActionGroup::triggered, this,
            [this](QAction* action) {
                if (!action) return;
                const QString requested = action->data().toString();
                if (requested.isEmpty() || requested == m_sortKey) return;
                m_sortKey = requested;
                onSortChanged();
            });
    filtersMenu->addSeparator();

    QAction* showVariantsAction = filtersMenu->addAction("Show variants");
    showVariantsAction->setCheckable(true);
    showVariantsAction->setChecked(m_showVariants);
    connect(showVariantsAction, &QAction::toggled,
            this, &MainWindow::onShowVariantsChanged);

    QAction* commandOverlayAction = filtersMenu->addAction("Command overlay");
    commandOverlayAction->setCheckable(true);
    commandOverlayAction->setChecked(m_commandOverlayEnabled);
    commandOverlayAction->setToolTip(
        "Open matching command.dat overlays automatically for future game "
        "launches");
    connect(commandOverlayAction, &QAction::toggled,
            this, &MainWindow::onCommandOverlayChanged);
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
#if defined(GOLIATH_X11_CAPTURE)
    // Qt's native menu indicator sits at the lower edge of styled buttons
    // on this X11 theme. Open the same menu from a plain themed button.
    connect(m_filtersButton, &QPushButton::clicked, this, [this, filtersMenu]() {
        filtersMenu->popup(m_filtersButton->mapToGlobal(
            QPoint(0, m_filtersButton->height())));
    });
#else
    m_filtersButton->setMenu(filtersMenu);
#endif
    toolbar->addWidget(m_filtersButton);
    updateFiltersButton();

    auto* favoritesOnlyCheckbox = new EngravedCheckBox("Favorites only");
    favoritesOnlyCheckbox->setChecked(m_favoritesOnly);
    connect(favoritesOnlyCheckbox, &QCheckBox::toggled,
            this, &MainWindow::onFavoritesOnlyChanged);
    toolbar->addWidget(favoritesOnlyCheckbox);

}

void MainWindow::buildToolbarTools(QHBoxLayout* toolbar) {
    auto* toolsBtn = new EngravedActionButton(
        "Tools", EngravedActionButton::Glyph::Tools);
    toolsBtn->setObjectName("tools_btn");
    auto* toolsMenu = new QMenu(toolsBtn);
    m_selectionActions.clear();
    m_selectionActions.push_back(toolsMenu->addAction(
        QString::fromUtf8("\xE2\x9A\x99 Per-game Settings..."),
        this, &MainWindow::openGameSettings));
    m_selectionActions.push_back(toolsMenu->addAction(
        QString::fromUtf8("\xF0\x9F\x92\xBE Save Data Manager..."),
        this, &MainWindow::manageSaveData));
    m_selectionActions.push_back(toolsMenu->addAction(
        QString::fromUtf8("\xE2\x8F\xB1 Benchmark Selected Game..."),
        this, &MainWindow::benchmarkSelected));
    m_selectionActions.push_back(toolsMenu->addAction(
        QString::fromUtf8("\xF0\x9F\x8E\xB5 Export Selected Audio WAV..."),
        this, &MainWindow::exportSelectedAudio));
    m_screenshotAction = toolsMenu->addAction(
        "Capture game screenshot (3s)...", this,
        &MainWindow::captureGameScreenshot);
#if defined(GOLIATH_X11_CAPTURE)
    m_gifAction = toolsMenu->addAction(
        "Record game GIF (3s)...", this, [this]() { recordGameGifAfter(3000); });
#endif
    updateScreenshotAction();
    toolsMenu->addAction(QString::fromUtf8(
        "\xF0\x9F\x94\x81 Convertor .zip to .neo (Lithogen)"), this, [this]() {
        LithogenDialog dialog(m_config, path_to_qstring(m_romDir), this);
        if (dialog.exec() == QDialog::Accepted) rescanRoms();
    });
    toolsMenu->addSeparator();
    m_rescanAction = toolsMenu->addAction(
        QString::fromUtf8("\xF0\x9F\x94\x84 Rescan ROMs (%1)").arg(
            QKeySequence(kRescanRomsShortcut).toString(QKeySequence::NativeText)),
        this, &MainWindow::rescanRoms);
    toolsMenu->addAction(
        QString::fromUtf8("\xF0\x9F\x94\x8D Verify BIOS (%1)").arg(
            QKeySequence(kVerifyBiosShortcut).toString(
                QKeySequence::NativeText)),
        this, &MainWindow::verifyBios); // 🔍
    toolsMenu->addSeparator();
    toolsMenu->addAction(QString::fromUtf8(
                             "\xF0\x9F\x93\x84 Export Diagnostic Report..."), // 📄
                         this, &MainWindow::exportDiagnosticReport);
    toolsMenu->addAction(QString::fromUtf8(
                             "\xF0\x9F\xA9\xBA Diagnostics & Logs..."), // 🩺
                         this, &MainWindow::openLogging);
#if defined(GOLIATH_X11_CAPTURE)
    connect(toolsBtn, &QPushButton::clicked, this, [toolsBtn, toolsMenu]() {
        toolsMenu->popup(toolsBtn->mapToGlobal(QPoint(0, toolsBtn->height())));
    });
#else
    toolsBtn->setMenu(toolsMenu);
#endif
    toolbar->addWidget(toolsBtn);

}

void MainWindow::buildToolbar(QVBoxLayout* mainLayout) {
    // --- Modern Top Toolbar ---
    auto* toolbarFrame = new QFrame();
    toolbarFrame->setObjectName("toolbar_frame");
    addPanelElevation(toolbarFrame);
    auto* toolbarLayout = new QGridLayout(toolbarFrame);
    toolbarLayout->setHorizontalSpacing(10);
    toolbarLayout->setVerticalSpacing(0);
    toolbarLayout->setColumnStretch(0, 1);

    auto* toolbarActions = new QWidget(toolbarFrame);
    toolbarActions->setObjectName("toolbar_actions");
    auto* toolbar = new QHBoxLayout(toolbarActions);
    toolbar->setContentsMargins(0, 0, 0, 0);
    toolbar->setSpacing(10);
    toolbarLayout->addWidget(toolbarActions, 0, 0);

    auto* themeLabel = new EngravedTextLabel(
        QString::fromUtf8("\xF0\x9F\x8E\xA8 Theme:")); // 🎨 Theme:
    themeLabel->setObjectName("toolbar_label");
    toolbar->addWidget(themeLabel);

    m_themeCombo = new EngravedComboBox();
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

    buildToolbarFilters(toolbar);

    toolbar->addWidget(new QLabel("  "));

    auto* randomBtn = new EngravedActionButton(
        "Random", EngravedActionButton::Glyph::Random);
    randomBtn->setObjectName("random_btn");
    randomBtn->setToolTip(QString("Launch random game (%1)").arg(
        QKeySequence(kRandomGameShortcut).toString(QKeySequence::NativeText)));
    connect(randomBtn, &QPushButton::clicked, this, &MainWindow::launchRandomGame);
    toolbar->addWidget(randomBtn);

    toolbar->addStretch();

    buildToolbarTools(toolbar);

    auto* settingsBtn = new EngravedActionButton(
        "Settings", EngravedActionButton::Glyph::Settings);
    settingsBtn->setObjectName("settings_btn");
    connect(settingsBtn, &QPushButton::clicked, this, &MainWindow::openSettings);
    toolbar->addWidget(settingsBtn);

    m_notificationsButton = new EngravedActionButton(
        "Alerts", EngravedActionButton::Glyph::Alerts);
    m_notificationsButton->setObjectName("alerts_btn");
    m_notificationsButton->setCursor(Qt::PointingHandCursor);
    connect(m_notificationsButton, &QPushButton::clicked,
            this, &MainWindow::openNotificationCenter);
    toolbar->addWidget(m_notificationsButton);
    updateNotificationsButton();

    auto* aboutBtn = new EngravedActionButton(
        "About", EngravedActionButton::Glyph::About, toolbarFrame);
    aboutBtn->setObjectName("about_btn");
    connect(aboutBtn, &QPushButton::clicked, this, &MainWindow::openAbout);
    toolbarLayout->addWidget(
        aboutBtn, 0, 1, Qt::AlignHCenter | Qt::AlignVCenter);

    m_updateAvailableButton = new QPushButton(toolbarFrame);
    m_updateAvailableButton->setObjectName("update_available_indicator");
    m_updateAvailableButton->setCursor(Qt::PointingHandCursor);
    m_updateAvailableButton->setFocusPolicy(Qt::StrongFocus);
    m_updateAvailableButton->setAccessibleDescription(
        "Open the available Goliath release details");
    m_updateAvailableButton->setSizePolicy(
        QSizePolicy::Maximum, QSizePolicy::Fixed);
    m_updateAvailableButton->hide();
    connect(m_updateAvailableButton, &QPushButton::clicked,
            this, &MainWindow::openAvailableUpdate);
    toolbarLayout->addWidget(
        m_updateAvailableButton, 1, 1, Qt::AlignHCenter | Qt::AlignTop);

    // Match the library's 8 px shadow room on the left and the details cards'
    // combined 12 px inset on the right, so the toolbar and content outlines
    // share the same visual axes.
    auto* toolbarRow = new QHBoxLayout();
    toolbarRow->setContentsMargins(8, 0, 12, 0);
    toolbarRow->setSpacing(0);
    toolbarRow->addWidget(toolbarFrame);
    mainLayout->addLayout(toolbarRow);

}


} // namespace goliath
