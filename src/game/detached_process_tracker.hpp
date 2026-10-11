// detached_process_tracker.hpp — non-owning observation of one detached
// JGRF process. The process is never signaled or terminated by this class.
#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

namespace goliath {

enum class DetachedProcessState {
    Running,
    Exited,
    Unknown,
};

class DetachedProcessTracker {
public:
    DetachedProcessTracker(std::int64_t pid,
                           std::string system,
                           std::string media,
                           std::string display_name = {},
                           std::string effective_system = {},
                           bool mvs_hardware = false,
                           std::uintmax_t log_offset = 0);
    ~DetachedProcessTracker();

    DetachedProcessTracker(const DetachedProcessTracker&) = delete;
    DetachedProcessTracker& operator=(const DetachedProcessTracker&) = delete;
    DetachedProcessTracker(DetachedProcessTracker&& other) noexcept;
    DetachedProcessTracker& operator=(DetachedProcessTracker&& other) noexcept;

    bool valid() const noexcept;
    DetachedProcessState state() const noexcept;
    std::optional<std::uint32_t> exit_code() const noexcept;
    std::int64_t elapsed_seconds() const noexcept;

    std::int64_t pid() const noexcept { return m_pid; }
    const std::string& system() const noexcept { return m_system; }
    const std::string& media() const noexcept { return m_media; }
    const std::string& display_name() const noexcept { return m_displayName; }
    const std::string& effective_system() const noexcept {
        return m_effectiveSystem;
    }
    bool mvs_hardware() const noexcept { return m_mvsHardware; }
    std::uintmax_t log_offset() const noexcept { return m_logOffset; }

private:
    enum class ObserverKind {
        None,
        NativeHandle,
        PidProbe,
    };

    void close_observer() noexcept;

    std::int64_t m_pid = 0;
    std::string m_system;
    std::string m_media;
    std::string m_displayName;
    std::string m_effectiveSystem;
    bool m_mvsHardware = false;
    std::uintmax_t m_logOffset = 0;
    std::chrono::steady_clock::time_point m_started =
        std::chrono::steady_clock::now();
    ObserverKind m_observerKind = ObserverKind::None;
    std::intptr_t m_observer = -1;
};

} // namespace goliath
