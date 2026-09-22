#include "catch2/catch.hpp"

#include "game/command_notation.hpp"

#include <string>

using goliath::CommandGlyph;
using goliath::CommandNotationToken;

TEST_CASE("command notation tokenizes Neo Geo motions and buttons",
          "[command_notation]") {
    const auto tokens = goliath::tokenize_command_notation(
        "Power Wave  _2_3_6_+_A");

    REQUIRE(tokens.size() == 6);
    CHECK(tokens[0].glyph == CommandGlyph::Text);
    CHECK(tokens[0].source == "Power Wave  ");
    CHECK(tokens[1].glyph == CommandGlyph::DirectionDown);
    CHECK(tokens[2].glyph == CommandGlyph::DirectionDownRight);
    CHECK(tokens[3].glyph == CommandGlyph::DirectionRight);
    CHECK(tokens[4].glyph == CommandGlyph::Plus);
    CHECK(tokens[5].glyph == CommandGlyph::ButtonA);
    CHECK(tokens[5].source == "_A");
}

TEST_CASE("command notation maps legacy labels and semantic glyphs",
          "[command_notation]") {
    const auto tokens = goliath::tokenize_command_notation(
        "^E ^J _^ _? _x _X _O ^M");

    REQUIRE(tokens.size() == 15);
    CHECK(tokens[0].glyph == CommandGlyph::ButtonLP);
    CHECK(tokens[2].glyph == CommandGlyph::ButtonHK);
    CHECK(tokens[4].glyph == CommandGlyph::Air);
    CHECK(tokens[6].glyph == CommandGlyph::AnyDirection);
    CHECK(tokens[8].glyph == CommandGlyph::Tap);
    CHECK(tokens[10].glyph == CommandGlyph::Rotate360);
    CHECK(tokens[12].glyph == CommandGlyph::Hold);
    CHECK(tokens[14].glyph == CommandGlyph::Maximum);
}

TEST_CASE("unknown command notation remains exact text",
          "[command_notation]") {
    const std::string input = "Unknown _Q ^Q and UTF-8: â";
    const auto tokens = goliath::tokenize_command_notation(input);

    REQUIRE(tokens.size() == 1);
    CHECK(tokens.front().glyph == CommandGlyph::Text);
    CHECK(tokens.front().source == input);
}

TEST_CASE("command notation recognizes move category prefixes",
          "[command_notation]") {
    const auto tokens = goliath::tokenize_command_notation(
        "_( Throw  _) Command  _@ Special  _* Super  _# Tag  _& SDM  "
        "_> HSDM  ^s Select  ^! Follow-up");

    CHECK(tokens[0].glyph == CommandGlyph::ThrowMove);
    CHECK(tokens[2].glyph == CommandGlyph::CommandMove);
    CHECK(tokens[4].glyph == CommandGlyph::SpecialMove);
    CHECK(tokens[6].glyph == CommandGlyph::SuperMove);
    CHECK(tokens[8].glyph == CommandGlyph::TagMove);
    CHECK(tokens[10].glyph == CommandGlyph::SuperDesperationMove);
    CHECK(tokens[12].glyph == CommandGlyph::HiddenSuperDesperationMove);
    CHECK(tokens[14].glyph == CommandGlyph::Select);
    CHECK(tokens[16].glyph == CommandGlyph::FollowUp);
}

TEST_CASE("command notation recognizes standalone and column headings",
          "[command_notation]") {
    CHECK(goliath::is_command_notation_heading("- CONTROLS -"));
    CHECK(goliath::is_command_notation_heading("  - IORI YAGAMI -  "));
    CHECK(goliath::is_command_notation_heading(
        "- EX IORI YAGAMI \"OROCHI IORI\" -"));
    CHECK_FALSE(goliath::is_command_notation_heading("YAGAMI TEAM"));
    CHECK_FALSE(goliath::is_command_notation_heading("- Power Wave"));
}
