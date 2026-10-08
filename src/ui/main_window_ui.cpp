#include "ui/main_window.hpp"
#include "ui/application_shortcuts.hpp"
#include "ui/gallery_frame.hpp"
#include "ui/library_item_delegate.hpp"

#include "common/theme.hpp"
#include "ui/widgets/title_bar.hpp"
#include "ui/lithogen_dialog.hpp"

#include <QAction>
#include <QActionGroup>
#include <QButtonGroup>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QDesktopServices>
#include <QFont>
#include <QFontMetrics>
#include <QFormLayout>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QGridLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QMenu>
#include <QModelIndex>
#include <QMouseEvent>
#include <QMovie>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPushButton>
#include <QPolygon>
#include <QPoint>
#include <QScrollArea>
#include <QShortcut>
#include <QSize>
#include <QSizePolicy>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QStyleOption>
#include <QStyleOptionButton>
#include <QStyleOptionComboBox>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QTextEdit>
#include <QTimer>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <string_view>
#include <utility>

namespace fs = std::filesystem;

namespace goliath {

namespace {

class HatchedBackgroundWidget final : public QWidget {
public:
    explicit HatchedBackgroundWidget(QWidget* parent = nullptr)
        : QWidget(parent) {
        setProperty("goliathHatchedBackground", true);
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QStyleOption option;
        option.initFrom(this);

        QPainter painter(this);
        style()->drawPrimitive(QStyle::PE_Widget, &option, &painter, this);

        const QColor hatchColor(
            property("goliathHatchColor").toString());
        if (!hatchColor.isValid()) return;

        painter.setRenderHint(QPainter::Antialiasing, false);
        painter.setPen(QPen(hatchColor, 1.0));
        constexpr int spacing = 14;
        const QPoint windowOrigin = mapTo(window(), QPoint(0, 0));
        const int originSum = windowOrigin.x() + windowOrigin.y();
        const int firstSum = ((-originSum % spacing) + spacing) % spacing;
        for (int sum = firstSum; sum <= width() + height();
             sum += spacing) {
            const int startX = std::max(0, sum - height());
            const int endX = std::min(width(), sum);
            painter.drawLine(startX, sum - startX,
                             endX, sum - endX);
        }
    }
};

class EngravedTextLabel final : public QLabel {
public:
    explicit EngravedTextLabel(const QString& text = {},
                               QWidget* parent = nullptr)
        : QLabel(text, parent) {
        setProperty("goliathEngravedSurface", true);
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QColor textColor(property("goliathEngravedText").toString());
        QColor depthColor(property("goliathEngravedDepth").toString());
        if (!textColor.isValid()) textColor = palette().windowText().color();
        if (!depthColor.isValid()) depthColor = QColor(255, 255, 255, 120);
        if (!isEnabled()) textColor.setAlpha(120);

        QRect drawRect = contentsRect().adjusted(margin(), margin(),
                                                  -margin(), -margin());
        int flags = alignment();
        if (wordWrap()) flags |= Qt::TextWordWrap;

        QPainter painter(this);
        painter.setFont(font());
        painter.setPen(depthColor);
        painter.drawText(drawRect.translated(1, 1), flags, text());
        painter.setPen(textColor);
        painter.drawText(drawRect, flags, text());
    }
};

class VerificationDetailsLabel final : public QLabel {
public:
    explicit VerificationDetailsLabel(std::function<void()> activate,
                                      QWidget* parent = nullptr)
        : QLabel(parent), m_activate(std::move(activate)) {}

protected:
    void mouseReleaseEvent(QMouseEvent* event) override {
        const bool activate =
            event->button() == Qt::LeftButton &&
            rect().contains(event->pos()) &&
            property("verificationDetailsAvailable").toBool();
        if (!activate) {
            QLabel::mouseReleaseEvent(event);
            return;
        }

        event->accept();
        QTimer::singleShot(0, this, [this]() {
            if (m_activate) m_activate();
        });
    }

private:
    std::function<void()> m_activate;
};

class CatalogLinkLabel final : public QLabel {
public:
    explicit CatalogLinkLabel(
        std::function<void(const QString&)> activate,
        QWidget* parent = nullptr)
        : QLabel(parent), m_activate(std::move(activate)) {}

protected:
    void mouseReleaseEvent(QMouseEvent* event) override {
        const QString url = property("externalUrl").toString();
        const bool activate = event->button() == Qt::LeftButton &&
                              rect().contains(event->pos()) &&
                              !url.isEmpty();
        if (!activate) {
            QLabel::mouseReleaseEvent(event);
            return;
        }

        event->accept();
        QTimer::singleShot(0, this, [this, url]() {
            if (m_activate) m_activate(url);
        });
    }

private:
    std::function<void(const QString&)> m_activate;
};

class EngravedComboBox final : public QComboBox {
public:
    explicit EngravedComboBox(QWidget* parent = nullptr)
        : QComboBox(parent) {
        setProperty("goliathEngravedSurface", true);
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QStyleOptionComboBox option;
        initStyleOption(&option);
        const QString displayText = option.currentText;
        option.currentText.clear();
        option.currentIcon = QIcon();

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        style()->drawComplexControl(QStyle::CC_ComboBox, &option, &painter,
                                    this);

        QColor textColor(property("goliathEngravedText").toString());
        QColor depthColor(property("goliathEngravedDepth").toString());
        QColor hoverColor(property("goliathEngravedHover").toString());
        if (!textColor.isValid()) textColor = palette().buttonText().color();
        if (!depthColor.isValid()) depthColor = QColor(255, 255, 255, 120);
        if (!hoverColor.isValid()) hoverColor = textColor;
        if (option.state.testFlag(QStyle::State_MouseOver) && isEnabled())
            textColor = hoverColor;
        if (!isEnabled()) textColor.setAlpha(120);

        QRect textRect = style()->subControlRect(
            QStyle::CC_ComboBox, &option, QStyle::SC_ComboBoxEditField, this);
        const QString elided = fontMetrics().elidedText(
            displayText, Qt::ElideRight, textRect.width());
        constexpr int flags = Qt::AlignLeft | Qt::AlignVCenter |
                              Qt::TextSingleLine;
        painter.setFont(font());
        painter.setPen(depthColor);
        painter.drawText(textRect.translated(1, 1), flags, elided);
        painter.setPen(textColor);
        painter.drawText(textRect, flags, elided);
    }
};

class EngravedCheckBox final : public QCheckBox {
public:
    explicit EngravedCheckBox(const QString& label,
                              QWidget* parent = nullptr)
        : QCheckBox(label, parent) {
        setProperty("goliathEngravedSurface", true);
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QStyleOptionButton option;
        initStyleOption(&option);

        QColor textColor(property("goliathEngravedText").toString());
        QColor depthColor(property("goliathEngravedDepth").toString());
        QColor hoverColor(property("goliathEngravedHover").toString());
        if (!textColor.isValid()) textColor = palette().windowText().color();
        if (!depthColor.isValid()) depthColor = QColor(255, 255, 255, 120);
        if (!hoverColor.isValid()) hoverColor = textColor;
        if (option.state.testFlag(QStyle::State_MouseOver) && isEnabled())
            textColor = hoverColor;
        if (!isEnabled()) textColor.setAlpha(120);

        QRect indicator = style()->subElementRect(
            QStyle::SE_CheckBoxIndicator, &option, this);
        const QRect textRect = style()->subElementRect(
            QStyle::SE_CheckBoxContents, &option, this);
        indicator = indicator.adjusted(1, 1, -1, -1);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(depthColor, 1.4));
        painter.drawRoundedRect(indicator.translated(1, 1), 3.0, 3.0);
        painter.setPen(QPen(textColor, 1.4));
        painter.drawRoundedRect(indicator, 3.0, 3.0);

