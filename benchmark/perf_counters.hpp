#pragma once

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <optional>
#include <stdexcept>
#include <string>

#if defined(__linux__)
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

namespace quickheap::bench {

struct HardwareCounts {
    std::uint64_t cache_references;
    std::uint64_t cache_misses;
    std::uint64_t branch_misses;
};

class PerfCounters {
public:
    explicit PerfCounters(bool enabled) : enabled_(enabled) {
        if (!enabled_) return;
#if defined(__linux__)
        constexpr std::array<std::uint64_t, 3> events = {
            PERF_COUNT_HW_CACHE_REFERENCES, PERF_COUNT_HW_CACHE_MISSES,
            PERF_COUNT_HW_BRANCH_MISSES};
        for (std::size_t i = 0; i < events.size(); ++i) {
            perf_event_attr attr{};
            attr.type = PERF_TYPE_HARDWARE;
            attr.size = sizeof(attr);
            attr.config = events[i];
            attr.disabled = 1;
            attr.exclude_kernel = 1;
            attr.exclude_hv = 1;
            attr.read_format = PERF_FORMAT_GROUP | PERF_FORMAT_TOTAL_TIME_ENABLED |
                               PERF_FORMAT_TOTAL_TIME_RUNNING;
            fds_[i] = static_cast<int>(syscall(SYS_perf_event_open, &attr, 0, -1,
                                              i == 0 ? -1 : fds_[0],
                                              PERF_FLAG_FD_CLOEXEC));
            if (fds_[i] < 0) {
                const std::string reason = std::strerror(errno);
                close_all();
                throw std::runtime_error("--perf cannot open hardware event " +
                                         std::to_string(events[i]) + ": " + reason);
            }
        }
#else
        throw std::runtime_error("--perf requires Linux perf_event_open");
#endif
    }

    PerfCounters(const PerfCounters&) = delete;
    PerfCounters& operator=(const PerfCounters&) = delete;
    ~PerfCounters() { close_all(); }

    void start() {
        if (!enabled_) return;
#if defined(__linux__)
        control(PERF_EVENT_IOC_RESET);
        control(PERF_EVENT_IOC_ENABLE);
#endif
    }

    std::optional<HardwareCounts> stop() {
        if (!enabled_) return std::nullopt;
#if defined(__linux__)
        control(PERF_EVENT_IOC_DISABLE);
        std::array<std::uint64_t, 6> values{};
        ssize_t size;
        do {
            size = read(fds_[0], values.data(), sizeof(values));
        } while (size < 0 && errno == EINTR);
        if (size != static_cast<ssize_t>(sizeof(values)) || values[0] != 3) {
            throw std::runtime_error("--perf could not read complete hardware event group");
        }
        if (values[1] == 0 || values[2] == 0 || values[1] != values[2]) {
            throw std::runtime_error("--perf hardware events unavailable or multiplexed; "
                                     "measurement rejected");
        }
        return HardwareCounts{values[3], values[4], values[5]};
#else
        throw std::runtime_error("--perf requires Linux perf_event_open");
#endif
    }

private:
    bool enabled_;
    std::array<int, 3> fds_ = {-1, -1, -1};

    void close_all() noexcept {
#if defined(__linux__)
        for (auto& fd : fds_) {
            if (fd >= 0) close(fd);
            fd = -1;
        }
#endif
    }

#if defined(__linux__)
    void control(unsigned long request) {
        if (ioctl(fds_[0], request, PERF_IOC_FLAG_GROUP) < 0) {
            throw std::runtime_error("--perf hardware event control failed: " +
                                     std::string(std::strerror(errno)));
        }
    }
#endif
};

}
