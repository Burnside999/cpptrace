#pragma once

#include <cstdint>
#include <string>

namespace cpptrace {

struct Node {
    std::uint64_t id = 0;
    std::string name;
    std::string type_name;
};

} // namespace cpptrace