        if (option.state.testFlag(QStyle::State_On)) {
            QPainterPath check;
            check.moveTo(indicator.left() + indicator.width() * 0.20,
                         indicator.center().y());
            check.lineTo(indicator.left() + indicator.width() * 0.43,
                         indicator.bottom() - indicator.height() * 0.22);
            check.lineTo(indicator.right() - indicator.width() * 0.16,
                         indicator.top() + indicator.height() * 0.20);
            painter.setPen(QPen(depthColor, 2.0, Qt::SolidLine,
                                Qt::RoundCap, Qt::RoundJoin));
            painter.drawPath(check.translated(1.0, 1.0));
            painter.setPen(QPen(hoverColor, 2.0, Qt::SolidLine,
                                Qt::RoundCap, Qt::RoundJoin));
            painter.drawPath(check);
        }

        const int baseline = textRect.center().y() +
            (fontMetrics().ascent() - fontMetrics().descent()) / 2;
        const QPoint textPoint(textRect.left(), baseline);
        painter.setFont(font());
        painter.setPen(depthColor);
        painter.drawText(textPoint + QPoint(1, 1), text());
        painter.setPen(textColor);
        painter.drawText(textPoint, text());
    }
};

class EngravedTextButton final : public QPushButton {
public:
    explicit EngravedTextButton(const QString& label = {},
                                QWidget* parent = nullptr)
        : QPushButton(label, parent) {
        setProperty("goliathEngravedSurface", true);
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QStyleOptionButton option;
        initStyleOption(&option);
        const QString displayText = option.text;
        option.text.clear();
        option.icon = QIcon();

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        style()->drawControl(QStyle::CE_PushButton, &option, &painter, this);

        QColor textColor(property("goliathEngravedText").toString());
        QColor depthColor(property("goliathEngravedDepth").toString());
        QColor accentColor(property("goliathEngravedHover").toString());
        if (!textColor.isValid()) textColor = palette().buttonText().color();
        if (!depthColor.isValid()) depthColor = QColor(255, 255, 255, 120);
        if (!accentColor.isValid()) accentColor = textColor;

        const bool accented = isChecked() || property("rated").toBool() ||
            option.state.testFlag(QStyle::State_MouseOver);
        if (accented && isEnabled()) textColor = accentColor;
        if (!isEnabled()) textColor.setAlpha(120);

        const QRect content = style()->subElementRect(
            QStyle::SE_PushButtonContents, &option, this);
        const int pressedOffset = option.state.testFlag(QStyle::State_Sunken)
            ? 1 : 0;
        constexpr int flags = Qt::AlignCenter | Qt::TextSingleLine;

        painter.setFont(font());
        if (property("goliathCircledStar").toBool()) {
            constexpr int iconSize = 17;
            constexpr int gap = 6;
            const int textWidth = fontMetrics().horizontalAdvance(displayText);
            const int totalWidth = iconSize + gap + textWidth;
            const int startX = content.center().x() - totalWidth / 2 +
                pressedOffset;
            const int iconY = content.center().y() - iconSize / 2 +
                pressedOffset;
            const QRectF iconRect(startX, iconY, iconSize, iconSize);
            const int baseline = content.center().y() +
                (fontMetrics().ascent() - fontMetrics().descent()) / 2 +
                pressedOffset;
            const QPoint textPoint(startX + iconSize + gap, baseline);

            drawCircledStar(painter, iconRect.translated(1.0, 1.0),
                            depthColor, isChecked());
            painter.setPen(depthColor);
            painter.drawText(textPoint + QPoint(1, 1), displayText);
            drawCircledStar(painter, iconRect, textColor, isChecked());
            painter.setPen(textColor);
            painter.drawText(textPoint, displayText);
            return;
        }

        const QRect textRect = content.translated(pressedOffset, pressedOffset);
        painter.setPen(depthColor);
        painter.drawText(textRect.translated(1, 1), flags, displayText);
        painter.setPen(textColor);
        painter.drawText(textRect, flags, displayText);
    }

private:
    static void drawCircledStar(QPainter& painter, const QRectF& bounds,
                                const QColor& color, bool filled) {
        painter.setPen(QPen(color, 1.25, Qt::SolidLine,
                            Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(bounds.adjusted(1.0, 1.0, -1.0, -1.0));

        QPainterPath star;
        const QPointF centre = bounds.center();
        constexpr int points = 10;
        constexpr qreal pi = 3.14159265358979323846;
        for (int index = 0; index < points; ++index) {
            const qreal radius = index % 2 == 0 ? 5.1 : 2.25;
            const qreal angle = -pi / 2.0 + index * pi / 5.0;
            const QPointF point(centre.x() + std::cos(angle) * radius,
                                centre.y() + std::sin(angle) * radius);
            if (index == 0) star.moveTo(point);
            else star.lineTo(point);
        }
        star.closeSubpath();
        painter.setBrush(filled ? QBrush(color) : QBrush(Qt::NoBrush));
        painter.drawPath(star);
        painter.setBrush(Qt::NoBrush);
    }
};

class EngravedActionButton final : public QPushButton {
public:
    enum class Glyph { Tools, Settings, About, Filters, Random };

    EngravedActionButton(const QString& label, Glyph glyph,
                         QWidget* parent = nullptr)
        : QPushButton(label, parent), m_glyph(glyph) {
        setProperty("goliathEngravedAction", true);
        setProperty("goliathEngravedSurface", true);
    }

    QSize sizeHint() const override {
        const QFontMetrics metrics(font());
        return QSize(metrics.horizontalAdvance(text()) + 48,
                     std::max(30, metrics.height() + 12));
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QStyleOptionButton option;
        initStyleOption(&option);
        option.features.setFlag(QStyleOptionButton::HasMenu, false);
        option.text.clear();
        option.icon = QIcon();

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        style()->drawControl(QStyle::CE_PushButton, &option, &painter, this);

        QColor textColor(property("goliathEngravedText").toString());
        QColor depthColor(property("goliathEngravedDepth").toString());
        QColor hoverColor(property("goliathEngravedHover").toString());
        if (!textColor.isValid()) textColor = palette().buttonText().color();
        if (!depthColor.isValid()) depthColor = QColor(255, 255, 255, 120);
        if (!hoverColor.isValid()) hoverColor = textColor;
        if (option.state.testFlag(QStyle::State_MouseOver) && isEnabled())
            textColor = hoverColor;
        if (!isEnabled()) textColor.setAlpha(120);

        const QRect content = style()->subElementRect(
            QStyle::SE_PushButtonContents, &option, this);
        const QFontMetrics metrics(font());
        constexpr int iconSize = 15;
        constexpr int gap = 6;
        const int textWidth = metrics.horizontalAdvance(text());
        const int totalWidth = iconSize + gap + textWidth;
        const int pressedOffset = option.state.testFlag(QStyle::State_Sunken)
            ? 1 : 0;
        const int startX = content.center().x() - totalWidth / 2 + pressedOffset;
        const int iconY = content.center().y() - iconSize / 2 + pressedOffset;
        const QRectF iconRect(startX, iconY, iconSize, iconSize);
        const int baseline = content.center().y() +
            (metrics.ascent() - metrics.descent()) / 2 + pressedOffset;
        const QPoint textPoint(startX + iconSize + gap, baseline);

        drawGlyph(painter, iconRect.translated(1.0, 1.0), depthColor);
        painter.setPen(depthColor);
        painter.drawText(textPoint + QPoint(1, 1), text());

        drawGlyph(painter, iconRect, textColor);
        painter.setPen(textColor);
        painter.drawText(textPoint, text());
    }

private:
    void drawGlyph(QPainter& painter, const QRectF& bounds,
                   const QColor& color) const {
        QPen pen(color, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        const QPointF centre = bounds.center();

        if (m_glyph == Glyph::Tools) {
            painter.drawLine(bounds.left() + 3.0, bounds.bottom() - 2.5,
                             bounds.right() - 3.5, bounds.top() + 3.0);
            painter.drawEllipse(QPointF(bounds.left() + 2.8,
                                        bounds.bottom() - 2.8), 1.8, 1.8);
            QPainterPath jaw;
            jaw.moveTo(bounds.right() - 6.0, bounds.top() + 2.0);
            jaw.lineTo(bounds.right() - 3.0, bounds.top() + 4.8);
            jaw.lineTo(bounds.right() - 1.3, bounds.top() + 1.2);
            painter.drawPath(jaw);
            painter.drawLine(bounds.left() + 3.0, bounds.top() + 2.0,
                             bounds.right() - 2.0, bounds.bottom() - 3.0);
            painter.drawLine(bounds.left() + 1.8, bounds.top() + 1.2,
                             bounds.left() + 4.8, bounds.top() + 4.5);
        } else if (m_glyph == Glyph::Settings) {
            painter.drawEllipse(centre, 3.0, 3.0);
            painter.drawEllipse(centre, 1.0, 1.0);
            for (int index = 0; index < 8; ++index) {
                const qreal angle = index * 3.14159265358979323846 / 4.0;
                const QPointF inner(centre.x() + std::cos(angle) * 4.2,
                                    centre.y() + std::sin(angle) * 4.2);
                const QPointF outer(centre.x() + std::cos(angle) * 6.2,
                                    centre.y() + std::sin(angle) * 6.2);
                painter.drawLine(inner, outer);
            }
        } else if (m_glyph == Glyph::About) {
            painter.drawEllipse(bounds.adjusted(1.5, 1.5, -1.5, -1.5));
            painter.drawPoint(QPointF(centre.x(), bounds.top() + 4.2));
            painter.drawLine(QPointF(centre.x(), bounds.top() + 6.7),
                             QPointF(centre.x(), bounds.bottom() - 3.0));
        } else if (m_glyph == Glyph::Filters) {
            QPainterPath funnel;
            funnel.moveTo(bounds.left() + 1.5, bounds.top() + 2.0);
            funnel.lineTo(bounds.right() - 1.5, bounds.top() + 2.0);
            funnel.lineTo(centre.x() + 2.0, centre.y() + 1.0);
            funnel.lineTo(centre.x() + 2.0, bounds.bottom() - 2.0);
            funnel.lineTo(centre.x() - 1.5, bounds.bottom() - 3.5);
            funnel.lineTo(centre.x() - 1.5, centre.y() + 1.0);
            funnel.closeSubpath();
            painter.drawPath(funnel);
        } else {
            const QRectF die = bounds.adjusted(1.8, 1.8, -1.8, -1.8);
            painter.drawRoundedRect(die, 2.2, 2.2);
            painter.setBrush(color);
            painter.drawEllipse(QPointF(die.left() + 2.6,
                                        die.top() + 2.6), 0.9, 0.9);
            painter.drawEllipse(centre, 0.9, 0.9);
            painter.drawEllipse(QPointF(die.right() - 2.6,
                                        die.bottom() - 2.6), 0.9, 0.9);
            painter.setBrush(Qt::NoBrush);
        }
    }

    Glyph m_glyph;
};

void addPanelElevation(QWidget* panel) {
    panel->setProperty("goliathElevatedPanel", true);

    auto* shadow = new QGraphicsDropShadowEffect(panel);
    shadow->setBlurRadius(14.0);
    shadow->setOffset(0.0, 0.0);
    shadow->setColor(QColor(0, 0, 0, 72));
    panel->setGraphicsEffect(shadow);
}

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

std::pair<QString, QString> ensureSpinAssets(
    const fs::path& assetsDir,
    const std::string& themeName,
    const QString& colorHex) {
    std::error_code ec;
    fs::create_directories(assetsDir, ec);

    std::string safeName = themeName;
    std::replace(safeName.begin(), safeName.end(), ' ', '_');
    std::replace(safeName.begin(), safeName.end(), '-', '_');

    const fs::path upPath = assetsDir / ("spin-up-" + safeName + ".png");
    const fs::path downPath = assetsDir / ("spin-down-" + safeName + ".png");

    constexpr int width = 10;
    constexpr int height = 6;
    QPixmap up(width, height);
    QPixmap down(width, height);
    up.fill(Qt::transparent);
    down.fill(Qt::transparent);

    const QColor color(colorHex);
    {
        QPainter painter(&up);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setBrush(color);
        painter.setPen(Qt::NoPen);
        QPolygon poly;
        poly << QPoint(1, 5) << QPoint(5, 1) << QPoint(9, 5);
        painter.drawPolygon(poly);
    }
    {
        QPainter painter(&down);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setBrush(color);
        painter.setPen(Qt::NoPen);
        QPolygon poly;
        poly << QPoint(1, 1) << QPoint(5, 5) << QPoint(9, 1);
        painter.drawPolygon(poly);
    }

    up.save(QString::fromStdString(upPath.string()));
    down.save(QString::fromStdString(downPath.string()));
    return {QString::fromStdString(upPath.string()),
            QString::fromStdString(downPath.string())};
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

    auto* contentWidget = new HatchedBackgroundWidget();
    outerLayout->addWidget(contentWidget, 1);
    auto* mainLayout = new QVBoxLayout(contentWidget);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    // --- Modern Top Toolbar ---
    auto* toolbarFrame = new QFrame();
    toolbarFrame->setObjectName("toolbar_frame");
    addPanelElevation(toolbarFrame);
    auto* toolbar = new QHBoxLayout(toolbarFrame);
    toolbar->setSpacing(10);

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

    toolbar->addWidget(new QLabel("  "));

    auto* randomBtn = new EngravedActionButton(
        "Random", EngravedActionButton::Glyph::Random);
    randomBtn->setObjectName("random_btn");
    randomBtn->setToolTip(QString("Launch random game (%1)").arg(
        QKeySequence(kRandomGameShortcut).toString(QKeySequence::NativeText)));
    connect(randomBtn, &QPushButton::clicked, this, &MainWindow::launchRandomGame);
    toolbar->addWidget(randomBtn);

    toolbar->addStretch();

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
        LithogenDialog dialog(m_config, QString::fromStdString(m_romDir.string()), this);
        if (dialog.exec() == QDialog::Accepted) rescanRoms();
    });
    toolsMenu->addSeparator();
    m_rescanAction = toolsMenu->addAction(
        QString::fromUtf8("\xF0\x9F\x94\x84 Rescan ROMs (%1)").arg(
            QKeySequence(kRescanRomsShortcut).toString(QKeySequence::NativeText)),
        this, &MainWindow::rescanRoms);
    toolsMenu->addAction(QString::fromUtf8("\xF0\x9F\x94\x8D Verify BIOS"),
                         this, &MainWindow::verifyBios); // 🔍
    toolsMenu->addSeparator();
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

    auto* settingsBtn = new EngravedActionButton(
        "Settings", EngravedActionButton::Glyph::Settings);
    settingsBtn->setObjectName("settings_btn");
    connect(settingsBtn, &QPushButton::clicked, this, &MainWindow::openSettings);
    toolbar->addWidget(settingsBtn);

    auto* aboutBtn = new EngravedActionButton(
        "About", EngravedActionButton::Glyph::About);
    aboutBtn->setObjectName("about_btn");
    connect(aboutBtn, &QPushButton::clicked, this, &MainWindow::openAbout);
    toolbar->addWidget(aboutBtn);

    // Match the library's 8 px shadow room on the left and the details cards'
    // combined 12 px inset on the right, so the toolbar and content outlines
    // share the same visual axes.
    auto* toolbarRow = new QHBoxLayout();
    toolbarRow->setContentsMargins(8, 0, 12, 0);
    toolbarRow->setSpacing(0);
    toolbarRow->addWidget(toolbarFrame);
    mainLayout->addLayout(toolbarRow);

    // --- Splitter ---
    m_splitter = new QSplitter(Qt::Horizontal);
    m_splitter->setObjectName("library_details_splitter");
    m_splitter->setHandleWidth(6);
    mainLayout->addWidget(m_splitter, 1);

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
    detailsLayout->addWidget(detailsHeaderCard);
    detailsLayout->addSpacing(12);

    auto* mediaRow = new QHBoxLayout();
    mediaRow->setSpacing(8);
    // The gallery container establishes the row's vertical shadow room. Keep
    // the row itself margin-free so its total height remains unchanged.
    mediaRow->setContentsMargins(0, 0, 0, 0);

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
        {"last_played", "Last Played"},
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
    mediaRow->addWidget(infoGrid, 1, Qt::AlignVCenter);

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
    mediaRow->addWidget(galleryPanel, 0, Qt::AlignTop);
    detailsLayout->addLayout(mediaRow);

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

    m_detailsScroll->setWidget(detailsWidget);
    rightLayout->addWidget(m_detailsScroll, 1);

    m_splitter->addWidget(rightWidget);

    m_splitter->setCollapsible(0, false);
    // resizeEvent() deliberately collapses the details pane below 1000 px to
    // enter Classic View, then restores it above 1150 px.
    m_splitter->setCollapsible(1, true);

    m_splitter->setSizes({400, 760});
    connect(m_splitter, &QSplitter::splitterMoved, this,
            [this](int, int) {
                updateIconViewGridSize();
            });

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

    // Keyboard shortcuts
    connect(new QShortcut(QKeySequence(kRescanRomsShortcut), this),
            &QShortcut::activated, this, &MainWindow::rescanRoms);
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
    auto [spinUpPath, spinDownPath] = ensureSpinAssets(
        assetsDir, themeKey, QString::fromStdString(theme.text_primary));

    QString qss = QString::fromStdString(stylesheet);
    qss.replace("<<BRANCH_CLOSED>>", toUrlPath(closedPath));
    qss.replace("<<BRANCH_OPEN>>", toUrlPath(openPath));
    qss.replace("<<BRANCH_CLOSED_SELECTED>>", toUrlPath(closedSelPath));
    qss.replace("<<BRANCH_OPEN_SELECTED>>", toUrlPath(openSelPath));
    qss.replace("<<SPIN_UP>>", toUrlPath(spinUpPath));
    qss.replace("<<SPIN_DOWN>>", toUrlPath(spinDownPath));

    setStyleSheet(qss);

    if (m_tree) {
        m_tree->setProperty(
            "goliathLibraryBackground",
            QString::fromStdString(theme.bg_secondary));
        m_tree->setProperty(
            "goliathLibraryAlternateBackground",
            QString::fromStdString(theme.bg_tertiary));
        m_tree->viewport()->update();
    }

    const bool darkSurface = contrast_text_for(theme.bg_primary) == "#ffffff";
    QColor shadowColor;
    if (darkSurface) {
        shadowColor = QColor(QString::fromStdString(theme.border));
        shadowColor.setAlpha(112);
    } else {
        shadowColor = QColor(0, 0, 0, 72);
    }

    const QColor borderColor(QString::fromStdString(theme.border));
    const QColor accentColor(QString::fromStdString(theme.accent));
    QColor hatchColor(
        (borderColor.red() * 3 + accentColor.red()) / 4,
        (borderColor.green() * 3 + accentColor.green()) / 4,
        (borderColor.blue() * 3 + accentColor.blue()) / 4,
        darkSurface ? 36 : 28);

    if (m_iconView) {
        QColor artworkBackground(
            QString::fromStdString(theme.bg_secondary));
        if (darkSurface) {
            // Dark themes need a restrained matte rather than the former
            // hard-coded white card. Blend the surface toward the theme border
            // so transparent pixel artwork retains its dark outlines.
            artworkBackground.setRed(
                (artworkBackground.red() + borderColor.red()) / 2);
            artworkBackground.setGreen(
                (artworkBackground.green() + borderColor.green()) / 2);
            artworkBackground.setBlue(
                (artworkBackground.blue() + borderColor.blue()) / 2);
        }
        m_iconView->setProperty("goliathLibraryArtworkBackground",
                                artworkBackground.name(QColor::HexRgb));
        m_iconView->setProperty("goliathLibraryArtworkBorder",
                                borderColor.name(QColor::HexRgb));
        m_iconView->viewport()->update();
    }

    const auto elevatedPanels = findChildren<QWidget*>();
    for (QWidget* panel : elevatedPanels) {
        if (panel->property("goliathHatchedBackground").toBool()) {
            panel->setProperty("goliathHatchColor",
                               hatchColor.name(QColor::HexArgb));
            panel->update();
        }
        if (panel->property("goliathElevatedPanel").toBool()) {
            if (auto* shadow = qobject_cast<QGraphicsDropShadowEffect*>(
                    panel->graphicsEffect())) {
                shadow->setColor(shadowColor);
            }
        }
        if (panel->property("goliathEngravedSurface").toBool()) {
            QColor depth = darkSurface
                ? QColor(0, 0, 0, 190)
                : QColor(255, 255, 255, 210);
            const bool secondary =
                panel->property("goliathEngravedRole").toString() ==
                QStringLiteral("secondary");
            panel->setProperty("goliathEngravedText",
                               QString::fromStdString(secondary
                                   ? theme.text_secondary
                                   : theme.text_primary));
            panel->setProperty("goliathEngravedDepth", depth.name(QColor::HexArgb));
            panel->setProperty("goliathEngravedHover",
                               QString::fromStdString(theme.accent));
            panel->setProperty(
                "goliathEngravedSelectionText",
                QString::fromStdString(contrast_text_for(theme.accent)));
            panel->update();
        }
    }
    const QString comboPopupStyle = QString::fromStdString(
        generate_combo_popup_style(theme));
    applyComboPopupStyle(m_themeCombo, comboPopupStyle);

    m_config.set("UI", "theme", themeName.toStdString());
    save_config(m_config);
}

} // namespace goliath
