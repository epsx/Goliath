#pragma once

#include <QString>

class QWidget;

namespace goliath {

struct LibraryVerificationPresentation;
struct Rom;

void showLibraryVerificationDialog(
    const QString& title,
    const Rom& rom,
    const LibraryVerificationPresentation& presentation,
    QWidget* parent);

} // namespace goliath
