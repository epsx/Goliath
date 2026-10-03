#include "ui/library_item_delegate.hpp"

#include "ui/library_view_logic.hpp"

#include <QApplication>
#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QString>
#include <QWidget>

#include <algorithm>
#include <cmath>

namespace goliath {

namespace {

constexpr qreal Pi = 3.14159265358979323846;

QPainterPath fivePointStar(const QRectF& bounds, qreal outerRadius,
                           qreal innerRadius) {
    QPainterPath star;
    const QPointF centre = bounds.center();
    for (int index = 0; index < 10; ++index) {
        const qreal radius = index % 2 == 0 ? outerRadius : innerRadius;
        const qreal angle = -Pi / 2.0 + index * Pi / 5.0;
        const QPointF point(centre.x() + std::cos(angle) * radius,
                            centre.y() + std::sin(angle) * radius);
        if (index == 0) star.moveTo(point);
        else star.lineTo(point);
    }
    star.closeSubpath();
    return star;
}

void drawFavoriteBadge(QPainter& painter, const QRectF& bounds,
                       const QColor& color) {
    painter.setPen(QPen(color, 1.2, Qt::SolidLine,
                        Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(bounds.adjusted(1.25, 1.25, -1.25, -1.25));
    painter.setBrush(color);
    painter.drawPath(fivePointStar(bounds, 5.0, 2.2));
    painter.setBrush(Qt::NoBrush);
}

void drawRatingStar(QPainter& painter, const QRectF& bounds,
                    const QColor& color) {
    painter.setPen(QPen(color, 1.0, Qt::SolidLine,
                        Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(color);
    painter.drawPath(fivePointStar(bounds, 5.8, 2.55));
    painter.setBrush(Qt::NoBrush);
}

QColor propertyColor(const QWidget* widget, const char* name,
                     const QColor& fallback) {
    if (!widget) return fallback;
    const QColor value(widget->property(name).toString());
    return value.isValid() ? value : fallback;
}

} // namespace

LibraryItemDelegate::LibraryItemDelegate(QObject* parent)
    : QStyledItemDelegate(parent) {}

void LibraryItemDelegate::paint(
        QPainter* painter, const QStyleOptionViewItem& sourceOption,
        const QModelIndex& index) const {
    if (index.column() != 0) {
        QStyledItemDelegate::paint(painter, sourceOption, index);
        return;
    }

    const bool favorite = index.data(LibraryFavoriteRole).toBool();
    const int rating = libraryListRatingBadge(
        index.data(LibraryRatingRole).toInt());
    if (!favorite && rating == 0) {
        QStyledItemDelegate::paint(painter, sourceOption, index);
        return;
    }

    QStyleOptionViewItem option(sourceOption);
    initStyleOption(&option, index);
    const QString title = option.text;
    const QWidget* widget = option.widget;
    QStyle* style = widget ? widget->style() : QApplication::style();
    const QRect textRect = style->subElementRect(
        QStyle::SE_ItemViewItemText, &option, widget);

    // Let the active style draw the row, selection, focus, branch spacing and
    // game artwork. Draw only the title and compact personal markers ourselves.
    QStyleOptionViewItem backgroundOption(option);
    backgroundOption.text.clear();
    style->drawControl(QStyle::CE_ItemViewItem, &backgroundOption,
                       painter, widget);

    constexpr int FavoriteBadgeSize = 18;
    constexpr int RatingStarSize = 13;
    constexpr int RatingStarSpacing = 2;
    constexpr int BeforeBadge = 4;
    const int favoriteWidth = favorite
        ? FavoriteBadgeSize + BeforeBadge : 0;
    const int ratingStarsWidth = rating > 0
        ? rating * RatingStarSize + (rating - 1) * RatingStarSpacing : 0;
    const int ratingWidth = rating > 0
        ? BeforeBadge + ratingStarsWidth : 0;
    const int titleWidth = std::max(
        0, textRect.width() - favoriteWidth - ratingWidth);
    const int titleX = textRect.left() + favoriteWidth;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setFont(option.font);

    const bool selected = option.state.testFlag(QStyle::State_Selected);
    const bool enabled = option.state.testFlag(QStyle::State_Enabled);
    const QPalette::ColorGroup colorGroup = !enabled
        ? QPalette::Disabled
        : option.state.testFlag(QStyle::State_Active)
              ? QPalette::Active : QPalette::Inactive;
    // QSS already defines one on-accent color for selected rows.  Use the
    // matching theme property here as well so custom badge rows do not switch
    // between Active and Inactive palette colors when focus leaves the tree.
    const QColor textColor = selected
        ? propertyColor(
              widget, "goliathEngravedSelectionText",
              option.palette.color(QPalette::Active,
                                   QPalette::HighlightedText))
        : option.palette.color(colorGroup, QPalette::Text);
    painter->setPen(textColor);

    const QFontMetrics metrics(option.font);
    const QString elided = metrics.elidedText(
        title, option.textElideMode, titleWidth);
    painter->drawText(QRect(titleX, textRect.top(),
                            titleWidth, textRect.height()),
                      Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                      elided);

    const int favoriteBadgeY =
        textRect.center().y() - FavoriteBadgeSize / 2;
    const int ratingStarY =
        textRect.center().y() - RatingStarSize / 2;

    QColor markerColor = selected
        ? textColor
        : propertyColor(widget, "goliathEngravedHover", textColor);
    QColor depthColor = propertyColor(
        widget, "goliathEngravedDepth", QColor(255, 255, 255, 120));
    if (!enabled) {
        markerColor.setAlpha(120);
        depthColor.setAlpha(80);
    }

    const auto engraved = [&](auto drawBadge, const QRectF& rect) {
        drawBadge(rect.translated(1.0, 1.0), depthColor);
        drawBadge(rect, markerColor);
    };

    if (favorite) {
        const QRectF badgeRect(
            textRect.left(), favoriteBadgeY,
            FavoriteBadgeSize, FavoriteBadgeSize);
        engraved([&](const QRectF& rect, const QColor& color) {
            drawFavoriteBadge(*painter, rect, color);
        }, badgeRect);
    }
    if (rating > 0) {
        int starX = textRect.right() - ratingStarsWidth;
        for (int index = 0; index < rating; ++index) {
            const QRectF starRect(
                starX, ratingStarY, RatingStarSize, RatingStarSize);
            engraved([&](const QRectF& rect, const QColor& color) {
                drawRatingStar(*painter, rect, color);
            }, starRect);
            starX += RatingStarSize + RatingStarSpacing;
        }
    }

    painter->restore();
}

} // namespace goliath
