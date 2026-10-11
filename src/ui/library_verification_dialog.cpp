#include "ui/library_verification_dialog.hpp"

#include "game/game_model.hpp"
#include "ui/library_details_presentation.hpp"
#include "ui/library_view_logic.hpp"
#include "ui/widgets/title_bar.hpp"

#include <QAbstractItemView>
#include <QClipboard>
#include <QColor>
#include <QDialog>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QHeaderView>
#include <QKeySequence>
#include <QLabel>
#include <QObject>
#include <QPushButton>
#include <QShortcut>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <algorithm>
#include <cstddef>

namespace goliath {

void showLibraryVerificationDialog(
        const QString& title,
        const Rom& rom,
        const LibraryVerificationPresentation& presentation,
        QWidget* parent) {
    QDialog dialog(parent);
    dialog.resize(1380, 540);
    dialog.setMinimumSize(920, 420);
    QVBoxLayout* layout = nullptr;
    setupFramelessDialog(&dialog, "Verification Details", &layout);

    auto* summary = new QLabel(&dialog);
    summary->setWordWrap(true);
    summary->setTextInteractionFlags(
        Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    QString summaryValue;
    if (!presentation.value.empty()) {
        const QString color =
            presentation.tone == VerificationTone::Success
                ? QStringLiteral("#5fd171")
                : presentation.tone == VerificationTone::Error
                      ? QStringLiteral("#ef6262")
                      : QString();
        const QString escapedValue =
            QString::fromStdString(presentation.value).toHtmlEscaped();
        summaryValue = color.isEmpty()
            ? escapedValue
            : QStringLiteral("<span style=\"color:%1; font-weight:600;\">"
                             "%2</span>")
                  .arg(color, escapedValue);
    }
    const QString escapedTitle = title.toHtmlEscaped();
    summary->setText(
        QStringLiteral("<b>%1</b><br>%2&nbsp;&nbsp;%3<br>%4")
            .arg(escapedTitle)
            .arg(QString::fromStdString(presentation.label).toHtmlEscaped())
            .arg(summaryValue)
            .arg(QString::fromStdString(presentation.tooltip)
                     .toHtmlEscaped()));
    layout->addWidget(summary);

    auto* table = new QTableWidget(
        static_cast<int>(rom.hashes.size()), 7, &dialog);
    table->setObjectName("verification_details_table");
    table->setHorizontalHeaderLabels({
        "Type", "Local file", "Catalog file", "Size",
        "Expected", "Calculated", "Status",
    });
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectItems);
    table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table->setAlternatingRowColors(true);
    table->setWordWrap(false);
    table->verticalHeader()->setVisible(false);

    const QFont fixedFont =
        QFontDatabase::systemFont(QFontDatabase::FixedFont);
    auto makeItem = [](const QString& text) {
        auto* cell = new QTableWidgetItem(text);
        cell->setFlags(cell->flags() & ~Qt::ItemIsEditable);
        return cell;
    };

    for (int row = 0; row < static_cast<int>(rom.hashes.size()); ++row) {
        const MediaHashDetail& hash = rom.hashes[static_cast<std::size_t>(row)];
        QString sizeText = hash.size.has_value()
            ? libraryBinarySizeText(*hash.size)
            : QStringLiteral("-");
        if (hash.expected_size.has_value() &&
            (!hash.size.has_value() ||
             *hash.expected_size != *hash.size)) {
            sizeText += QStringLiteral(" / expected ") +
                libraryBinarySizeText(*hash.expected_size);
        }

        table->setItem(row, 0, makeItem(
            QString::fromStdString(hash.role + " / " + hash.algorithm)));
        table->setItem(row, 1, makeItem(
            QString::fromStdString(hash.file)));
        table->setItem(row, 2, makeItem(
            hash.catalog_file.has_value()
                ? QString::fromStdString(*hash.catalog_file)
                : QStringLiteral("-")));
        table->setItem(row, 3, makeItem(sizeText));

        auto* expected = makeItem(
            hash.expected.has_value()
                ? QString::fromStdString(*hash.expected)
                : QStringLiteral("-"));
        expected->setFont(fixedFont);
        table->setItem(row, 4, expected);

        auto* calculated = makeItem(
            hash.actual.has_value()
                ? QString::fromStdString(*hash.actual)
                : QStringLiteral("-"));
        calculated->setFont(fixedFont);
        table->setItem(row, 5, calculated);

        auto* status = makeItem(hash.matched ? "Match" : "Mismatch");
        status->setForeground(
            QColor(hash.matched ? "#5fd171" : "#ef6262"));
        status->setTextAlignment(Qt::AlignCenter);
        table->setItem(row, 6, status);
    }

    QHeaderView* header = table->horizontalHeader();
    header->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(1, QHeaderView::Stretch);
    header->setSectionResizeMode(2, QHeaderView::Stretch);
    header->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(4, QHeaderView::Stretch);
    header->setSectionResizeMode(5, QHeaderView::Stretch);
    header->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    layout->addWidget(table, 1);

    auto copySelectedCells = [table]() {
        QList<QTableWidgetItem*> selected = table->selectedItems();
        if (selected.isEmpty()) return;

        std::sort(selected.begin(), selected.end(),
                  [](const QTableWidgetItem* left,
                     const QTableWidgetItem* right) {
                      if (left->row() != right->row())
                          return left->row() < right->row();
                      return left->column() < right->column();
                  });

        QStringList rows;
        QStringList cells;
        int currentRow = selected.front()->row();
        for (const QTableWidgetItem* item : selected) {
            if (item->row() != currentRow) {
                rows.push_back(cells.join(QChar('\t')));
                cells.clear();
                currentRow = item->row();
            }
            cells.push_back(item->text());
        }
        rows.push_back(cells.join(QChar('\t')));
        QGuiApplication::clipboard()->setText(rows.join(QChar('\n')));
    };

    auto* copyButton = new QPushButton("Copy selected", &dialog);
    copyButton->setToolTip("Copy the selected cells (Ctrl+C)");
    copyButton->setEnabled(false);
    QObject::connect(table, &QTableWidget::itemSelectionChanged,
                     copyButton, [table, copyButton]() {
                         copyButton->setEnabled(
                             !table->selectedItems().isEmpty());
                     });
    QObject::connect(copyButton, &QPushButton::clicked,
                     &dialog, copySelectedCells);

    auto* copyShortcut = new QShortcut(QKeySequence::Copy, table);
    copyShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    QObject::connect(copyShortcut, &QShortcut::activated,
                     &dialog, copySelectedCells);

    auto* closeButton = new QPushButton("Close", &dialog);
    QObject::connect(closeButton, &QPushButton::clicked,
                     &dialog, &QDialog::accept);
    auto* buttonRow = new QHBoxLayout();
    buttonRow->addStretch();
    buttonRow->addWidget(copyButton);
    buttonRow->addWidget(closeButton);
    layout->addLayout(buttonRow);
    dialog.exec();
}

} // namespace goliath
