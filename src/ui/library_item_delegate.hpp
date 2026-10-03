#pragma once

#include <QStyledItemDelegate>

namespace goliath {

// Personal-library markers live in dedicated model roles so the visible game
// title remains clean, searchable, and independent from the painted badges.
inline constexpr int LibraryFavoriteRole = Qt::UserRole + 4;
inline constexpr int LibraryRatingRole = Qt::UserRole + 5;

class LibraryItemDelegate final : public QStyledItemDelegate {
public:
    explicit LibraryItemDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
};

} // namespace goliath
