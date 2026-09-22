#include "ui/widgets/command_notation_view.hpp"

#include <QEvent>
#include <QByteArray>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QPolygonF>
#include <QResizeEvent>
#include <QScrollBar>
#include <QStringList>

#include <algorithm>
#include <cmath>
#include <optional>
#include <string_view>
#include <utility>

namespace goliath {
namespace {

bool is_separator_line(const QString& line) {
    const QString trimmed = line.trimmed();
    if (trimmed.size() < 5) return false;
    for (const QChar character : trimmed) {
        if (character != QChar('-') && character != QChar('=') &&
            character != QChar(0x2500) && character != QChar(0x2550)) {
            return false;
        }
    }
    return true;
}

bool is_heading_line(const QString& line) {
    const QByteArray utf8 = line.toUtf8();
    return is_command_notation_heading(std::string_view(
        utf8.constData(), static_cast<std::size_t>(utf8.size())));
}

bool is_direction(CommandGlyph glyph) {
    return glyph >= CommandGlyph::DirectionDownLeft &&
           glyph <= CommandGlyph::DirectionUpRight;
}

bool is_round_button(CommandGlyph glyph) {
    return (glyph >= CommandGlyph::ButtonA &&
            glyph <= CommandGlyph::ButtonZ) ||
           (glyph >= CommandGlyph::Button1 &&
            glyph <= CommandGlyph::Button0);
}

bool is_capsule_button(CommandGlyph glyph) {
    return glyph >= CommandGlyph::ButtonLP &&
           glyph <= CommandGlyph::Button2K;
}

void draw_outlined_text(QPainter& painter, int x, int baseline,
                        const QString& text, const QColor& foreground) {
    painter.save();
    painter.setPen(QColor(0, 0, 0, 190));
    painter.drawText(x - 1, baseline, text);
    painter.drawText(x + 1, baseline, text);
    painter.drawText(x, baseline - 1, text);
    painter.drawText(x, baseline + 1, text);
    painter.setPen(foreground);
    painter.drawText(x, baseline, text);
    painter.restore();
}

QColor button_color(CommandGlyph glyph) {
    switch (glyph) {
    case CommandGlyph::ButtonA: return QColor("#ff4040");
    case CommandGlyph::ButtonB: return QColor("#ffee00");
    case CommandGlyph::ButtonC: return QColor("#00ff40");
    case CommandGlyph::ButtonD: return QColor("#00aaff");
    case CommandGlyph::ButtonP: return QColor("#ff00aa");
    case CommandGlyph::ButtonK: return QColor("#aa00ff");
    case CommandGlyph::ButtonS: return QColor("#ffa000");
    case CommandGlyph::ButtonG: return QColor("#00ffcc");
    case CommandGlyph::ButtonH: return QColor("#ff00ff");
    case CommandGlyph::ButtonZ: return QColor("#aa00ff");
    case CommandGlyph::ButtonLP: return QColor("#ffee00");
    case CommandGlyph::ButtonMP: return QColor("#ffa000");
    case CommandGlyph::ButtonHP: return QColor("#ff4040");
    case CommandGlyph::ButtonLK: return QColor("#c0c0c0");
    case CommandGlyph::ButtonMK: return QColor("#00ffcc");
    case CommandGlyph::ButtonHK: return QColor("#00aaff");
    case CommandGlyph::Button3P: return QColor("#aa00ff");
    case CommandGlyph::Button3K: return QColor("#ff00aa");
    case CommandGlyph::Button2P: return QColor("#ff00aa");
    case CommandGlyph::Button2K: return QColor("#aa00ff");
    case CommandGlyph::Button1: return QColor("#ff4040");
    case CommandGlyph::Button2: return QColor("#ffee00");
    case CommandGlyph::Button3: return QColor("#00ff40");
    case CommandGlyph::Button4: return QColor("#00aaff");
    case CommandGlyph::Button5: return QColor("#ff00aa");
    case CommandGlyph::Button6: return QColor("#aa00ff");
    case CommandGlyph::Button7: return QColor("#00ffcc");
    case CommandGlyph::Button8: return QColor("#ff00ff");
    case CommandGlyph::Button9: return QColor("#ffa000");
    case CommandGlyph::Button0: return QColor("#c0c0c0");
    default: return QColor("#808080");
    }
}

QPoint direction_vector(CommandGlyph glyph) {
    switch (glyph) {
    case CommandGlyph::DirectionDownLeft: return {-1, 1};
    case CommandGlyph::DirectionDown: return {0, 1};
    case CommandGlyph::DirectionDownRight: return {1, 1};
    case CommandGlyph::DirectionLeft: return {-1, 0};
    case CommandGlyph::DirectionRight: return {1, 0};
    case CommandGlyph::DirectionUpLeft: return {-1, -1};
    case CommandGlyph::DirectionUp: return {0, -1};
    case CommandGlyph::DirectionUpRight: return {1, -1};
    default: return {0, 0};
    }
}

std::optional<std::pair<qsizetype, qsizetype>> column_gap(
        const QString& line) {
    qsizetype bestStart = -1;
    qsizetype bestLength = 0;
    qsizetype index = 0;
    while (index < line.size()) {
        if (line[index] != QChar(' ')) {
            ++index;
            continue;
        }
        const qsizetype start = index;
        while (index < line.size() && line[index] == QChar(' ')) ++index;
        const qsizetype length = index - start;
        if (length >= 4 && length > bestLength) {
            bestStart = start;
            bestLength = length;
        }
    }
    if (bestStart <= 0 || bestStart + bestLength >= line.size())
        return std::nullopt;
    return std::pair{bestStart, bestLength};
}

std::vector<CommandNotationToken> tokenize_qstring(const QString& text) {
    const QByteArray utf8 = text.toUtf8();
    return tokenize_command_notation(std::string_view(
        utf8.constData(), static_cast<std::size_t>(utf8.size())));
}

std::vector<CommandNotationToken> layout_units(
        const std::vector<CommandNotationToken>& tokens) {
    std::vector<CommandNotationToken> units;
    for (const CommandNotationToken& token : tokens) {
        if (token.glyph != CommandGlyph::Text) {
            units.push_back(token);
            continue;
        }

        const QString text = QString::fromUtf8(
            token.source.data(), static_cast<qsizetype>(token.source.size()));
        qsizetype start = 0;
        while (start < text.size()) {
            const bool whitespace = text[start].isSpace();
            qsizetype end = start + 1;
            while (end < text.size() &&
                   text[end].isSpace() == whitespace) {
                ++end;
            }
            const QByteArray utf8 = text.sliced(start, end - start).toUtf8();
            units.push_back({
                CommandGlyph::Text,
                std::string(utf8.constData(),
                            static_cast<std::size_t>(utf8.size()))});
            start = end;
        }
    }
    return units;
}

bool is_whitespace_token(const CommandNotationToken& token) {
    if (token.glyph != CommandGlyph::Text || token.source.empty()) return false;
    const QString text = QString::fromUtf8(
        token.source.data(), static_cast<qsizetype>(token.source.size()));
    for (const QChar character : text) {
        if (!character.isSpace()) return false;
    }
    return true;
}

} // namespace

CommandNotationView::CommandNotationView(QWidget* parent)
    : QAbstractScrollArea(parent) {
    setObjectName("command_visual_view");
    setFrameShape(QFrame::NoFrame);
    setAttribute(Qt::WA_TranslucentBackground);
    viewport()->setAttribute(Qt::WA_TranslucentBackground);
    viewport()->setAutoFillBackground(false);
    setAutoFillBackground(false);
    setFocusPolicy(Qt::StrongFocus);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    QFont visualFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    visualFont.setPointSizeF(std::max(10.5, visualFont.pointSizeF()));
    visualFont.setWeight(QFont::Medium);
    setFont(visualFont);
}

void CommandNotationView::setCommandText(const QString& text) {
    m_lines.clear();
    m_highlightedSourceLine = -1;
    const QStringList sourceLines = text.split('\n', Qt::KeepEmptyParts);
    m_lines.reserve(static_cast<std::size_t>(sourceLines.size()));

    for (const QString& sourceLine : sourceLines) {
        VisualLine line;
        line.raw = sourceLine;
        line.separator = is_separator_line(sourceLine);
        line.heading = is_heading_line(sourceLine);
        if (!line.separator && !line.heading) {
            if (const auto gap = column_gap(sourceLine)) {
                const QString left = sourceLine.left(gap->first).trimmed();
                const QString right = sourceLine.mid(
                    gap->first + gap->second).trimmed();
                if (!left.isEmpty() && !right.isEmpty()) {
                    line.leftHeading = is_heading_line(left);
                    line.tokens = tokenize_qstring(left);
                    line.rightTokens = tokenize_qstring(right);
                    line.columns = true;
                }
            }
        }
        if (!line.columns)
            line.tokens = tokenize_qstring(sourceLine);
        m_lines.push_back(std::move(line));
    }
    verticalScrollBar()->setValue(0);
    horizontalScrollBar()->setValue(0);
    updateScrollBars();
    viewport()->update();
}

void CommandNotationView::setOverlayPresentation(bool enabled) {
    if (m_overlayPresentation == enabled) return;
    m_overlayPresentation = enabled;
    viewport()->update();
}

void CommandNotationView::revealSourceLine(int sourceLine) {
    m_highlightedSourceLine = sourceLine;
    for (std::size_t index = 0; index < m_renderRows.size(); ++index) {
        if (m_renderRows[index].sourceLine != sourceLine) continue;
        const int rowTop = m_margin + static_cast<int>(index) * m_lineHeight;
        const int centered = rowTop -
            std::max(0, (viewport()->height() - m_lineHeight) / 3);
        verticalScrollBar()->setValue(centered);
        viewport()->update();
        return;
    }
    viewport()->update();
}

void CommandNotationView::clearFindHighlight() {
    m_highlightedSourceLine = -1;
    viewport()->update();
}

void CommandNotationView::changeEvent(QEvent* event) {
    QAbstractScrollArea::changeEvent(event);
    if (event->type() == QEvent::FontChange ||
        event->type() == QEvent::PaletteChange ||
        event->type() == QEvent::StyleChange) {
        updateScrollBars();
        viewport()->update();
    }
}

void CommandNotationView::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const QFontMetrics metrics(font());
    const int verticalOffset = verticalScrollBar()->value();
    const int firstLine = std::max(
        0, (verticalOffset - m_margin) / m_lineHeight);
    const int visibleBottom = verticalOffset + viewport()->height();

