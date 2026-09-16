// title_bar.hpp — the custom (frameless-window) title bar: app icon/title,
// minimize/maximize/close buttons, and window dragging.
#pragma once

#include <QFrame>
#include <QObject>
#include <QPoint>
#include <QString>
#include <QtCore/qnamespace.h>

class QDialog;
class QEvent;
class QLabel;
class QMouseEvent;
class QPushButton;
class QComboBox;
class QTimer;
class QVBoxLayout;
class QWidget;

namespace goliath {

// Title bar for any frameless top-level widget (QMainWindow or QDialog).
// Optionally hides the minimize/maximize buttons (e.g. for modal dialogs).
class TitleBar : public QFrame {
    Q_OBJECT
public:
    explicit TitleBar(QWidget* window, bool showMinMax = true);

    void updateMaxButton();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private slots:
    void toggleMaximize();

private:
    QWidget* m_window;
    QLabel* m_titleLabel;
    QPushButton* m_minBtn = nullptr;
    QPushButton* m_maxBtn = nullptr;
    QPushButton* m_closeBtn;
};

// Tracks the cursor near the window borders for a frameless widget and
// delegates left-click drags to QWindow::startSystemResize().
class ResizeFilter : public QObject {
public:
    explicit ResizeFilter(QWidget* window);
    ~ResizeFilter() override;

    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    static bool isInteractiveControl(QObject* obj);
    Qt::Edges resizeEdges(const QPoint& globalPos) const;
    void updateCursor();

    QWidget* m_window = nullptr;
    QTimer* m_cursorTimer = nullptr;
    bool m_overrideCursor = false;
};

// Makes a QDialog frameless, gives it a themed perimeter, a modern title bar,
// and an all-side resize filter. Returns the content layout (with 16px margins)
// where the caller should add its widgets.
void setupFramelessDialog(QDialog* dialog, const QString& title,
                          QVBoxLayout** contentLayout = nullptr,
                          bool resizable = true, bool showMinMax = true);

// Apply the generated palette to both a combo's item view and the separate
// native popup container created by Qt on Windows. The descendant overload is
// intended for dialogs that own several QComboBox controls.
void applyComboPopupStyle(QComboBox* combo, const QString& styleSheet);
void applyComboPopupStyleToDescendants(QWidget* root,
                                       const QString& styleSheet);

} // namespace goliath
