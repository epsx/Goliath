#include "game/command_notation.hpp"

#include <optional>
#include <utility>

namespace goliath {
namespace {

std::optional<CommandGlyph> direction_glyph(char code) {
    switch (code) {
    case '1': return CommandGlyph::DirectionDownLeft;
    case '2': return CommandGlyph::DirectionDown;
    case '3': return CommandGlyph::DirectionDownRight;
    case '4': return CommandGlyph::DirectionLeft;
    case '5': return CommandGlyph::DirectionNeutral;
    case '6': return CommandGlyph::DirectionRight;
    case '7': return CommandGlyph::DirectionUpLeft;
    case '8': return CommandGlyph::DirectionUp;
    case '9': return CommandGlyph::DirectionUpRight;
    default: return std::nullopt;
    }
}

std::optional<CommandGlyph> underscore_glyph(char code) {
    if (const auto direction = direction_glyph(code)) return direction;

    switch (code) {
    case 'N': return CommandGlyph::DirectionNeutral;
    case 'A': return CommandGlyph::ButtonA;
    case 'B': return CommandGlyph::ButtonB;
    case 'C': return CommandGlyph::ButtonC;
    case 'D': return CommandGlyph::ButtonD;
    case 'P': return CommandGlyph::ButtonP;
    case 'K': return CommandGlyph::ButtonK;
    case 'S': return CommandGlyph::ButtonS;
    case 'G': return CommandGlyph::ButtonG;
    case 'H': return CommandGlyph::ButtonH;
    case 'Z': return CommandGlyph::ButtonZ;
    case 'a': return CommandGlyph::Button1;
    case 'b': return CommandGlyph::Button2;
    case 'c': return CommandGlyph::Button3;
    case 'd': return CommandGlyph::Button4;
    case 'e': return CommandGlyph::Button5;
    case 'f': return CommandGlyph::Button6;
    case 'g': return CommandGlyph::Button7;
    case 'h': return CommandGlyph::Button8;
    case 'i': return CommandGlyph::Button9;
    case 'j': return CommandGlyph::Button0;
    case '+': return CommandGlyph::Plus;
    case '.': return CommandGlyph::Ellipsis;
    case '^': return CommandGlyph::Air;
    case '?': return CommandGlyph::AnyDirection;
    case 'x': return CommandGlyph::Tap;
    case 'X': return CommandGlyph::Rotate360;
    case 'O': return CommandGlyph::Hold;
    case '!': return CommandGlyph::FollowUp;
    case '(': return CommandGlyph::ThrowMove;
    case ')': return CommandGlyph::CommandMove;
    case '@': return CommandGlyph::SpecialMove;
    case '*': return CommandGlyph::SuperMove;
    case '#': return CommandGlyph::TagMove;
    case '&': return CommandGlyph::SuperDesperationMove;
    case '>': return CommandGlyph::HiddenSuperDesperationMove;
    case '`': return CommandGlyph::Mode;
    default: return std::nullopt;
    }
}

std::optional<CommandGlyph> caret_glyph(char code) {
    if (const auto direction = direction_glyph(code)) return direction;

    switch (code) {
    case 'E': return CommandGlyph::ButtonLP;
    case 'F': return CommandGlyph::ButtonMP;
    case 'G': return CommandGlyph::ButtonHP;
    case 'H': return CommandGlyph::ButtonLK;
    case 'I': return CommandGlyph::ButtonMK;
    case 'J': return CommandGlyph::ButtonHK;
    case 'T': return CommandGlyph::Button3P;
    case 'U': return CommandGlyph::Button3K;
    case 'V': return CommandGlyph::Button2P;
    case 'W': return CommandGlyph::Button2K;
    case 'M': return CommandGlyph::Maximum;
    case '!': return CommandGlyph::FollowUp;
    case '*': return CommandGlyph::FollowUp;
    case 's': return CommandGlyph::Select;
    case 'S': return CommandGlyph::Select;
    default: return std::nullopt;
    }
}

std::optional<CommandGlyph> marker_glyph(char prefix, char code) {
    if (prefix == '_') return underscore_glyph(code);
    if (prefix == '^') return caret_glyph(code);
    return std::nullopt;
}

} // namespace

std::vector<CommandNotationToken> tokenize_command_notation(
        std::string_view line) {
    std::vector<CommandNotationToken> tokens;
    std::string text;

    const auto flush_text = [&]() {
        if (text.empty()) return;
        tokens.push_back({CommandGlyph::Text, std::move(text)});
        text.clear();
    };

    std::size_t index = 0;
    while (index < line.size()) {
        if ((line[index] == '_' || line[index] == '^') &&
            index + 1 < line.size()) {
            if (const auto glyph = marker_glyph(line[index],
                                                line[index + 1])) {
                flush_text();
                tokens.push_back({*glyph,
                                  std::string(line.substr(index, 2))});
                index += 2;
                continue;
            }
        }
        text.push_back(line[index]);
        ++index;
    }
    flush_text();
    return tokens;
}

bool is_command_notation_heading(std::string_view line) {
    const auto is_space = [](char character) {
        return character == ' ' || character == '\t' ||
               character == '\r' || character == '\n';
    };
    while (!line.empty() && is_space(line.front())) line.remove_prefix(1);
    while (!line.empty() && is_space(line.back())) line.remove_suffix(1);
    return line.size() >= 5 && line.starts_with("- ") &&
           line.ends_with(" -");
}

const char* command_glyph_label(CommandGlyph glyph) {
    switch (glyph) {
    case CommandGlyph::ButtonA: return "A";
    case CommandGlyph::ButtonB: return "B";
    case CommandGlyph::ButtonC: return "C";
    case CommandGlyph::ButtonD: return "D";
    case CommandGlyph::ButtonP: return "P";
    case CommandGlyph::ButtonK: return "K";
    case CommandGlyph::ButtonS: return "S";
    case CommandGlyph::ButtonG: return "G";
    case CommandGlyph::ButtonH: return "H";
    case CommandGlyph::ButtonZ: return "Z";
    case CommandGlyph::ButtonLP: return "LP";
    case CommandGlyph::ButtonMP: return "MP";
    case CommandGlyph::ButtonHP: return "HP";
    case CommandGlyph::ButtonLK: return "LK";
    case CommandGlyph::ButtonMK: return "MK";
    case CommandGlyph::ButtonHK: return "HK";
    case CommandGlyph::Button3P: return "3P";
    case CommandGlyph::Button3K: return "3K";
    case CommandGlyph::Button2P: return "2P";
    case CommandGlyph::Button2K: return "2K";
    case CommandGlyph::Button1: return "1";
    case CommandGlyph::Button2: return "2";
    case CommandGlyph::Button3: return "3";
    case CommandGlyph::Button4: return "4";
    case CommandGlyph::Button5: return "5";
    case CommandGlyph::Button6: return "6";
    case CommandGlyph::Button7: return "7";
    case CommandGlyph::Button8: return "8";
    case CommandGlyph::Button9: return "9";
    case CommandGlyph::Button0: return "0";
    case CommandGlyph::Air: return "AIR";
    case CommandGlyph::AnyDirection: return "DIR";
    case CommandGlyph::Tap: return "TAP";
    case CommandGlyph::Rotate360: return "360";
    case CommandGlyph::Hold: return "HOLD";
    case CommandGlyph::Maximum: return "MAX";
    case CommandGlyph::Select: return "SEL";
    case CommandGlyph::ThrowMove: return "T";
    case CommandGlyph::CommandMove: return "C";
    case CommandGlyph::SpecialMove: return "SP";
    case CommandGlyph::SuperMove: return "SU";
    case CommandGlyph::TagMove: return "TAG";
    case CommandGlyph::SuperDesperationMove: return "SDM";
    case CommandGlyph::HiddenSuperDesperationMove: return "HSDM";
    case CommandGlyph::Mode: return "MODE";
    default: return "";
    }
}

} // namespace goliath
