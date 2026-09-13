#include "catch2/catch.hpp"

#include "audio/audio_devices.hpp"
#include "common/goliath_common.hpp"

using namespace goliath;

TEST_CASE("Goliath audio volume is clamped to a safe percentage", "[audio]") {
    REQUIRE(clamp_audio_volume(-50) == 0);
    REQUIRE(clamp_audio_volume(0) == 0);
    REQUIRE(clamp_audio_volume(37) == 37);
    REQUIRE(clamp_audio_volume(100) == 100);
    REQUIRE(clamp_audio_volume(250) == 100);
}

TEST_CASE("audio snapshot reports playback availability from enumerated devices", "[audio]") {
    AudioDeviceSnapshot snapshot;
    REQUIRE_FALSE(snapshot.has_playback_device());

    snapshot.playback_devices.push_back("Test Speakers");
    REQUIRE(snapshot.has_playback_device());
}

TEST_CASE("default Goliath config starts at full audio volume", "[audio][config]") {
    const Config cfg = default_config();
    REQUIRE(cfg.get_int("Audio", "volume", -1) == 100);
}
