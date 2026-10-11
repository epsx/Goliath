#include "ui/main_window.hpp"

#include "common/qt_path.hpp"
#include "common/theme.hpp"
#include "ui/widgets/title_bar.hpp"

#include <QColor>
#include <QGraphicsDropShadowEffect>
#include <QListWidget>
#include <QPainter>
#include <QPixmap>
#include <QPoint>
#include <QPolygon>
#include <QTreeWidget>
#include <QWidget>

#include <algorithm>
#include <filesystem>
#include <utility>

namespace fs = std::filesystem;

namespace goliath {

namespace {

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

    closed.save(path_to_qstring(closedPath));
    open.save(path_to_qstring(openPath));

    return {path_to_qstring(closedPath), path_to_qstring(openPath)};
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

    up.save(path_to_qstring(upPath));
    down.save(path_to_qstring(downPath));
    return {path_to_qstring(upPath), path_to_qstring(downPath)};
}

QString toUrlPath(const QString& path) {
    const fs::path abs = fs::absolute(qstring_to_path(path));
    QString result = path_to_qstring(abs);
    return result.replace('\\', '/');
}

} // namespace

void MainWindow::applyTheme(const QString& themeNameIn) {
    const std::string requestedThemeKey = themeNameIn.toStdString();
    const Theme& theme = find_theme(requestedThemeKey);
    const std::string& themeKey = theme.key;
    const QString themeName = QString::fromStdString(themeKey);
    std::string stylesheet = generate_theme_style(theme);

    fs::path assetsDir = m_paths.base_dir / "assets";
    auto [closedPath, openPath] = ensureBranchAssets(
        assetsDir, themeKey, QString::fromStdString(theme.text_primary));
    auto [closedSelPath, openSelPath] = ensureBranchAssets(
        assetsDir, themeKey, QString::fromStdString(theme.text_secondary),
        "selected");
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
        QColor artworkBackground(QString::fromStdString(theme.bg_secondary));
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
            panel->setProperty("goliathEngravedDepth",
                               depth.name(QColor::HexArgb));
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
