#include "catch2/catch.hpp"
#include "ui/application_shortcuts.hpp"
#include "ui/capture_hotkeys.hpp"

#include <set>
#include <string_view>

using goliath::jgrfWarningForHotkey;
using goliath::parseCaptureHotkey;

TEST_CASE("Goliath application shortcuts are present and unique",
          "[hotkeys][ui]") {
    REQUIRE(goliath::kApplicationShortcuts.size() == 7);
    std::set<std::string_view> sequences;
    for (const auto& shortcut : goliath::kApplicationShortcuts) {
        CHECK_FALSE(std::string_view(shortcut.action).empty());
        CHECK_FALSE(std::string_view(shortcut.sequence).empty());
        CHECK(sequences.insert(shortcut.sequence).second);
    }
    CHECK(std::string_view(goliath::kRandomGameShortcut) == "Ctrl+R");
}

TEST_CASE("Windows capture shortcuts accept one modified key and allow disabling",
          "[hotkeys]") {
    QString error;
    const auto gif = parseCaptureHotkey(
        QKeySequence("Ctrl+Alt+F12", QKeySequence::PortableText), &error);
    REQUIRE(gif.has_value());
    REQUIRE(gif->virtualKey == 0x7bu);
    REQUIRE(gif->modifiers == (0x0001u | 0x0002u));
    REQUIRE(error.isEmpty());
    const auto disabled = parseCaptureHotkey(QKeySequence(), &error);
    REQUIRE(disabled.has_value());
    REQUIRE(disabled->virtualKey == 0u);
    const auto single = parseCaptureHotkey(QKeySequence("F1"), &error);
    REQUIRE(single.has_value());
    REQUIRE(single->virtualKey == 0x70u);
    REQUIRE(single->modifiers == 0u);
    REQUIRE(parseCaptureHotkey(QKeySequence(Qt::Key_Semicolon))->virtualKey == 0xbau);
    REQUIRE(parseCaptureHotkey(QKeySequence(Qt::Key_Comma))->virtualKey == 0xbcu);
    REQUIRE(parseCaptureHotkey(QKeySequence(Qt::Key_Period))->virtualKey == 0xbeu);
    const auto commaText = QKeySequence(Qt::Key_Comma).toString(
        QKeySequence::PortableText);
    REQUIRE(QKeySequence(commaText, QKeySequence::PortableText).count() == 1);
    REQUIRE(parseCaptureHotkey(QKeySequence(commaText, QKeySequence::PortableText))
                ->virtualKey == 0xbcu);
    REQUIRE_FALSE(parseCaptureHotkey(QKeySequence("Ctrl+Alt+F1, Ctrl+Alt+F2"),
                                     &error).has_value());
}

TEST_CASE("JGRF direct key overlaps identify their actual action",
          "[hotkeys][jgrf]") {
    REQUIRE(jgrfWarningForHotkey(QKeySequence("Ctrl+Alt+F1"))
                .contains("Soft Reset"));
    REQUIRE(jgrfWarningForHotkey(QKeySequence("Ctrl+Alt+F12"))
                .contains("Toggle Cheats"));
    REQUIRE(jgrfWarningForHotkey(QKeySequence("Ctrl+Alt+G")).isEmpty());
}
