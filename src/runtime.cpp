#include "cpptrace/runtime.hpp"

#include <chrono>
#include <iomanip>
#include <sstream>
#include <thread>

namespace cpptrace {
namespace {

std::uint64_t now_ns() {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
}

std::string current_thread_id() {
    std::ostringstream out;
    out << std::this_thread::get_id();
    return out.str();
}

std::string json_escape(const std::string& value) {
    std::ostringstream out;
    for (unsigned char c : value) {
        switch (c) {
            case '"':
                out << "\\\"";
                break;
            case '\\':
                out << "\\\\";
                break;
            case '\b':
                out << "\\b";
                break;
            case '\f':
                out << "\\f";
                break;
            case '\n':
                out << "\\n";
                break;
            case '\r':
                out << "\\r";
                break;
            case '\t':
                out << "\\t";
                break;
            default:
                if (c < 0x20) {
                    out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                        << static_cast<int>(c) << std::dec;
                } else {
                    out << static_cast<char>(c);
                }
                break;
        }
    }
    return out.str();
}

std::string bytes_to_hex(const std::vector<std::uint8_t>& bytes) {
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (std::uint8_t byte : bytes) {
        out << std::setw(2) << static_cast<unsigned int>(byte);
    }
    return out.str();
}

void write_snapshot(std::ostringstream& out, const ValueSnapshot& snapshot) {
    out << "{";
    out << "\"type\":\"" << json_escape(snapshot.type_name) << "\",";
    out << "\"text\":\"" << json_escape(snapshot.text) << "\",";
    out << "\"raw_hex\":\"" << bytes_to_hex(snapshot.raw_bytes) << "\"";
    out << "}";
}

} // namespace

runtime& runtime::instance() {
    static runtime state;
    return state;
}

std::uint64_t runtime::next_node_id() {
    return next_node_id_.fetch_add(1, std::memory_order_relaxed);
}

void runtime::record(Event event) {
    event.sequence = next_sequence_.fetch_add(1, std::memory_order_relaxed);
    event.timestamp_ns = now_ns();
    event.thread_id = current_thread_id();

    std::lock_guard<std::mutex> lock(mutex_);
    events_.push_back(std::move(event));
}

std::vector<Event> runtime::events() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return events_;
}

void runtime::clear_events() {
    std::lock_guard<std::mutex> lock(mutex_);
    events_.clear();
    next_sequence_.store(0, std::memory_order_relaxed);
}

std::string runtime::to_json() const {
    const auto snapshot = events();
    std::ostringstream out;
    out << "[";
    for (std::size_t i = 0; i < snapshot.size(); ++i) {
        const Event& event = snapshot[i];
        if (i != 0) {
            out << ",";
        }
        out << "{";
        out << "\"sequence\":" << event.sequence << ",";
        out << "\"timestamp_ns\":" << event.timestamp_ns << ",";
        out << "\"thread_id\":\"" << json_escape(event.thread_id) << "\",";
        out << "\"type\":\"" << event_type_name(event.type) << "\",";
        out << "\"node_id\":" << event.node_id << ",";
        out << "\"variable\":\"" << json_escape(event.variable_name) << "\",";
        out << "\"declared_type\":\"" << json_escape(event.type_name) << "\",";
        out << "\"old_value\":";
        write_snapshot(out, event.old_value);
        out << ",\"new_value\":";
        write_snapshot(out, event.new_value);
        out << ",\"location\":{";
        out << "\"file\":\"" << json_escape(event.location.file) << "\",";
        out << "\"line\":" << event.location.line << ",";
        out << "\"function\":\"" << json_escape(event.location.function) << "\"";
        out << "}";
        out << "}";
    }
    out << "]";
    return out.str();
}

} // namespace cpptrace
