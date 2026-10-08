#pragma once

#include "ui/library_view_logic.hpp"

#include <QSize>
#include <QStyledItemDelegate>

namespace goliath {

// Personal-library markers live in dedicated model roles so the visible game
// title remains clean, searchable, and independent from the painted badges.
inline constexpr int LibraryFavoriteRole = Qt::UserRole + 4;
inline constexpr int LibraryRatingRole = Qt::UserRole + 5;
inline constexpr int LibraryAlternateBackgroundRole = Qt::UserRole + 6;

class LibraryItemDelegate final : public QStyledItemDelegate {
public:
    explicit LibraryItemDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
};

class LibraryTileDelegate final : public QStyledItemDelegate {
public:
    explicit LibraryTileDelegate(QObject* parent = nullptr);

    void setDisplayMode(LibraryDisplayMode mode);
    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;
    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;

private:
    LibraryDisplayMode m_mode = LibraryDisplayMode::Grid;
};

} // namespace goliath
