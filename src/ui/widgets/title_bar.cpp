#include "title_bar.hpp"

#include "common/window_geometry.hpp"

#include <QAbstractItemView>
#include <QAbstractSlider>
#include <QApplication>
#include <QComboBox>
#include <QCursor>
#include <QDialog>
#include <QEvent>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPointer>
#include <QPushButton>
#include <QScreen>
#include <QSizeGrip>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QWindow>

namespace goliath {

namespace {

// Keeps a modal frameless dialog inside the active monitor's work area. The
// first show is centered on its parent's monitor; later layout/work-area or
// monitor changes preserve the user's position whenever possible.
class DialogGeometryGuard final : public QObject {
public:
    explicit DialogGeometryGuard(QDialog* dialog)
        : QObject(dialog), m_dialog(dialog) {
        dialog->installEventFilter(this);
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (watched != m_dialog) return false;

        if (event->type() == QEvent::Show) {
            if (auto* layout = m_dialog->layout()) layout->activate();
            connectWindowHandle();
            setScreen(initialScreen());
            fitToScreen(true);
            scheduleFit(false);
        } else if ((event->type() == QEvent::LayoutRequest ||
                    event->type() == QEvent::Resize) &&
                   m_dialog->isVisible()) {
            scheduleFit(false);
        }
        return false;
    }

private:
    QScreen* initialScreen() const {
        if (auto* parent = m_dialog->parentWidget()) {
            if (QScreen* screen = parent->screen()) return screen;
        }
        if (QScreen* screen = m_dialog->screen()) return screen;
        return QGuiApplication::primaryScreen();
    }

    void connectWindowHandle() {
        if (m_windowConnected) return;
        QWindow* handle = m_dialog->windowHandle();
        if (!handle) return;

        connect(handle, &QWindow::screenChanged, this,
                [this](QScreen* screen) {
                    setScreen(screen);
                    scheduleFit(false);
                });
        m_windowConnected = true;
    }

    void setScreen(QScreen* screen) {
        if (!screen) screen = QGuiApplication::primaryScreen();
        if (screen == m_screen) return;

        if (m_availableGeometryConnection) {
            disconnect(m_availableGeometryConnection);
        }
        m_screen = screen;
        if (m_screen) {
            m_availableGeometryConnection = connect(
                m_screen, &QScreen::availableGeometryChanged, this,
                [this](const QRect&) { scheduleFit(false); });
        }
    }

    void scheduleFit(bool center) {
        m_centerPending = m_centerPending || center;
        if (m_fitPending) return;

        m_fitPending = true;
        QTimer::singleShot(0, this, [this]() {
            const bool centerNow = m_centerPending;
            m_centerPending = false;
            m_fitPending = false;
            if (m_dialog->isVisible()) fitToScreen(centerNow);
        });
    }

    void fitToScreen(bool center) {
        if (!m_screen || m_dialog->isMaximized() ||
            m_dialog->isMinimized()) {
            return;
        }

        const QRect available = m_screen->availableGeometry();
        const QRect target = center
            ? centeredWindowGeometry(m_dialog->size(), available)
            : constrainedWindowGeometry(m_dialog->frameGeometry(), available);
        if (m_dialog->size() != target.size()) m_dialog->resize(target.size());
        if (m_dialog->frameGeometry().topLeft() != target.topLeft())
            m_dialog->move(target.topLeft());
    }

