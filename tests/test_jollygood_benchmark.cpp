#include "catch2/catch.hpp"

#include "game/jollygood_benchmark.hpp"

using namespace goliath;

TEST_CASE("JGRF benchmark arguments stay before the selected media",
          "[benchmark][launch]") {
    const QStringList launchArgs = {
        "jollygood.exe", "-c", "geolith", "--shader", "5",
        "D:/Neo Geo CD/Metal Slug.cue",
    };

    const auto benchmarkArgs =
        build_jollygood_benchmark_args(launchArgs, 10000);
    REQUIRE(benchmarkArgs.has_value());
    REQUIRE(*benchmarkArgs == QStringList{
        "jollygood.exe", "-c", "geolith", "--shader", "5",
        "-b", "10000", "D:/Neo Geo CD/Metal Slug.cue",
    });
    REQUIRE(benchmarkArgs->last() == "D:/Neo Geo CD/Metal Slug.cue");
    REQUIRE(benchmarkArgs->at(benchmarkArgs->size() - 3) == "-b");

    REQUIRE_FALSE(build_jollygood_benchmark_args(launchArgs, 0).has_value());
    REQUIRE_FALSE(build_jollygood_benchmark_args(
        launchArgs, kBenchmarkMaximumFrames + 1).has_value());
    REQUIRE_FALSE(build_jollygood_benchmark_args({}, 10000).has_value());
    REQUIRE_FALSE(build_jollygood_benchmark_args(
        {"jollygood.exe"}, 10000).has_value());
}

TEST_CASE("JGRF benchmark completion parsing tolerates real log framing",
          "[benchmark][output]") {
    const auto plain = parse_jollygood_benchmark_completion(
        "i: Benchmark completed after 10000 frames\n");
    REQUIRE(plain == std::optional<std::uint64_t>(10000));

    const auto ansi = parse_jollygood_benchmark_completion(
        "\x1b[0mi: setup\n\x1b[0;36mI: Benchmark completed after 30000 frames\n\x1b[0m");
    REQUIRE(ansi == std::optional<std::uint64_t>(30000));

    const auto latest = parse_jollygood_benchmark_completion(
        "Benchmark completed after 5000 frames\n"
        "Benchmark completed after 10000 frames\n");
    REQUIRE(latest == std::optional<std::uint64_t>(10000));

    REQUIRE_FALSE(parse_jollygood_benchmark_completion("normal launch\n").has_value());
    REQUIRE_FALSE(parse_jollygood_benchmark_completion(
        "Benchmark completed after frames\n").has_value());
    REQUIRE_FALSE(parse_jollygood_benchmark_completion(
        "Benchmark completed after 0 frames\n").has_value());
}

TEST_CASE("effective JGRF benchmark labels follow the last valid CLI override",
          "[benchmark][configuration]") {
    const QStringList videoArgs = {
        "jollygood.exe", "--video", "1", "-a2", "--video=3",
        "--video", "9", "game.neo",
    };
    REQUIRE(effective_jollygood_numeric_option(
        videoArgs, "-a", "--video", 0, 0, 3) == 3);

    const QStringList shaderArgs = {
        "jollygood.exe", "-s5", "--shader", "0", "game.neo",
    };
    REQUIRE(effective_jollygood_numeric_option(
        shaderArgs, "-s", "--shader", 2, 0, 6) == 0);
    REQUIRE(effective_jollygood_numeric_option(
        {"jollygood.exe", "game.neo"},
        "-s", "--shader", 2, 0, 6) == 2);
    REQUIRE(effective_jollygood_numeric_option(
        {"jollygood.exe", "--shader=banana", "game.neo"},
        "-s", "--shader", 4, 0, 6) == 4);
}

TEST_CASE("benchmark metrics report effective process throughput",
          "[benchmark][metrics]") {
    const auto metrics =
        calculate_jollygood_benchmark_metrics(10000, 12500);
    REQUIRE(metrics.has_value());
    REQUIRE(metrics->frames == 10000);
    REQUIRE(metrics->elapsed_milliseconds == 12500);
    REQUIRE(metrics->frames_per_second == Approx(800.0));
    REQUIRE(metrics->milliseconds_per_frame == Approx(1.25));

    REQUIRE_FALSE(calculate_jollygood_benchmark_metrics(0, 12500).has_value());
    REQUIRE_FALSE(calculate_jollygood_benchmark_metrics(10000, 0).has_value());
    REQUIRE_FALSE(calculate_jollygood_benchmark_metrics(10000, -1).has_value());
}
