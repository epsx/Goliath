// command_dialog.hpp — non-modal command.dat companion for a JGRF session.
#pragma once

#include <QDialog>
#include <QString>
#include <QStringList>

#include <vector>

class QCheckBox;
class QEvent;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QSlider;
class QStackedWidget;
class QWidget;

namespace goliath {

class CommandNotationView;

class CommandDialog final : public QDialog {
public:
    CommandDialog(const QString& gameTitle,
                  const QString& matchedMameId,
                  const QString& commandText,
                  const QString& inheritedStyleSheet);

    void placeBeside(const QWidget* reference, int cascadeIndex = 0);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void applyPresentation();
    void showFindBar();
    void hideFindBar();
    void rebuildFindMatches();
    void findNext(bool backwards);
    void revealFindMatch();

    QString m_inheritedStyleSheet;
    QStringList m_commandLines;
    std::vector<int> m_findMatches;
    int m_findMatchIndex = -1;
    QCheckBox* m_overlayMode = nullptr;
    QSlider* m_backgroundSlider = nullptr;
    QLabel* m_backgroundLabel = nullptr;
    CommandNotationView* m_visualView = nullptr;
    QPlainTextEdit* m_rawView = nullptr;
    QStackedWidget* m_viewStack = nullptr;
    QWidget* m_findBar = nullptr;
    QLineEdit* m_findEdit = nullptr;
    QLabel* m_findStatus = nullptr;
};

} // namespace goliath
