// command_notation.hpp — command.dat marker tokenization for visual rendering.
#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace goliath {

enum class CommandGlyph {
    Text,
    DirectionDownLeft,
    DirectionDown,
    DirectionDownRight,
    DirectionLeft,
    DirectionNeutral,
    DirectionRight,
    DirectionUpLeft,
    DirectionUp,
    DirectionUpRight,
    ButtonA,
    ButtonB,
    ButtonC,
    ButtonD,
    ButtonP,
    ButtonK,
    ButtonS,
    ButtonG,
    ButtonH,
    ButtonZ,
    ButtonLP,
    ButtonMP,
    ButtonHP,
    ButtonLK,
    ButtonMK,
    ButtonHK,
    Button3P,
    Button3K,
    Button2P,
    Button2K,
    Button1,
    Button2,
    Button3,
    Button4,
    Button5,
    Button6,
    Button7,
    Button8,
    Button9,
    Button0,
    Plus,
    Ellipsis,
    Air,
    AnyDirection,
    Tap,
    Rotate360,
    Hold,
    Maximum,
    FollowUp,
    Select,
    ThrowMove,
    CommandMove,
    SpecialMove,
    SuperMove,
    TagMove,
    SuperDesperationMove,
    HiddenSuperDesperationMove,
    Mode,
};

struct CommandNotationToken {
    CommandGlyph glyph = CommandGlyph::Text;
    // Text tokens contain display text. Glyph tokens retain their original
    // two-byte marker so Raw/diagnostic fallbacks never need to reconstruct it.
    std::string source;
};

std::vector<CommandNotationToken> tokenize_command_notation(
    std::string_view line);

bool is_command_notation_heading(std::string_view line);

const char* command_glyph_label(CommandGlyph glyph);

} // namespace goliath
