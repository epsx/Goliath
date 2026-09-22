#include "ui/command_dialog.hpp"

#include "ui/widgets/command_notation_view.hpp"
#include "ui/widgets/title_bar.hpp"

#include <QCheckBox>
#include <QEvent>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QScreen>
#include <QShortcut>
#include <QSlider>
#include <QStackedWidget>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <QWindow>

#include <algorithm>

namespace goliath {

CommandDialog::CommandDialog(const QString& gameTitle,
                             const QString& matchedMameId,
                             const QString& commandText,
                             const QString& inheritedStyleSheet)
    : QDialog(nullptr),
      m_inheritedStyleSheet(inheritedStyleSheet),
      m_commandLines(commandText.split('\n', Qt::KeepEmptyParts)) {
    setStyleSheet(inheritedStyleSheet);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowFlag(Qt::WindowStaysOnTopHint, true);

    QVBoxLayout* layout = nullptr;
    setupFramelessDialog(
        this, QString("Command List — %1").arg(gameTitle),
        &layout, true, false, false);
    layout->setContentsMargins(10, 8, 10, 10);
    layout->setSpacing(6);

    auto* controlBar = new QWidget(this);
    controlBar->setObjectName("command_control_bar");
    controlBar->setCursor(Qt::OpenHandCursor);
    controlBar->installEventFilter(this);
    auto* controls = new QHBoxLayout(controlBar);
    controls->setContentsMargins(4, 2, 4, 2);
    controls->setSpacing(9);

    auto* romId = new QLabel(QString("ROM ID: %1").arg(matchedMameId),
                             controlBar);
    romId->setObjectName("command_drag_handle");
    romId->setCursor(Qt::OpenHandCursor);
    romId->setToolTip("Drag the overlay. Alt+F4 closes it.");
    romId->installEventFilter(this);
    controls->addWidget(romId);
    controls->addStretch();

    auto* visualNotation = new QCheckBox("Visual", controlBar);
    visualNotation->setChecked(true);
    visualNotation->setToolTip(
        "Render supported command.dat markers as scalable directions and "
        "buttons");
    controls->addWidget(visualNotation);

    m_overlayMode = new QCheckBox("Transparent", controlBar);
    m_overlayMode->setChecked(true);
    m_overlayMode->setToolTip("Use the compact transparent overlay style");
    controls->addWidget(m_overlayMode);

    auto* keepAbove = new QCheckBox("Keep above", controlBar);
    keepAbove->setChecked(true);
    controls->addWidget(keepAbove);

    m_backgroundLabel = new QLabel("72%", controlBar);
    m_backgroundLabel->setMinimumWidth(32);
    m_backgroundLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    controls->addWidget(m_backgroundLabel);

    m_backgroundSlider = new QSlider(Qt::Horizontal, controlBar);
    m_backgroundSlider->setObjectName("command_opacity_slider");
    m_backgroundSlider->setRange(35, 95);
    m_backgroundSlider->setValue(72);
    m_backgroundSlider->setFixedWidth(96);
    m_backgroundSlider->setToolTip(
        "Adjust only the overlay background; text and glyphs stay opaque");
    controls->addWidget(m_backgroundSlider);
    layout->addWidget(controlBar);

    m_findBar = new QWidget(this);
    m_findBar->setObjectName("command_find_bar");
    auto* findLayout = new QHBoxLayout(m_findBar);
    findLayout->setContentsMargins(8, 3, 8, 3);
    findLayout->setSpacing(7);

    m_findEdit = new QLineEdit(m_findBar);
    m_findEdit->setObjectName("command_find_edit");
    m_findEdit->setPlaceholderText("Find character or command…");
    m_findEdit->setFixedWidth(250);
    m_findEdit->setToolTip(
        "Enter: next match · Shift+Enter: previous match · Esc: close");
    m_findEdit->installEventFilter(this);
    findLayout->addWidget(m_findEdit);

    m_findStatus = new QLabel(m_findBar);
    m_findStatus->setObjectName("command_find_status");
    m_findStatus->setMinimumWidth(44);
    m_findStatus->setAlignment(Qt::AlignCenter);
    findLayout->addWidget(m_findStatus);

    m_findBar->hide();
    layout->addWidget(m_findBar, 0, Qt::AlignLeft);

    m_visualView = new CommandNotationView(this);
    m_visualView->setCommandText(commandText);

    m_rawView = new QPlainTextEdit(this);
    m_rawView->setObjectName("command_raw_view");
    m_rawView->setReadOnly(true);
    m_rawView->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_rawView->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_rawView->setPlainText(commandText);
    m_rawView->moveCursor(QTextCursor::Start);

    m_viewStack = new QStackedWidget(this);
    m_viewStack->setObjectName("command_view_stack");
    m_viewStack->addWidget(m_visualView);
    m_viewStack->addWidget(m_rawView);
    layout->addWidget(m_viewStack, 1);

    connect(visualNotation, &QCheckBox::toggled, this,
            [this](bool enabled) {
                m_viewStack->setCurrentIndex(enabled ? 0 : 1);
                if (m_findMatchIndex >= 0) revealFindMatch();
            });

    connect(m_findEdit, &QLineEdit::textChanged, this,
            [this]() { rebuildFindMatches(); });

    auto* findShortcut = new QShortcut(QKeySequence::Find, this);
    findShortcut->setContext(Qt::WindowShortcut);
    connect(findShortcut, &QShortcut::activated, this,
            [this]() { showFindBar(); });

    connect(m_overlayMode, &QCheckBox::toggled, this,
            [this](bool enabled) {
                m_backgroundSlider->setEnabled(enabled);
                applyPresentation();
            });

    connect(m_backgroundSlider, &QSlider::valueChanged, this,
            [this](int value) {
                m_backgroundLabel->setText(
                    QString("%1%").arg(value));
                applyPresentation();
            });

    connect(keepAbove, &QCheckBox::toggled, this, [this](bool enabled) {
        const QRect oldGeometry = geometry();
        setWindowFlag(Qt::WindowStaysOnTopHint, enabled);
        show();
        setGeometry(oldGeometry);
        QTimer::singleShot(0, this, [this, oldGeometry]() {
            setGeometry(oldGeometry);
        });
    });

    applyPresentation();
    setMinimumSize(600, 420);
    resize(680, 760);
}

bool CommandDialog::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_findEdit && event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Escape) {
            hideFindBar();
            event->accept();
            return true;
        }
        if (keyEvent->key() == Qt::Key_Return ||
            keyEvent->key() == Qt::Key_Enter) {
            findNext(keyEvent->modifiers().testFlag(Qt::ShiftModifier));
            event->accept();
            return true;
        }
    }

    if ((watched->objectName() == "command_control_bar" ||
         watched->objectName() == "command_drag_handle") &&
        event->type() == QEvent::MouseButtonPress) {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            if (QWindow* handle = windowHandle()) {
                handle->startSystemMove();
                event->accept();
                return true;
            }
        }
    }
    return QDialog::eventFilter(watched, event);
}