    QDialog* m_dialog = nullptr;
    QPointer<QScreen> m_screen;
    QMetaObject::Connection m_availableGeometryConnection;
    bool m_windowConnected = false;
    bool m_fitPending = false;
    bool m_centerPending = false;
};

} // namespace

TitleBar::TitleBar(QWidget* window, bool showMinMax)
    : QFrame(window), m_window(window) {
    setObjectName("title_bar");
    setFixedHeight(40);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(14, 0, 8, 0);
    layout->setSpacing(8);

    // Same branded icon as the .exe/taskbar and every other top-level
    // window (set app-wide via QApplication::setWindowIcon() in main.cpp).
    auto* icon = new QLabel();
    icon->setObjectName("titlebar_icon");
    icon->setPixmap(QIcon(":/icons/goliath-qt.ico").pixmap(20, 20));
    layout->addWidget(icon);

    m_titleLabel = new QLabel(window->windowTitle());
    m_titleLabel->setObjectName("titlebar_title");
    layout->addWidget(m_titleLabel);

    layout->addStretch();

    if (showMinMax) {
        m_minBtn = new QPushButton(QString::fromUtf8("\xE2\x94\x80")); // ─
        m_minBtn->setObjectName("titlebar_btn");
        m_minBtn->setToolTip("Minimize");
        connect(m_minBtn, &QPushButton::clicked, window, &QWidget::showMinimized);

        m_maxBtn = new QPushButton(QString::fromUtf8("\xE2\x98\x90")); // ☐
        m_maxBtn->setObjectName("titlebar_btn");
        m_maxBtn->setToolTip("Maximize");
        connect(m_maxBtn, &QPushButton::clicked, this, &TitleBar::toggleMaximize);
    }

    m_closeBtn = new QPushButton(QString::fromUtf8("\xE2\x9C\x95")); // ✕
    m_closeBtn->setObjectName("titlebar_close");
    m_closeBtn->setToolTip("Close");
    connect(m_closeBtn, &QPushButton::clicked, window, &QWidget::close);

    for (QPushButton* btn : {m_minBtn, m_maxBtn, m_closeBtn}) {
        if (!btn) continue;
        btn->setFixedSize(44, 30);
        btn->setFocusPolicy(Qt::NoFocus);
        layout->addWidget(btn);
    }
}

void TitleBar::toggleMaximize() {
    if (m_window->isMaximized()) {
        m_window->showNormal();
    } else {
        m_window->showMaximized();
    }
    updateMaxButton();
}

void TitleBar::updateMaxButton() {
    if (!m_maxBtn) return;
    if (m_window->isMaximized()) {
        m_maxBtn->setText(QString::fromUtf8("\xE2\x9D\x90")); // ❐
        m_maxBtn->setToolTip("Restore");
    } else {
        m_maxBtn->setText(QString::fromUtf8("\xE2\x98\x90")); // ☐
        m_maxBtn->setToolTip("Maximize");
    }
}

void TitleBar::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        QWindow* handle = m_window->windowHandle();
        if (handle != nullptr) {
            handle->startSystemMove();
            event->accept();
            return;
        }
    }
    QFrame::mousePressEvent(event);
}

void TitleBar::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        if (m_maxBtn) toggleMaximize();
        event->accept();
        return;
    }
    QFrame::mouseDoubleClickEvent(event);
}

constexpr int RESIZE_BORDER = 6;

ResizeFilter::ResizeFilter(QWidget* window)
    : QObject(window), m_window(window) {
    m_cursorTimer = new QTimer(this);
    m_cursorTimer->setInterval(100);
    connect(m_cursorTimer, &QTimer::timeout, this, [this]() { updateCursor(); });
    m_cursorTimer->start();
    if (qApp) qApp->installEventFilter(this);
}

ResizeFilter::~ResizeFilter() {
    m_cursorTimer->stop();
    if (qApp) qApp->removeEventFilter(this);
    if (m_overrideCursor) QApplication::restoreOverrideCursor();
}

bool ResizeFilter::eventFilter(QObject* obj, QEvent* event) {
    if (!m_window || m_window->isMaximized() || !m_window->isActiveWindow())
        return false;

    if (event->type() != QEvent::MouseButtonPress)
        return false;

    QMouseEvent* me = static_cast<QMouseEvent*>(event);
    if (me->button() != Qt::LeftButton)
        return false;

    // Let interactive children (buttons, inputs, the native QSizeGrip) keep
    // their mouse events. We only resize on plain window chrome / bars.
    if (isInteractiveControl(obj))
        return false;

    Qt::Edges edges = resizeEdges(me->globalPosition().toPoint());
    if (!edges) return false;

    QWindow* wh = m_window->windowHandle();
    if (!wh) return false;

    if (wh->startSystemResize(edges)) {
        return true;
    }
    return false;
}

bool ResizeFilter::isInteractiveControl(QObject* obj) {
    return obj->isWidgetType() &&
           (obj->inherits("QAbstractButton") ||
            obj->inherits("QLineEdit") ||
            obj->inherits("QTextEdit") ||
            obj->inherits("QComboBox") ||
            obj->inherits("QAbstractSlider") ||
            obj->inherits("QSizeGrip"));
}

