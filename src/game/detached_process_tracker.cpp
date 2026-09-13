#include "game/detached_process_tracker.hpp"

#include <cerrno>
#include <limits>
#include <utility>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <csignal>
#include <sys/types.h>
#if defined(__linux__)
#include <poll.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif
#endif

namespace goliath {

namespace {

#if !defined(_WIN32)
DetachedProcessState probe_pid(std::int64_t pid) noexcept {
    if (pid <= 0 ||
        pid > static_cast<std::int64_t>(
                  std::numeric_limits<pid_t>::max())) {
        return DetachedProcessState::Unknown;
    }

    if (::kill(static_cast<pid_t>(pid), 0) == 0)
        return DetachedProcessState::Running;
    if (errno == EPERM)
        return DetachedProcessState::Running;
    if (errno == ESRCH)
        return DetachedProcessState::Exited;
    return DetachedProcessState::Unknown;
}
#endif

} // namespace

DetachedProcessTracker::DetachedProcessTracker(std::int64_t pid,
                                               std::string system,
                                               std::string media)
    : m_pid(pid),
      m_system(std::move(system)),
      m_media(std::move(media)),
      m_started(std::chrono::steady_clock::now()) {
    if (m_pid <= 0) return;

#if defined(_WIN32)
    if (m_pid > static_cast<std::int64_t>(
                    std::numeric_limits<DWORD>::max())) {
        return;
    }

    // Retaining a SYNCHRONIZE handle binds observation to this exact process
    // object. A later process reusing the numeric PID cannot extend the
    // measured session accidentally.
    HANDLE handle = OpenProcess(
        SYNCHRONIZE, FALSE, static_cast<DWORD>(m_pid));
    if (!handle) return;

    m_observerKind = ObserverKind::NativeHandle;
    m_observer = reinterpret_cast<std::intptr_t>(handle);
#elif defined(__linux__) && defined(SYS_pidfd_open)
    if (m_pid <= static_cast<std::int64_t>(
                     std::numeric_limits<pid_t>::max())) {
        const int pidfd = static_cast<int>(
            ::syscall(SYS_pidfd_open, static_cast<pid_t>(m_pid), 0));
        if (pidfd >= 0) {
            m_observerKind = ObserverKind::NativeHandle;
            m_observer = pidfd;
            return;
        }
    }

    // Older kernels do not provide pidfds. The portable fallback is
    // best-effort and cannot eliminate the small PID-reuse race completely.
    if (probe_pid(m_pid) == DetachedProcessState::Running)
        m_observerKind = ObserverKind::PidProbe;
#else
    if (probe_pid(m_pid) == DetachedProcessState::Running)
        m_observerKind = ObserverKind::PidProbe;
#endif
}

DetachedProcessTracker::~DetachedProcessTracker() {
    close_observer();
}

DetachedProcessTracker::DetachedProcessTracker(
        DetachedProcessTracker&& other) noexcept
    : m_pid(other.m_pid),
      m_system(std::move(other.m_system)),
      m_media(std::move(other.m_media)),
      m_started(other.m_started),
      m_observerKind(other.m_observerKind),
      m_observer(other.m_observer) {
    other.m_pid = 0;
    other.m_observerKind = ObserverKind::None;
    other.m_observer = -1;
}

DetachedProcessTracker& DetachedProcessTracker::operator=(
        DetachedProcessTracker&& other) noexcept {
    if (this == &other) return *this;

    close_observer();
    m_pid = other.m_pid;
    m_system = std::move(other.m_system);
    m_media = std::move(other.m_media);
    m_started = other.m_started;
    m_observerKind = other.m_observerKind;
    m_observer = other.m_observer;

    other.m_pid = 0;
    other.m_observerKind = ObserverKind::None;
    other.m_observer = -1;
    return *this;
}

bool DetachedProcessTracker::valid() const noexcept {
    return m_observerKind != ObserverKind::None;
}

DetachedProcessState DetachedProcessTracker::state() const noexcept {
    if (!valid()) return DetachedProcessState::Unknown;

#if defined(_WIN32)
    if (m_observerKind != ObserverKind::NativeHandle)
        return DetachedProcessState::Unknown;

    HANDLE handle = reinterpret_cast<HANDLE>(m_observer);
    const DWORD result = WaitForSingleObject(handle, 0);
    if (result == WAIT_TIMEOUT) return DetachedProcessState::Running;
    if (result == WAIT_OBJECT_0) return DetachedProcessState::Exited;
    return DetachedProcessState::Unknown;
#elif defined(__linux__) && defined(SYS_pidfd_open)
    if (m_observerKind == ObserverKind::NativeHandle) {
        pollfd descriptor{};
        descriptor.fd = static_cast<int>(m_observer);
        descriptor.events = POLLIN;
        const int result = ::poll(&descriptor, 1, 0);
        if (result == 0) return DetachedProcessState::Running;
        if (result > 0 &&
            (descriptor.revents & (POLLIN | POLLHUP | POLLERR)) != 0) {
            return DetachedProcessState::Exited;
        }
        return DetachedProcessState::Unknown;
    }
    return probe_pid(m_pid);
#else
    return probe_pid(m_pid);
#endif
}

std::int64_t DetachedProcessTracker::elapsed_seconds() const noexcept {
    const auto elapsed = std::chrono::steady_clock::now() - m_started;
    const auto seconds =
        std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
    return seconds < 0 ? 0 : static_cast<std::int64_t>(seconds);
}

void DetachedProcessTracker::close_observer() noexcept {
    if (m_observerKind != ObserverKind::NativeHandle) {
        m_observerKind = ObserverKind::None;
        m_observer = -1;
        return;
    }

#if defined(_WIN32)
    if (m_observer != -1)
        CloseHandle(reinterpret_cast<HANDLE>(m_observer));
#elif defined(__linux__) && defined(SYS_pidfd_open)
    if (m_observer >= 0)
        ::close(static_cast<int>(m_observer));
#endif

    m_observerKind = ObserverKind::None;
    m_observer = -1;
}

} // namespace goliath
