#pragma once

#include "event.hpp"

#include <atomic>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace cpptrace {

class runtime {
public:
    static runtime& instance();

    std::uint64_t next_node_id();
    void record(Event event);

    std::vector<Event> events() const;
    void clear_events();
    std::string to_json() const;

private:
    runtime() = default;

    mutable std::mutex mutex_;
    std::vector<Event> events_;
    std::atomic<std::uint64_t> next_node_id_{0};
    std::atomic<std::uint64_t> next_sequence_{0};
};

inline std::uint64_t next_node_id() {
    return runtime::instance().next_node_id();
}

inline void record_event(Event event) {
    runtime::instance().record(std::move(event));
}

inline std::vector<Event> events() {
    return runtime::instance().events();
}

inline std::vector<Event> get_events() {
    return events();
}

inline void clear_events() {
    runtime::instance().clear_events();
}

inline std::string to_json() {
    return runtime::instance().to_json();
}

class session {
public:
    explicit session(bool clear_on_start = true) {
        if (clear_on_start) {
            clear_events();
        }
    }

    std::vector<Event> events() const {
        return cpptrace::events();
    }

    std::string to_json() const {
        return cpptrace::to_json();
    }
};

} // namespace cpptrace
