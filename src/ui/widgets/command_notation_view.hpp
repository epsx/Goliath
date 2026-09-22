// command_notation_view.hpp — vector renderer for command.dat notation.
#pragma once

#include "game/command_notation.hpp"

#include <QAbstractScrollArea>
#include <QString>

#include <vector>

class QEvent;
class QColor;
class QFontMetrics;
class QPaintEvent;
class QPainter;
class QRectF;
class QResizeEvent;

namespace goliath {

class CommandNotationView final : public QAbstractScrollArea {
public:
    explicit CommandNotationView(QWidget* parent = nullptr);

    void setCommandText(const QString& text);
    void setOverlayPresentation(bool enabled);
    void revealSourceLine(int sourceLine);
    void clearFindHighlight();

protected:
    void changeEvent(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    struct VisualLine {
        QString raw;
        std::vector<CommandNotationToken> tokens;
        std::vector<CommandNotationToken> rightTokens;
        bool columns = false;
        bool separator = false;
        bool heading = false;
        bool leftHeading = false;
    };

    struct PositionedToken {
        CommandNotationToken token;
        int x = 0;
        bool highlighted = false;
    };

    struct RenderRow {
        QString text;
        std::vector<PositionedToken> tokens;
        bool separator = false;
        bool heading = false;
        int sourceLine = -1;
    };

    int glyphWidth(CommandGlyph glyph, const QFontMetrics& metrics) const;
    QColor headingColor() const;
    int tokenWidth(const CommandNotationToken& token,
                   const QFontMetrics& metrics) const;
    void rebuildRenderRows(const QFontMetrics& metrics);
    void drawGlyph(QPainter& painter, CommandGlyph glyph,
                   const QRectF& rect) const;
    void updateScrollBars();

    std::vector<VisualLine> m_lines;
    std::vector<RenderRow> m_renderRows;
    int m_lineHeight = 28;
    int m_margin = 12;
    int m_highlightedSourceLine = -1;
    bool m_overlayPresentation = true;
};

} // namespace goliath