Qt::Edges ResizeFilter::resizeEdges(const QPoint& globalPos) const {
    QRect geo = m_window->frameGeometry();
    if (!geo.contains(globalPos)) return Qt::Edges();
    int x = globalPos.x() - geo.x();
    int y = globalPos.y() - geo.y();
    int w = geo.width();
    int h = geo.height();

    Qt::Edges edges;
    if (x < RESIZE_BORDER) edges |= Qt::LeftEdge;
    if (x >= w - RESIZE_BORDER) edges |= Qt::RightEdge;
    if (y < RESIZE_BORDER) edges |= Qt::TopEdge;
    if (y >= h - RESIZE_BORDER) edges |= Qt::BottomEdge;
    return edges;
}

void ResizeFilter::updateCursor() {
    if (!m_window || m_window->isMaximized() || !m_window->isActiveWindow()) {
        if (m_overrideCursor) { QApplication::restoreOverrideCursor(); m_overrideCursor = false; }
        return;
    }

    Qt::Edges edges = resizeEdges(QCursor::pos());
    if (!edges) {
        if (m_overrideCursor) { QApplication::restoreOverrideCursor(); m_overrideCursor = false; }
        return;
    }

    QWidget* w = QApplication::widgetAt(QCursor::pos());
    if (w && isInteractiveControl(w)) {
        if (m_overrideCursor) { QApplication::restoreOverrideCursor(); m_overrideCursor = false; }
        return;
    }

    QCursor c;
    if (edges == (Qt::LeftEdge | Qt::TopEdge) ||
        edges == (Qt::RightEdge | Qt::BottomEdge))
        c = QCursor(Qt::SizeFDiagCursor);
    else if (edges == (Qt::RightEdge | Qt::TopEdge) ||
             edges == (Qt::LeftEdge | Qt::BottomEdge))
        c = QCursor(Qt::SizeBDiagCursor);
    else if (edges == Qt::LeftEdge || edges == Qt::RightEdge)
        c = QCursor(Qt::SizeHorCursor);
    else if (edges == Qt::TopEdge || edges == Qt::BottomEdge)
        c = QCursor(Qt::SizeVerCursor);
    else
        return;

    if (m_overrideCursor) {
        QApplication::changeOverrideCursor(c);
    } else {
        QApplication::setOverrideCursor(c);
        m_overrideCursor = true;
    }
}

void setupFramelessDialog(QDialog* dialog, const QString& title,
                          QVBoxLayout** contentLayout, bool resizable,
                          bool showMinMax) {
    if (!dialog) return;

    dialog->setWindowTitle(title);
    dialog->setObjectName("frameless_window");
    dialog->setWindowFlags(dialog->windowFlags() | Qt::FramelessWindowHint);

    // Inherit the main window stylesheet so the title bar/cursor keep the theme.
    if (auto* pw = dialog->parentWidget(); pw && !pw->styleSheet().isEmpty())
        dialog->setStyleSheet(pw->styleSheet());

    auto* outerLayout = new QVBoxLayout(dialog);
    outerLayout->setContentsMargins(1, 1, 1, 1);
    outerLayout->setSpacing(0);

    outerLayout->addWidget(new TitleBar(dialog, showMinMax));

    auto* content = new QWidget(dialog);
    content->setObjectName("dialog_content");
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);
    outerLayout->addWidget(content, 1);

    if (resizable)
        new ResizeFilter(dialog);

    new DialogGeometryGuard(dialog);

    if (contentLayout)
        *contentLayout = layout;
}

void applyComboPopupStyle(QComboBox* combo, const QString& styleSheet) {
    if (!combo || !combo->view()) return;

    QAbstractItemView* view = combo->view();
    QWidget* container = view->window();
    if (container) {
        container->setObjectName("combo_popup_container");
        container->setAttribute(Qt::WA_StyledBackground, true);
        container->setStyleSheet(styleSheet);
    }
    view->setStyleSheet(styleSheet);
}

void applyComboPopupStyleToDescendants(QWidget* root,
                                       const QString& styleSheet) {
    if (!root) return;
    const auto combos = root->findChildren<QComboBox*>();
    for (QComboBox* combo : combos) {
        applyComboPopupStyle(combo, styleSheet);
    }
}

} // namespace goliath