    for (int index = firstLine;
         index < static_cast<int>(m_renderRows.size()); ++index) {
        const int top = m_margin + index * m_lineHeight;
        if (top > visibleBottom) break;
        if (top + m_lineHeight < verticalOffset) continue;

        const RenderRow& row = m_renderRows[static_cast<std::size_t>(index)];
        const int x = m_margin;
        const int y = top - verticalOffset;
        const int baseline = y + (m_lineHeight - metrics.height()) / 2 +
                             metrics.ascent();

        if (row.sourceLine == m_highlightedSourceLine) {
            QColor match = headingColor();
            match.setAlpha(44);
            painter.fillRect(QRect(0, y, viewport()->width(), m_lineHeight),
                             match);
            QColor marker = headingColor();
            marker.setAlpha(220);
            painter.fillRect(QRect(0, y, 3, m_lineHeight), marker);
        }

        if (row.separator) {
            QPen separatorPen(palette().color(QPalette::Mid));
            separatorPen.setWidthF(1.5);
            painter.setPen(separatorPen);
            painter.drawLine(
                QPointF(x, y + m_lineHeight / 2.0),
                QPointF(viewport()->width() - m_margin,
                        y + m_lineHeight / 2.0));
            continue;
        }

        if (row.heading) {
            QFont headingFont = font();
            headingFont.setBold(true);
            painter.setFont(headingFont);
            draw_outlined_text(
                painter, x, baseline, row.text,
                headingColor());
            painter.setFont(font());
            continue;
        }

        painter.setFont(font());
        painter.setPen(palette().color(QPalette::Text));
        for (const PositionedToken& positioned : row.tokens) {
            const CommandNotationToken& token = positioned.token;
            const int tokenX = x + positioned.x;
            if (token.glyph == CommandGlyph::Text) {
                const QString text = QString::fromUtf8(
                    token.source.data(),
                    static_cast<qsizetype>(token.source.size()));
                draw_outlined_text(
                    painter, tokenX, baseline, text,
                    positioned.highlighted
                        ? headingColor()
                        : palette().color(QPalette::Text));
                continue;
            }

            const int width = glyphWidth(token.glyph, metrics);
            drawGlyph(painter, token.glyph,
                      QRectF(tokenX, y + 3, width, m_lineHeight - 6));
        }
    }
}

