#include "cpptrace/trace.hpp"

#include <cassert>
#include <ostream>
#include <string>
#include <thread>
#include <vector>

namespace {

struct Point {
    int x = 0;
    int y = 0;
};

std::ostream& operator<<(std::ostream& out, const Point& point) {
    out << "(" << point.x << "," << point.y << ")";
    return out;
}

std::size_t count_events(cpptrace::EventType type) {
    std::size_t count = 0;
    for (const auto& event : cpptrace::events()) {
        if (event.type == type) {
            ++count;
        }
    }
    return count;
}

void test_basic_assignments() {
    cpptrace::clear_events();
    {
        auto value = cpptrace::var<int>("value", 1, CPPTRACE_HERE);
        value = 2;
        value += 3;
        ++value;
        assert(value.get() == 6);
    }

    assert(count_events(cpptrace::EventType::Create) == 1);
    assert(count_events(cpptrace::EventType::Assign) == 3);
    assert(count_events(cpptrace::EventType::Destroy) == 1);

    const auto events = cpptrace::events();
    assert(events[0].variable_name == "value");
    assert(events[0].new_value.text == "1");
    assert(events[1].old_value.text == "1");
    assert(events[1].new_value.text == "2");
}

void test_typed_snapshots() {
    cpptrace::clear_events();
    {
        auto name = cpptrace::var<std::string>("name", "alpha", CPPTRACE_HERE);
        name.set("beta", CPPTRACE_HERE);

        auto point = cpptrace::var<Point>("point", Point{1, 2}, CPPTRACE_HERE);
        point.set(Point{3, 4}, CPPTRACE_HERE);
    }

    const auto events = cpptrace::events();
    bool saw_string = false;
    bool saw_point = false;
    for (const auto& event : events) {
        if (event.variable_name == "name" && event.new_value.text == "beta") {
            saw_string = true;
        }
        if (event.variable_name == "point" && event.new_value.text == "(3,4)") {
            saw_point = true;
        }
    }
    assert(saw_string);
    assert(saw_point);
}

void test_raw_reference_bypass_is_not_traced() {
    cpptrace::clear_events();
    int raw = 1;
    {
        auto traced = cpptrace::trace_var(raw, "raw", CPPTRACE_HERE);
        raw = 9;
        traced = 10;
    }

    assert(raw == 10);
    assert(count_events(cpptrace::EventType::Assign) == 1);
}

void test_json_escaping_and_location() {
    cpptrace::clear_events();
    {
        auto text = cpptrace::var<std::string>("text", "a\nb", CPPTRACE_HERE);
        text.set("quote: \"", CPPTRACE_HERE);
    }

    const std::string json = cpptrace::to_json();
    assert(json.find("\\n") != std::string::npos);
    assert(json.find("\\\"") != std::string::npos);
    assert(json.find("\"line\":") != std::string::npos);
}

void test_explicit_read_event() {
    cpptrace::clear_events();
    {
        auto value = cpptrace::var<int>("value", 42, CPPTRACE_HERE);
        assert(value.read(CPPTRACE_HERE) == 42);
    }

    assert(count_events(cpptrace::EventType::Read) == 1);
}

void test_box_and_ptr() {
    cpptrace::clear_events();
    int external = 4;
    {
        auto heap_value = cpptrace::box<int>("heap_value", 5, CPPTRACE_HERE);
        heap_value.set(6, CPPTRACE_HERE);

        cpptrace::ptr<int> tracked_ptr("tracked_ptr", &external, CPPTRACE_HERE);
        assert(tracked_ptr.set(7, CPPTRACE_HERE));
        tracked_ptr.reset(nullptr, CPPTRACE_HERE);
        assert(!tracked_ptr.set(8, CPPTRACE_HERE));
    }

    assert(external == 7);
    assert(count_events(cpptrace::EventType::Create) == 2);
    assert(count_events(cpptrace::EventType::Assign) == 3);
    assert(count_events(cpptrace::EventType::Destroy) == 2);
}

void test_thread_safe_runtime() {
    cpptrace::clear_events();

    std::vector<std::thread> threads;
    for (int thread_index = 0; thread_index < 4; ++thread_index) {
        threads.emplace_back([thread_index]() {
            auto value = cpptrace::var<int>("thread_value", thread_index, CPPTRACE_HERE);
            for (int i = 0; i < 25; ++i) {
                value.set(thread_index * 100 + i, CPPTRACE_HERE);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    assert(count_events(cpptrace::EventType::Assign) == 100);
    assert(count_events(cpptrace::EventType::Create) == 4);
    assert(count_events(cpptrace::EventType::Destroy) == 4);
}

} // namespace

int main() {
    test_basic_assignments();
    test_typed_snapshots();
    test_raw_reference_bypass_is_not_traced();
    test_json_escaping_and_location();
    test_explicit_read_event();
    test_box_and_ptr();
    test_thread_safe_runtime();
    return 0;
}
