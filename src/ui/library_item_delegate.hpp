#pragma once

#include "ui/library_item_roles.hpp"
#include "ui/library_view_logic.hpp"

#include <QSize>
#include <QStyledItemDelegate>

namespace goliath {

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