void CommandNotationView::resizeEvent(QResizeEvent* event) {
    QAbstractScrollArea::resizeEvent(event);
    updateScrollBars();
}

int CommandNotationView::glyphWidth(CommandGlyph glyph,
                                    const QFontMetrics& metrics) const {
    if (is_direction(glyph) || is_round_button(glyph) ||
        glyph == CommandGlyph::FollowUp) {
        return 22;
    }
    if (glyph == CommandGlyph::Plus) return 14;
    if (glyph == CommandGlyph::Ellipsis) return 20;

    const QString label = QString::fromLatin1(command_glyph_label(glyph));
    if (is_capsule_button(glyph))
        return std::max(25, metrics.horizontalAdvance(label) + 10);
    return std::max(22, metrics.horizontalAdvance(label) + 12);
}

QColor CommandNotationView::headingColor() const {
    return m_overlayPresentation
        ? QColor("#8fb3ff")
        : palette().color(QPalette::Highlight);
}

int CommandNotationView::tokenWidth(
        const CommandNotationToken& token,
        const QFontMetrics& metrics) const {
    if (token.glyph == CommandGlyph::Text) {
        return metrics.horizontalAdvance(QString::fromUtf8(
            token.source.data(),
            static_cast<qsizetype>(token.source.size())));
    }
    return glyphWidth(token.glyph, metrics) + 2;
}