void CommandDialog::showFindBar() {
    m_findBar->show();
    m_findEdit->setFocus(Qt::ShortcutFocusReason);
    m_findEdit->selectAll();
}

void CommandDialog::hideFindBar() {
    m_findBar->hide();
    m_findEdit->clear();
    m_visualView->clearFindHighlight();
    QTextCursor cursor = m_rawView->textCursor();
    cursor.clearSelection();
    m_rawView->setTextCursor(cursor);
    m_viewStack->currentWidget()->setFocus(Qt::ShortcutFocusReason);
}

void CommandDialog::rebuildFindMatches() {
    m_findMatches.clear();
    m_findMatchIndex = -1;

    const QString query = m_findEdit->text().trimmed();
    if (query.isEmpty()) {
        m_findStatus->clear();
        m_visualView->clearFindHighlight();
        QTextCursor cursor = m_rawView->textCursor();
        cursor.clearSelection();
        m_rawView->setTextCursor(cursor);
        return;
    }

    for (qsizetype line = 0; line < m_commandLines.size(); ++line) {
        if (m_commandLines[line].contains(query, Qt::CaseInsensitive))
            m_findMatches.push_back(static_cast<int>(line));
    }

    if (m_findMatches.empty()) {
        m_findStatus->setText("0/0");
        m_visualView->clearFindHighlight();
        QTextCursor cursor = m_rawView->textCursor();
        cursor.clearSelection();
        m_rawView->setTextCursor(cursor);
        return;
    }

    m_findMatchIndex = 0;
    revealFindMatch();
}

void CommandDialog::findNext(bool backwards) {
    if (m_findMatches.empty()) {
        rebuildFindMatches();
        return;
    }

    const int count = static_cast<int>(m_findMatches.size());
    m_findMatchIndex = backwards
        ? (m_findMatchIndex + count - 1) % count
        : (m_findMatchIndex + 1) % count;
    revealFindMatch();
}

void CommandDialog::revealFindMatch() {
    if (m_findMatchIndex < 0 ||
        m_findMatchIndex >= static_cast<int>(m_findMatches.size())) {
        return;
    }

    m_findStatus->setText(QString("%1/%2")
        .arg(m_findMatchIndex + 1)
        .arg(static_cast<int>(m_findMatches.size())));
    const int sourceLine = m_findMatches[
        static_cast<std::size_t>(m_findMatchIndex)];
    m_visualView->revealSourceLine(sourceLine);

    const QTextBlock block = m_rawView->document()->findBlockByNumber(
        sourceLine);
    if (block.isValid()) {
        QTextCursor cursor(m_rawView->document());
        cursor.setPosition(block.position() +
                           std::max(0, block.length() - 1));
        cursor.setPosition(block.position(), QTextCursor::KeepAnchor);
        m_rawView->setTextCursor(cursor);
        m_rawView->ensureCursorVisible();
    }
}

