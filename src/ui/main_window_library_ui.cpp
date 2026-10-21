#include "ui/main_window.hpp"

#include "ui/application_shortcuts.hpp"
#include "ui/library_item_delegate.hpp"
#include "ui/widgets/main_window_panel_widgets.hpp"

#include <QAbstractItemView>
#include <QFrame>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QPushButton>
#include <QShortcut>
#include <QSize>
#include <QSplitter>
#include <QStackedWidget>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace goliath {

void MainWindow::buildLibraryPanel() {
    // Left: Game List + Search
    auto* leftPanel = new HatchedBackgroundWidget();
    auto* leftLayout = new QVBoxLayout(leftPanel);
    // Leave enough horizontal room for the elevated controls to paint their
    // shadows instead of clipping them at the splitter pane boundary.
    leftLayout->setContentsMargins(8, 0, 8, 12);
    leftLayout->setSpacing(10);

    auto* systemSelector = new QFrame();
    systemSelector->setObjectName("system_selector_frame");
    addPanelElevation(systemSelector);
    auto* systemLayout = new QHBoxLayout(systemSelector);
    systemLayout->setContentsMargins(2, 2, 2, 2);
    systemLayout->setSpacing(2);

    m_mvsAesButton = new QPushButton("Neo Geo MVS/AES");
    m_mvsAesButton->setObjectName("system_selector_button");
    m_mvsAesButton->setCheckable(true);
    m_mvsAesButton->setAutoExclusive(true);
    m_mvsAesButton->setChecked(m_librarySystem == "neogeo");
    connect(m_mvsAesButton, &QPushButton::clicked, this, [this] {
        setLibrarySystem("neogeo");
    });
    systemLayout->addWidget(m_mvsAesButton);

    m_cdButton = new QPushButton("Neo Geo CD");
    m_cdButton->setObjectName("system_selector_button");
    m_cdButton->setCheckable(true);
    m_cdButton->setAutoExclusive(true);
    m_cdButton->setChecked(m_librarySystem == "neogeocd");
    connect(m_cdButton, &QPushButton::clicked, this, [this] {
        setLibrarySystem("neogeocd");
    });
    systemLayout->addWidget(m_cdButton);

    leftLayout->addWidget(systemSelector);

    m_libraryViewStack = new QStackedWidget();
    m_libraryViewStack->setObjectName("library_view_stack");
    // Elevate the common container rather than its pages. QGraphicsEffect on
    // a child page is clipped by QStackedWidget, which made the library shadow
    // disappear after List/Grid/Big Grid Icons were introduced.
    addPanelElevation(m_libraryViewStack);

    m_tree = new QTreeWidget();
    m_tree->setProperty("goliathEngravedSurface", true);
    // The delegate paints continuous visible-row stripes and also paints the
    // personal-library badges in the title column.
    m_tree->setItemDelegate(new LibraryItemDelegate(m_tree));
    m_tree->setColumnCount(2);
    m_tree->setHeaderHidden(true);
    // Both columns are constrained to the viewport and long titles are
    // elided. Suppress the transient horizontal scrollbar that Qt can expose
    // while the CD-only content-sized badge column is being shown.
    m_tree->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
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
    m_tree->setAlternatingRowColors(false);
    m_tree->setIndentation(18);
    connect(m_tree, &QTreeWidget::itemSelectionChanged, this, &MainWindow::updateSelection);
    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, &MainWindow::launchSelectedItem);
    connect(m_tree, &QTreeWidget::itemExpanded, this,
            [this](QTreeWidgetItem*) {
                refreshLibraryListRowBackgrounds();
            });
    connect(m_tree, &QTreeWidget::itemCollapsed, this,
            [this](QTreeWidgetItem*) {
                refreshLibraryListRowBackgrounds();
                ensureVisibleSelection(false);
            });
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tree, &QTreeWidget::customContextMenuRequested, this, &MainWindow::showTreeContextMenu);
    m_libraryViewStack->addWidget(m_tree);

    m_iconView = new QListWidget();
    m_iconView->setObjectName("library_icon_view");
    m_iconView->setProperty("goliathEngravedSurface", true);
    m_libraryTileDelegate = new LibraryTileDelegate(m_iconView);
    m_iconView->setItemDelegate(m_libraryTileDelegate);
    m_iconView->setViewMode(QListView::IconMode);
    m_iconView->setResizeMode(QListView::Adjust);
    m_iconView->setMovement(QListView::Static);
    m_iconView->setWrapping(true);
    m_iconView->setUniformItemSizes(true);
    m_iconView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_iconView->setTextElideMode(Qt::ElideRight);
    m_iconView->setSpacing(4);
    m_iconView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_iconView, &QListWidget::currentItemChanged, this,
            [this](QListWidgetItem* current, QListWidgetItem*) {
                selectIconItem(current);
            });
    connect(m_iconView, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem* item) {
                selectIconItem(item);
                launchSelected();
            });
    connect(m_iconView, &QListWidget::customContextMenuRequested,
            this, &MainWindow::showIconContextMenu);
    m_libraryViewStack->addWidget(m_iconView);
    leftLayout->addWidget(m_libraryViewStack, 1);
    setLibraryDisplayMode(m_libraryDisplayMode, false);

    m_searchEntry = new QLineEdit();
    addPanelElevation(m_searchEntry);
    m_searchEntry->setPlaceholderText(QString::fromUtf8("\xF0\x9F\x94\x8D Search games...")); // 🔍
    m_searchEntry->setClearButtonEnabled(true);
    m_searchEntry->setToolTip(QString(
        "Filter the current library. Use the clear button or press %1 "
        "to restore the complete list. %2 focuses and selects the current query.")
        .arg(QKeySequence(kClearSearchShortcut).toString(QKeySequence::NativeText),
             QKeySequence(kFocusSearchShortcut).toString(QKeySequence::NativeText)));
    connect(m_searchEntry, &QLineEdit::textChanged, this, &MainWindow::filterGames);

    auto* clearSearchShortcut =
        new QShortcut(QKeySequence(kClearSearchShortcut), m_searchEntry);
    clearSearchShortcut->setContext(Qt::WidgetShortcut);
    connect(clearSearchShortcut, &QShortcut::activated,
            m_searchEntry, &QLineEdit::clear);
    leftLayout->addWidget(m_searchEntry);

    m_splitter->addWidget(leftPanel);

}

void MainWindow::buildLibraryWorkspace(QVBoxLayout* mainLayout) {
    // --- Splitter ---
    m_splitter = new QSplitter(Qt::Horizontal);
    m_splitter->setObjectName("library_details_splitter");
    m_splitter->setHandleWidth(6);
    // The details pane contains several elevated cards, rich text, and media.
    // Resize them once on release instead of repainting the complete workspace
    // for every pixel traversed while dragging the splitter handle.
    m_splitter->setOpaqueResize(false);
    mainLayout->addWidget(m_splitter, 1);

    buildLibraryPanel();

    buildDetailsPanel();

    m_splitter->setCollapsible(0, false);
    // resizeEvent() deliberately collapses the details pane below 1000 px to
    // enter Classic View, then restores it above 1150 px.
    m_splitter->setCollapsible(1, true);

    m_splitter->setSizes({400, 760});
    connect(m_splitter, &QSplitter::splitterMoved, this,
            [this](int, int) {
                updateIconViewGridSize();
                updateDetailsLayoutMode();
            });

}


} // namespace goliath
