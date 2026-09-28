// command_dialog.hpp — non-modal command.dat companion for a JGRF session.
#pragma once

#include <QDialog>
#include <QString>
#include <QStringList>

#include <vector>

class QCheckBox;
class QCloseEvent;
class QEvent;
class QHideEvent;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QSlider;
class QShowEvent;
class QStackedWidget;
class QWidget;

namespace goliath {

class CommandNotationView;

class CommandDialog final : public QDialog {
public:
    CommandDialog(const QString& gameTitle,
                  const QString& matchedMameId,
                  const QString& commandText,
                  const QString& inheritedStyleSheet,
                  const QString& positionFile,
                  const QString& exactMediaKey);

    void placeBeside(const QWidget* reference, int cascadeIndex = 0);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    void applyPresentation();
    void showFindBar();
    void hideFindBar();
    void rebuildFindMatches();
    void findNext(bool backwards);
    void revealFindMatch();
    void scrollToSourceLine(int sourceLine);
    int currentSourceLine() const;
    void restorePosition();
    void savePosition();

    QString m_inheritedStyleSheet;
    QStringList m_commandLines;
    QString m_positionFile;
    QString m_positionKey;
    bool m_savedForThisDisplay = true;
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
