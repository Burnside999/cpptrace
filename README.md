# cpptrace

`cpptrace` is a small C++17 debugging helper for explicitly tracked variables. It records when tracked objects are created, assigned, read explicitly, and destroyed, with typed value snapshots, thread ids, monotonic sequence numbers, and JSON export.

## Quick Start

```cpp
#include "cpptrace/trace.hpp"

int raw = 10;
auto traced = CPPTRACE_WRAP(raw);
CPPTRACE_SET(traced, 20);

CPPTRACE_VAR(name, std::string("cpptrace"));
CPPTRACE_SET(name, std::string("cpptrace-v1"));

for (const auto& event : cpptrace::events()) {
    // inspect event.old_value.text and event.new_value.text
}
```

For new values, prefer `cpptrace::var<T>` or `CPPTRACE_VAR`. For existing variables, use `cpptrace::trace_var` or `CPPTRACE_WRAP`. Heap-owned values can use `cpptrace::box<T>`. Pointer rebinding and controlled pointee writes can use `cpptrace::ptr<T>`.

## Boundary

This is not compiler instrumentation. It cannot see raw writes that bypass the wrapper: naked references, naked pointers, `memcpy`, external libraries, or untracked container elements. In C++17, operator assignments also cannot capture caller source locations automatically; use `CPPTRACE_SET` or pass `CPPTRACE_HERE` to `set()` when location matters.