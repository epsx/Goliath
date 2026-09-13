#include "game/jollygood_benchmark.hpp"

#include <charconv>

namespace goliath {

namespace {

std::optional<int> bounded_integer(const QString& text,
                                   int minimum,
                                   int maximum) {
    bool ok = false;
    const int value = text.toInt(&ok);
    if (!ok || value < minimum || value > maximum)
        return std::nullopt;
    return value;
}

} // namespace

std::optional<QStringList> build_jollygood_benchmark_args(
        const QStringList& launchArgs, int frames) {
    if (frames < kBenchmarkMinimumFrames ||
        frames > kBenchmarkMaximumFrames ||
        launchArgs.size() < 2 || launchArgs.first().isEmpty() ||
        launchArgs.last().isEmpty()) {
        return std::nullopt;
    }

    QStringList result = launchArgs;
    const QString media = result.takeLast();
    result << "-b" << QString::number(frames) << media;
    return result;
}

std::optional<std::uint64_t> parse_jollygood_benchmark_completion(
        std::string_view output) {
    constexpr std::string_view marker = "Benchmark completed after ";
    const std::size_t markerPos = output.rfind(marker);
    if (markerPos == std::string_view::npos)
        return std::nullopt;

    const std::size_t digitsBegin = markerPos + marker.size();
    std::size_t digitsEnd = digitsBegin;
    while (digitsEnd < output.size() &&
           output[digitsEnd] >= '0' && output[digitsEnd] <= '9') {
        ++digitsEnd;
    }
    if (digitsEnd == digitsBegin)
        return std::nullopt;

    std::uint64_t frames = 0;
    const char* begin = output.data() + digitsBegin;
    const char* end = output.data() + digitsEnd;
    const auto parsed = std::from_chars(begin, end, frames);
    if (parsed.ec != std::errc{} || parsed.ptr != end || frames == 0)
        return std::nullopt;
    return frames;
}

int effective_jollygood_numeric_option(const QStringList& args,
                                       const QString& shortOption,
                                       const QString& longOption,
                                       int fallback,
                                       int minimum,
                                       int maximum) {
    int result = fallback;
    for (qsizetype i = 1; i < args.size(); ++i) {
        const QString& token = args.at(i);
        std::optional<int> candidate;

        if (token == shortOption || token == longOption) {
            if (i + 1 < args.size())
                candidate = bounded_integer(args.at(++i), minimum, maximum);
        } else if (!shortOption.isEmpty() &&
                   token.startsWith(shortOption) &&
                   token.size() > shortOption.size()) {
            candidate = bounded_integer(
                token.mid(shortOption.size()), minimum, maximum);
        } else {
            const QString prefix = longOption + QStringLiteral("=");
            if (!longOption.isEmpty() && token.startsWith(prefix)) {
                candidate = bounded_integer(
                    token.mid(prefix.size()), minimum, maximum);
            }
        }

        if (candidate.has_value())
            result = *candidate;
    }
    return result;
}

std::optional<JollygoodBenchmarkMetrics> calculate_jollygood_benchmark_metrics(
        std::uint64_t frames, std::int64_t elapsedMilliseconds) {
    if (frames == 0 || elapsedMilliseconds <= 0)
        return std::nullopt;

    JollygoodBenchmarkMetrics result;
    result.frames = frames;
    result.elapsed_milliseconds = elapsedMilliseconds;
    result.frames_per_second =
        static_cast<double>(frames) * 1000.0 /
        static_cast<double>(elapsedMilliseconds);
    result.milliseconds_per_frame =
        static_cast<double>(elapsedMilliseconds) /
        static_cast<double>(frames);
    return result;
}

} // namespace goliath
