#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace cpptrace {

enum class EventType {
    Create,
    Assign,
    Destroy,
    Read,
};

inline const char* event_type_name(EventType type) {
    switch (type) {
        case EventType::Create:
            return "create";
        case EventType::Assign:
            return "assign";
        case EventType::Destroy:
            return "destroy";
        case EventType::Read:
            return "read";
    }
    return "unknown";
}

struct SourceLocation {
    std::string file;
    std::uint_least32_t line = 0;
    std::string function;
};

struct ValueSnapshot {
    std::string type_name;
    std::string text;
    std::vector<std::uint8_t> raw_bytes;
};

struct Event {
    EventType type = EventType::Assign;
    std::uint64_t sequence = 0;
    std::uint64_t timestamp_ns = 0;
    std::string thread_id;

    std::uint64_t node_id = 0;
    std::string variable_name;
    std::string type_name;

    ValueSnapshot old_value;
    ValueSnapshot new_value;
    SourceLocation location;
};

} // namespace cpptrace
