#pragma once

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QSize>

class QColor;
class QPaintEvent;
class QPainter;
class QRectF;

namespace goliath {

class EngravedTextLabel final : public QLabel {
public:
    explicit EngravedTextLabel(const QString& text = {},
                               QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;
};

class EngravedComboBox final : public QComboBox {
public:
    explicit EngravedComboBox(QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;
};

class EngravedCheckBox final : public QCheckBox {
public:
    explicit EngravedCheckBox(const QString& label,
                              QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;
};

class EngravedTextButton final : public QPushButton {
public:
    explicit EngravedTextButton(const QString& label = {},
                                QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    static void drawCircledStar(QPainter& painter, const QRectF& bounds,
                                const QColor& color, bool filled);
};

class EngravedActionButton final : public QPushButton {
public:
    enum class Glyph { Tools, Settings, About, Filters, Random, Alerts };

    EngravedActionButton(const QString& label, Glyph glyph,
                         QWidget* parent = nullptr);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void drawGlyph(QPainter& painter, const QRectF& bounds,
                   const QColor& color) const;

    Glyph m_glyph;
};

} // namespace goliath
