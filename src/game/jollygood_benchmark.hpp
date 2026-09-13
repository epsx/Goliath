// jollygood_benchmark.hpp — deterministic helpers for JGRF benchmark launch
// arguments, completion detection, and throughput calculations. Process
// ownership and presentation remain in the UI layer.
#pragma once

#include <QString>
#include <QStringList>

#include <cstdint>
#include <optional>
#include <string_view>

namespace goliath {

constexpr int kBenchmarkMinimumFrames = 1;
constexpr int kBenchmarkMaximumFrames = 10000000;
constexpr int kBenchmarkDefaultFrames = 10000;

// JGRF expects every option before the final media argument. The short -b
// spelling is intentional: JGRF 2.0.1 declares --benchmark but its help text
// calls the same option --bmark.
std::optional<QStringList> build_jollygood_benchmark_args(
    const QStringList& launchArgs, int frames);

// Reads the frame count from JGRF's authoritative completion message. ANSI
// color sequences around the message are harmless because only the marker and
// following decimal digits are inspected.
std::optional<std::uint64_t> parse_jollygood_benchmark_completion(
    std::string_view output);

// Resolve the last valid numeric CLI override, matching getopt's repeated-
// option behavior. Supports separate, attached short, and --long=value forms.
int effective_jollygood_numeric_option(const QStringList& args,
                                       const QString& shortOption,
                                       const QString& longOption,
                                       int fallback,
                                       int minimum,
                                       int maximum);

struct JollygoodBenchmarkMetrics {
    std::uint64_t frames = 0;
    std::int64_t elapsed_milliseconds = 0;
    double frames_per_second = 0.0;
    double milliseconds_per_frame = 0.0;
};

std::optional<JollygoodBenchmarkMetrics> calculate_jollygood_benchmark_metrics(
    std::uint64_t frames, std::int64_t elapsedMilliseconds);

} // namespace goliath
