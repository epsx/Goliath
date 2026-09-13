// detached_process_tracker.hpp — non-owning observation of one detached
// JGRF process. The process is never signaled or terminated by this class.
#pragma once

#include <chrono>
#include <cstdint>
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
                           std::string media);
    ~DetachedProcessTracker();

    DetachedProcessTracker(const DetachedProcessTracker&) = delete;
    DetachedProcessTracker& operator=(const DetachedProcessTracker&) = delete;
    DetachedProcessTracker(DetachedProcessTracker&& other) noexcept;
    DetachedProcessTracker& operator=(DetachedProcessTracker&& other) noexcept;

    bool valid() const noexcept;
    DetachedProcessState state() const noexcept;
    std::int64_t elapsed_seconds() const noexcept;

    std::int64_t pid() const noexcept { return m_pid; }
    const std::string& system() const noexcept { return m_system; }
    const std::string& media() const noexcept { return m_media; }

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
    std::chrono::steady_clock::time_point m_started =
        std::chrono::steady_clock::now();
    ObserverKind m_observerKind = ObserverKind::None;
    std::intptr_t m_observer = -1;
};

} // namespace goliath
