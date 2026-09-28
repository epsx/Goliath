// Portable validation for Goliath's capture keys.
#pragma once

#include "input/input_maps.hpp"

#include <QKeySequence>
#include <QString>

#include <optional>

namespace goliath {

struct CaptureHotkey {
    unsigned int modifiers = 0;
    unsigned int virtualKey = 0;
};

inline std::optional<CaptureHotkey> parseCaptureHotkey(
    const QKeySequence& sequence, QString* error = nullptr) {
    if (error) error->clear();
    if (sequence.isEmpty()) return CaptureHotkey{}; // Disabled.
    if (sequence.count() != 1) {
        if (error) *error = "Use one key combination, not a sequence.";
        return std::nullopt;
    }
    const QKeyCombination combination = sequence[0];
    const auto modifiers = combination.keyboardModifiers();
    if (modifiers & ~(Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier)) {
        if (error) *error = "Use Ctrl, Alt, and Shift only; the Meta/Super key is reserved.";
        return std::nullopt;
    }
    const int key = combination.key();
    unsigned int virtualKey = 0;
    if (key >= Qt::Key_A && key <= Qt::Key_Z) virtualKey = static_cast<unsigned int>(key);
    else if (key >= Qt::Key_0 && key <= Qt::Key_9)
        virtualKey = static_cast<unsigned int>(key);
    else if (key >= Qt::Key_F1 && key <= Qt::Key_F24)
        virtualKey = 0x70u + static_cast<unsigned int>(key - Qt::Key_F1);
    else if (key == Qt::Key_Semicolon) virtualKey = 0xbau; // VK_OEM_1
    else if (key == Qt::Key_Comma) virtualKey = 0xbcu; // VK_OEM_COMMA
    else if (key == Qt::Key_Period) virtualKey = 0xbeu; // VK_OEM_PERIOD
    else if (key == Qt::Key_Equal) virtualKey = 0xbbu; // VK_OEM_PLUS
    else if (key == Qt::Key_Minus) virtualKey = 0xbdu; // VK_OEM_MINUS
    else if (key == Qt::Key_Slash) virtualKey = 0xbfu; // VK_OEM_2
    else if (key == Qt::Key_QuoteLeft) virtualKey = 0xc0u; // VK_OEM_3
    else if (key == Qt::Key_BracketLeft) virtualKey = 0xdbu; // VK_OEM_4
    else if (key == Qt::Key_Backslash) virtualKey = 0xdcu; // VK_OEM_5
    else if (key == Qt::Key_BracketRight) virtualKey = 0xddu; // VK_OEM_6
    else if (key == Qt::Key_Apostrophe) virtualKey = 0xdeu; // VK_OEM_7
    if (!virtualKey) {
        if (error) *error = "Choose a letter, number, F1-F24, or common punctuation key.";
        return std::nullopt;
    }
    unsigned int winModifiers = 0;
    if (modifiers & Qt::AltModifier) winModifiers |= 0x0001u; // MOD_ALT
    if (modifiers & Qt::ControlModifier) winModifiers |= 0x0002u; // MOD_CONTROL
    if (modifiers & Qt::ShiftModifier) winModifiers |= 0x0004u; // MOD_SHIFT
    return CaptureHotkey{winModifiers, virtualKey};
}

inline QString jgrfWarningForHotkey(const QKeySequence& sequence) {
    if (sequence.isEmpty() || sequence.count() != 1) return {};
    const int key = sequence[0].key();
    const auto it = qt_key_to_sdl_scancode().find(key);
    if (it == qt_key_to_sdl_scancode().end()) return {};
    const auto action = jgrf_hotkey_action(it->second);
    if (!action) return {};
    const QString keyName = QKeySequence(key).toString(QKeySequence::NativeText);
    return QString("%1 is also used by JGRF: %2. This capture shortcut may "
                   "take precedence while a game is running.")
        .arg(keyName, QString::fromStdString(*action));
}

} // namespace goliath