void CommandNotationView::rebuildRenderRows(
        const QFontMetrics& metrics) {
    m_renderRows.clear();
    const int contentWidth = std::max(120, viewport()->width() - m_margin * 2);

    const auto addUnwrapped = [this, &metrics](
            RenderRow& row,
            const std::vector<CommandNotationToken>& tokens,
            int startX,
            bool highlighted) {
        int x = startX;
        for (const CommandNotationToken& token : layout_units(tokens)) {
            row.tokens.push_back({token, x, highlighted});
            x += tokenWidth(token, metrics);
        }
        return x;
    };

    const auto addWrapped = [this, &metrics, contentWidth](
            RenderRow first,
            const std::vector<CommandNotationToken>& tokens,
            int startX,
            int sourceLine,
            bool highlighted) {
        RenderRow row = std::move(first);
        row.sourceLine = sourceLine;
        int x = startX;
        bool contentAdded = false;
        std::optional<CommandNotationToken> pendingSpace;

        for (const CommandNotationToken& token : layout_units(tokens)) {
            if (is_whitespace_token(token)) {
                if (contentAdded) pendingSpace = token;
                continue;
            }

            const int spaceWidth = pendingSpace
                ? tokenWidth(*pendingSpace, metrics) : 0;
            const int width = tokenWidth(token, metrics);
            if (!contentAdded && !row.tokens.empty() &&
                x + width > contentWidth) {
                m_renderRows.push_back(std::move(row));
                row = RenderRow{};
                row.sourceLine = sourceLine;
                x = startX;
            }
            if (contentAdded && x + spaceWidth + width > contentWidth) {
                m_renderRows.push_back(std::move(row));
                row = RenderRow{};
                row.sourceLine = sourceLine;
                x = startX;
                contentAdded = false;
                pendingSpace.reset();
            }

            if (pendingSpace && contentAdded) {
                row.tokens.push_back({*pendingSpace, x, highlighted});
                x += tokenWidth(*pendingSpace, metrics);
            }
            pendingSpace.reset();
            row.tokens.push_back({token, x, highlighted});
            x += width;
            contentAdded = true;
        }

        m_renderRows.push_back(std::move(row));
    };

    for (std::size_t lineIndex = 0; lineIndex < m_lines.size(); ++lineIndex) {
        const VisualLine& line = m_lines[lineIndex];
        const int sourceLine = static_cast<int>(lineIndex);
        if (line.separator || line.heading) {
            RenderRow row;
            row.text = line.raw.trimmed();
            row.separator = line.separator;
            row.heading = line.heading;
            row.sourceLine = sourceLine;
            m_renderRows.push_back(std::move(row));
            continue;
        }

        if (!line.columns) {
            addWrapped(RenderRow{}, line.tokens, 0, sourceLine, false);
            continue;
        }

        RenderRow first;
        first.sourceLine = sourceLine;
        const int leftWidth = addUnwrapped(
            first, line.tokens, 0, line.leftHeading);
        if (leftWidth + 96 >= contentWidth) {
            m_renderRows.push_back(std::move(first));
            addWrapped(RenderRow{}, line.rightTokens, 24,
                       sourceLine, false);
            continue;
        }

        const int commandX = std::clamp(
            std::max(leftWidth + 18, contentWidth * 52 / 100),
            0, contentWidth - 80);
        addWrapped(std::move(first), line.rightTokens, commandX,
                   sourceLine, false);
    }
}

