#include "cpptrace/trace.hpp"

#include <iostream>
#include <string>

int main() {
    cpptrace::clear_events();

    int raw_x = 10;
    int pointed = 3;
    int x_snapshot = 0;
    int heap_snapshot = 0;
    std::string name_snapshot;

    {
        auto tx = CPPTRACE_WRAP(raw_x);
        CPPTRACE_VAR(name, std::string("cpptrace"));
        auto heap_value = cpptrace::box<int>("heap_value", 7, CPPTRACE_HERE);
        cpptrace::ptr<int> tracked_ptr("tracked_ptr", &pointed, CPPTRACE_HERE);

        CPPTRACE_SET(tx, 30);
        CPPTRACE_SET(tx, tx.get() + 12);

        CPPTRACE_SET(name, std::string("cpptrace-v1"));
        heap_value.set(9, CPPTRACE_HERE);
        tracked_ptr.set(4, CPPTRACE_HERE);
        tracked_ptr.reset(nullptr, CPPTRACE_HERE);

        x_snapshot = tx.get();
        heap_snapshot = heap_value.get();
        name_snapshot = name.get();
    }

    std::cout << "Recorded events:\n";
    for (const auto& event : cpptrace::events()) {
        std::cout << "  #" << event.sequence << " node " << event.node_id << " "
                  << event.variable_name << " " << cpptrace::event_type_name(event.type)
                  << " " << event.old_value.text << " -> " << event.new_value.text;
        if (!event.location.file.empty()) {
            std::cout << " at " << event.location.file << ":" << event.location.line;
        }
        std::cout << "\n";
    }

    std::cout << "\nFinal values: raw_x = " << raw_x
              << ", name = " << name_snapshot
              << ", heap_value = " << heap_snapshot
              << ", pointed = " << pointed
              << ", x snapshot = " << x_snapshot << "\n";

    std::cout << "\nJSON:\n" << cpptrace::to_json() << "\n";
    return 0;
}
