#pragma once

#include <QtCore/qnamespace.h>

namespace goliath {

// Shared item-data contract between the tree, grid, and their delegates.
inline constexpr int GameIndexRole = Qt::UserRole;
inline constexpr int RomIndexRole = Qt::UserRole + 1;
inline constexpr int ExactMatchRole = Qt::UserRole + 2;
inline constexpr int FilterAutoExpandedRole = Qt::UserRole + 3;
inline constexpr int LibraryFavoriteRole = Qt::UserRole + 4;
inline constexpr int LibraryRatingRole = Qt::UserRole + 5;
inline constexpr int LibraryAlternateBackgroundRole = Qt::UserRole + 6;

} // namespace goliath