void CommandNotationView::drawGlyph(QPainter& painter, CommandGlyph glyph,
                                    const QRectF& rect) const {
    painter.save();
    const QColor textColor = palette().color(QPalette::Text);

    if (is_direction(glyph)) {
        if (glyph == CommandGlyph::DirectionNeutral) {
            painter.setPen(QPen(textColor, 2.0));
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(rect.center(), 4.5, 4.5);
            painter.restore();
            return;
        }

        const QPoint direction = direction_vector(glyph);
        const double length = (direction.x() != 0 && direction.y() != 0)
            ? 6.5 : 7.5;
        const double norm = std::hypot(
            static_cast<double>(direction.x()),
            static_cast<double>(direction.y()));
        const QPointF unit(direction.x() / norm, direction.y() / norm);
        const QPointF perpendicular(-unit.y(), unit.x());
        const QPointF start = rect.center() - unit * length;
        const QPointF end = rect.center() + unit * length;

        QPen pen(textColor, 2.6, Qt::SolidLine, Qt::RoundCap,
                 Qt::RoundJoin);
        painter.setPen(pen);
        painter.drawLine(start, end);
        painter.setBrush(textColor);
        painter.setPen(Qt::NoPen);
        QPolygonF head;
        head << end
             << end - unit * 6.0 + perpendicular * 3.7
             << end - unit * 6.0 - perpendicular * 3.7;
        painter.drawPolygon(head);
        painter.restore();
        return;
    }

    if (is_round_button(glyph)) {
        const QColor fill = button_color(glyph);
        const qreal diameter = std::min(rect.width(), rect.height()) - 2.0;
        const QRectF circle(rect.center().x() - diameter / 2.0,
                            rect.center().y() - diameter / 2.0,
                            diameter, diameter);
        painter.setPen(QPen(fill.lighter(145), 1.0));
        painter.setBrush(fill);
        painter.drawEllipse(circle);
        QFont labelFont = font();
        labelFont.setBold(true);
        labelFont.setPointSizeF(std::max(7.0, labelFont.pointSizeF() - 2.0));
        painter.setFont(labelFont);
        painter.setPen(QColor("#111111"));
        painter.drawText(circle, Qt::AlignCenter,
                         QString::fromLatin1(command_glyph_label(glyph)));
        painter.restore();
        return;
    }

    if (glyph == CommandGlyph::FollowUp) {
        QPen pen(textColor, 2.2, Qt::SolidLine, Qt::RoundCap,
                 Qt::RoundJoin);
        painter.setPen(pen);
        const QPointF left(rect.left() + 4.0, rect.top() + 3.0);
        const QPointF corner(rect.left() + 4.0, rect.center().y());
        const QPointF end(rect.right() - 4.0, rect.center().y());
        painter.drawLine(left, corner);
        painter.drawLine(corner, end);
        painter.setBrush(textColor);
        painter.setPen(Qt::NoPen);
        QPolygonF head;
        head << end << QPointF(end.x() - 5.0, end.y() - 3.5)
             << QPointF(end.x() - 5.0, end.y() + 3.5);
        painter.drawPolygon(head);
        painter.restore();
        return;
    }

    if (glyph == CommandGlyph::Rotate360) {
        painter.setPen(QPen(textColor, 2.0));
        painter.setBrush(Qt::NoBrush);
        const QRectF arc = rect.adjusted(4.0, 3.0, -4.0, -3.0);
        painter.drawArc(arc, 35 * 16, 285 * 16);
        const QPointF end(arc.right() - 1.0, arc.center().y() - 3.0);
        painter.setBrush(textColor);
        painter.setPen(Qt::NoPen);
        QPolygonF head;
        head << end << QPointF(end.x() - 5.0, end.y() - 1.0)
             << QPointF(end.x() - 1.0, end.y() + 4.0);
        painter.drawPolygon(head);
        painter.restore();
        return;
    }

    const QString label = glyph == CommandGlyph::Plus
        ? QStringLiteral("+")
        : glyph == CommandGlyph::Ellipsis
            ? QStringLiteral("…")
            : QString::fromLatin1(command_glyph_label(glyph));
    if (glyph == CommandGlyph::Plus || glyph == CommandGlyph::Ellipsis) {
        QFont symbolFont = font();
        symbolFont.setBold(true);
        symbolFont.setPointSizeF(symbolFont.pointSizeF() + 1.0);
        painter.setFont(symbolFont);
        painter.setPen(textColor);
        painter.drawText(rect, Qt::AlignCenter, label);
        painter.restore();
        return;
    }

    QColor fill = is_capsule_button(glyph)
        ? button_color(glyph)
        : palette().color(QPalette::Mid);
    fill.setAlpha(is_capsule_button(glyph) ? 235 : 150);
    painter.setPen(QPen(textColor, 1.0));
    painter.setBrush(fill);
    painter.drawRoundedRect(rect.adjusted(1.0, 2.0, -1.0, -2.0),
                            4.0, 4.0);
    QFont labelFont = font();
    labelFont.setBold(true);
    labelFont.setPointSizeF(std::max(7.0, labelFont.pointSizeF() - 2.0));
    painter.setFont(labelFont);
    painter.setPen(is_capsule_button(glyph)
        ? QColor("#111111") : textColor);
    painter.drawText(rect, Qt::AlignCenter, label);
    painter.restore();
}

void CommandNotationView::updateScrollBars() {
    const QFontMetrics metrics(font());
    m_lineHeight = std::max(26, metrics.height() + 8);
    rebuildRenderRows(metrics);

    const int contentHeight =
        static_cast<int>(m_renderRows.size()) * m_lineHeight + m_margin * 2;
    horizontalScrollBar()->setRange(0, 0);
    verticalScrollBar()->setRange(
        0, std::max(0, contentHeight - viewport()->height()));
    verticalScrollBar()->setPageStep(viewport()->height());
    verticalScrollBar()->setSingleStep(m_lineHeight);
}

} // namespace goliath
