#include "ui/main_window.hpp"

#include "ui/application_shortcuts.hpp"
#include "ui/widgets/main_window_panel_widgets.hpp"
#include "ui/widgets/title_bar.hpp"

#include <QKeySequence>
#include <QLabel>
#include <QPushButton>
#include <QShortcut>
#include <QStatusBar>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace goliath {

void MainWindow::buildStatusBar() {
    // Status bar with Launch aligned to the right edge of the detail cards.
    auto* statusBar_ = statusBar();
    // ResizeFilter already provides all-edge resizing for this frameless
    // window, so the native QSizeGrip would only add a stray corner square.
    statusBar_->setSizeGripEnabled(false);
    statusBar_->setContentsMargins(0, 0, 28, 0);
    m_libraryStatusLabel = new QLabel(statusBar_);
    m_libraryStatusLabel->setObjectName("library_status");
    m_libraryStatusLabel->setContentsMargins(4, 0, 0, 0);
    statusBar_->addWidget(m_libraryStatusLabel, 1);
    m_launchButton = new QPushButton(QString::fromUtf8("\xE2\x96\xB6 Launch")); // ▶ Launch
    m_launchButton->setObjectName("launch_btn");
    m_launchButton->setToolTip(QString("Launch selected game (%1)").arg(
        QKeySequence(kLaunchSelectedShortcut).toString(QKeySequence::NativeText)));
    connect(m_launchButton, &QPushButton::clicked,
            this, &MainWindow::launchSelected);
    statusBar_->addPermanentWidget(m_launchButton);
    setSelectionActionsEnabled(false);

}

void MainWindow::setupKeyboardShortcuts() {
    // Keyboard shortcuts
    connect(new QShortcut(QKeySequence(kRescanRomsShortcut), this),
            &QShortcut::activated, this, &MainWindow::rescanRoms);
    connect(new QShortcut(QKeySequence(kVerifyBiosShortcut), this),
            &QShortcut::activated, this, &MainWindow::verifyBios);
    connect(new QShortcut(QKeySequence(kFocusSearchShortcut), this),
            &QShortcut::activated, this, &MainWindow::focusSearch);
    connect(new QShortcut(QKeySequence(kRandomGameShortcut), this),
            &QShortcut::activated, this, &MainWindow::launchRandomGame);
    connect(new QShortcut(QKeySequence(kExpandAllShortcut), this),
            &QShortcut::activated, this, &MainWindow::expandAll);
    connect(new QShortcut(QKeySequence(kCollapseAllShortcut), this),
            &QShortcut::activated, this, &MainWindow::collapseAll);

    auto* launchShortcut =
        new QShortcut(QKeySequence(kLaunchSelectedShortcut), m_tree);
    launchShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(launchShortcut, &QShortcut::activated, this, &MainWindow::launchSelected);
}

void MainWindow::buildUi() {
    auto* central = new QWidget(this);
    setCentralWidget(central);
    auto* outerLayout = new QVBoxLayout(central);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    // --- Custom Title Bar ---
    m_titleBar = new TitleBar(this);
    outerLayout->addWidget(m_titleBar);

    auto* contentWidget = new HatchedBackgroundWidget();
    outerLayout->addWidget(contentWidget, 1);
    auto* mainLayout = new QVBoxLayout(contentWidget);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    buildToolbar(mainLayout);

    buildLibraryWorkspace(mainLayout);

    buildStatusBar();

    setupKeyboardShortcuts();
}

} // namespace goliath