void CommandDialog::applyPresentation() {
    const bool overlayEnabled = m_overlayMode && m_overlayMode->isChecked();
    if (m_visualView)
        m_visualView->setOverlayPresentation(overlayEnabled);

    if (!overlayEnabled) {
        setStyleSheet(m_inheritedStyleSheet);
        return;
    }

    const int percent = m_backgroundSlider
        ? m_backgroundSlider->value() : 72;
    const int alpha = (255 * percent + 50) / 100;
    const int panelAlpha = std::clamp(alpha - 24, 48, 231);
    const int viewAlpha = std::clamp(alpha - 58, 28, 197);

    const QString overlayStyle = QString(R"(
QDialog#frameless_window {
    background-color: transparent;
    border: 1px solid rgba(216, 224, 255, 150);
}
QWidget#dialog_content {
    background-color: rgba(8, 10, 22, %2);
    color: #f5f7ff;
}
QWidget#command_control_bar {
    background-color: rgba(8, 10, 22, %1);
    border-bottom: 1px solid rgba(216, 224, 255, 90);
}
QLabel#command_drag_handle {
    color: #ffffff;
    font-weight: 600;
}
QWidget#dialog_content QLabel,
QWidget#dialog_content QCheckBox {
    background-color: transparent;
    color: #f5f7ff;
}
QWidget#command_view_stack {
    background-color: transparent;
}
QWidget#command_find_bar {
    background-color: rgba(8, 10, 22, %1);
    border: 1px solid rgba(216, 224, 255, 95);
    border-radius: 5px;
}
QLineEdit#command_find_edit {
    color: #f5f7ff;
    background-color: transparent;
    border: none;
    border-bottom: 1px solid rgba(216, 224, 255, 155);
    padding: 2px 3px;
}
QLabel#command_find_status {
    color: #9db7ff;
    background-color: transparent;
    font-weight: 600;
}
QAbstractScrollArea#command_visual_view,
QPlainTextEdit#command_raw_view {
    background-color: rgba(0, 0, 0, %3);
    color: #ffffff;
    border: 1px solid rgba(216, 224, 255, 105);
    border-radius: 6px;
    selection-background-color: rgba(82, 132, 255, 185);
}
QAbstractScrollArea#command_visual_view QWidget#qt_scrollarea_viewport {
    background-color: transparent;
}
QAbstractScrollArea#command_visual_view QScrollBar:vertical {
    width: 7px;
    margin: 0;
    background: transparent;
}
QAbstractScrollArea#command_visual_view QScrollBar::handle:vertical {
    min-height: 28px;
    border: none;
    border-radius: 3px;
    background: rgba(245, 247, 255, 95);
}
QAbstractScrollArea#command_visual_view QScrollBar::add-line:vertical,
QAbstractScrollArea#command_visual_view QScrollBar::sub-line:vertical {
    height: 0;
    background: transparent;
}
QAbstractScrollArea#command_visual_view QScrollBar::add-page:vertical,
QAbstractScrollArea#command_visual_view QScrollBar::sub-page:vertical {
    background: transparent;
}
QSlider#command_opacity_slider {
    background-color: transparent;
}
QSlider#command_opacity_slider::groove:horizontal {
    height: 5px;
    background: rgba(255, 255, 255, 65);
    border-radius: 2px;
}
QSlider#command_opacity_slider::handle:horizontal {
    width: 14px;
    margin: -5px 0;
    background: #f5f7ff;
    border: 1px solid rgba(0, 0, 0, 150);
    border-radius: 7px;
}
)")
        .arg(alpha)
        .arg(panelAlpha)
        .arg(viewAlpha);
    setStyleSheet(m_inheritedStyleSheet + "\n" + overlayStyle);
}

void CommandDialog::placeBeside(const QWidget* reference, int cascadeIndex) {
    QScreen* targetScreen = reference ? reference->screen() : nullptr;
    if (!targetScreen) targetScreen = QGuiApplication::primaryScreen();
    if (!targetScreen) return;

    const QRect available = targetScreen->availableGeometry();
    resize(std::min(width(), available.width()),
           std::min(height(), available.height()));

    int x = available.right() - width() + 1;
    int y = available.top() + (available.height() - height()) / 2;
    if (reference) {
        const QRect anchor = reference->frameGeometry();
        const int right = anchor.right() + 12;
        const int left = anchor.left() - width() - 12;
        if (right + width() - 1 <= available.right()) {
            x = right;
        } else if (left >= available.left()) {
            x = left;
        }
        y = std::clamp(anchor.top(), available.top(),
                       available.bottom() - height() + 1);
    }
    const int cascade = std::max(0, cascadeIndex % 6) * 24;
    const int maximumX = std::max(
        available.left(), available.right() - width() + 1);
    const int maximumY = std::max(
        available.top(), available.bottom() - height() + 1);
    x = std::clamp(x - cascade, available.left(), maximumX);
    y = std::clamp(y + cascade, available.top(), maximumY);
    move(x, y);
}

} // namespace goliath
