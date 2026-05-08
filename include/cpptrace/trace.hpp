#pragma once

#include "event.hpp"
#include "runtime.hpp"
#include "trace_node.hpp"
#include "trace_type.hpp"

#include <cstring>
#include <cstdint>
#include <ios>
#include <memory>
#include <ostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <utility>

namespace cpptrace {

namespace detail {

template <typename T, typename = void>
struct is_streamable : std::false_type {};

template <typename T>
struct is_streamable<
    T,
    std::void_t<decltype(std::declval<std::ostream&>() << std::declval<const T&>())>>
    : std::true_type {};

template <typename T>
std::string type_name() {
    using clean_t = typename std::remove_cv<typename std::remove_reference<T>::type>::type;
    return typeid(clean_t).name();
}

template <typename T>
void capture_raw_bytes(ValueSnapshot& snapshot, const T& value) {
    using clean_t = typename std::remove_cv<typename std::remove_reference<T>::type>::type;
    if constexpr (std::is_trivially_copyable<clean_t>::value) {
        snapshot.raw_bytes.resize(sizeof(clean_t));
        std::memcpy(snapshot.raw_bytes.data(), &value, sizeof(clean_t));
    }
}

template <typename T>
std::string pointer_text(T* value) {
    std::ostringstream out;
    out << static_cast<const void*>(value);
    return out.str();
}

template <typename T>
ValueSnapshot make_value_snapshot(const T& value) {
    using clean_t = typename std::remove_cv<typename std::remove_reference<T>::type>::type;

    ValueSnapshot snapshot;
    snapshot.type_name = type_name<clean_t>();

    if constexpr (std::is_pointer<clean_t>::value) {
        snapshot.text = pointer_text(value);
    } else if constexpr (std::is_same<clean_t, bool>::value) {
        std::ostringstream out;
        out << std::boolalpha << value;
        snapshot.text = out.str();
    } else if constexpr (is_streamable<clean_t>::value) {
        std::ostringstream out;
        out << value;
        snapshot.text = out.str();
    } else {
        snapshot.text = "<unprintable>";
    }

    capture_raw_bytes(snapshot, value);
    return snapshot;
}

inline ValueSnapshot empty_snapshot(std::string type_name) {
    ValueSnapshot snapshot;
    snapshot.type_name = std::move(type_name);
    snapshot.text = "<none>";
    return snapshot;
}

inline Event make_event(
    EventType type,
    const Node& node,
    ValueSnapshot old_value,
    ValueSnapshot new_value,
    SourceLocation location) {
    Event event;
    event.type = type;
    event.node_id = node.id;
    event.variable_name = node.name;
    event.type_name = node.type_name;
    event.old_value = std::move(old_value);
    event.new_value = std::move(new_value);
    event.location = std::move(location);
    return event;
}

inline void record_value_event(
    EventType type,
    const Node& node,
    ValueSnapshot old_value,
    ValueSnapshot new_value,
    SourceLocation location) {
    record_event(make_event(
        type, node, std::move(old_value), std::move(new_value), std::move(location)));
}

template <typename T>
void record_create(const Node& node, const T& value, SourceLocation location) {
    record_value_event(
        EventType::Create,
        node,
        empty_snapshot(node.type_name),
        make_value_snapshot(value),
        std::move(location));
}

template <typename T>
void record_destroy(const Node& node, const T& value, SourceLocation location) {
    record_value_event(
        EventType::Destroy,
        node,
        make_value_snapshot(value),
        empty_snapshot(node.type_name),
        std::move(location));
}

template <typename T>
void assign_value(T& target, const T& value, const Node& node, SourceLocation location) {
    auto old_value = make_value_snapshot(target);
    auto new_value = make_value_snapshot(value);
    target = value;
    record_value_event(
        EventType::Assign, node, std::move(old_value), std::move(new_value), std::move(location));
}

template <typename T>
void record_read(const Node& node, const T& value, SourceLocation location) {
    auto snapshot = make_value_snapshot(value);
    record_value_event(
        EventType::Read,
        node,
        snapshot,
        std::move(snapshot),
        std::move(location));
}

inline Node make_node(std::string name, std::string type_name) {
    Node node;
    node.id = next_node_id();
    node.name = std::move(name);
    node.type_name = std::move(type_name);
    return node;
}

} // namespace detail

template <typename T>
class trace {
public:
    trace(T& ref, std::string name = {}, SourceLocation location = {})
        : ptr_(&ref), node_(detail::make_node(std::move(name), detail::type_name<T>())) {
        detail::record_create(node_, *ptr_, std::move(location));
    }

    trace(T& ref, std::uint64_t id, std::string name, SourceLocation location = {})
        : ptr_(&ref), node_(Node{id, std::move(name), detail::type_name<T>()}) {
        detail::record_create(node_, *ptr_, std::move(location));
    }

    trace(const trace&) = delete;
    trace& operator=(const trace&) = delete;

    trace(trace&& other) noexcept
        : ptr_(other.ptr_), node_(std::move(other.node_)), active_(other.active_) {
        other.ptr_ = nullptr;
        other.active_ = false;
    }

    trace& operator=(trace&& other) noexcept {
        if (this != &other) {
            destroy();
            ptr_ = other.ptr_;
            node_ = std::move(other.node_);
            active_ = other.active_;
            other.ptr_ = nullptr;
            other.active_ = false;
        }
        return *this;
    }

    ~trace() {
        destroy();
    }

    operator T() const {
        return *ptr_;
    }

    const T& get() const {
        return *ptr_;
    }

    T read(SourceLocation location = {}) const {
        detail::record_read(node_, *ptr_, std::move(location));
        return *ptr_;
    }

    trace& set(const T& value, SourceLocation location = {}) {
        detail::assign_value(*ptr_, value, node_, std::move(location));
        return *this;
    }

    trace& operator=(const T& value) {
        return set(value);
    }

    template <typename U>
    trace& operator+=(U&& value) {
        T next = static_cast<T>(*ptr_ + std::forward<U>(value));
        return set(next);
    }

    template <typename U>
    trace& operator-=(U&& value) {
        T next = static_cast<T>(*ptr_ - std::forward<U>(value));
        return set(next);
    }

    template <typename U>
    trace& operator*=(U&& value) {
        T next = static_cast<T>(*ptr_ * std::forward<U>(value));
        return set(next);
    }

    template <typename U>
    trace& operator/=(U&& value) {
        T next = static_cast<T>(*ptr_ / std::forward<U>(value));
        return set(next);
    }

    trace& operator++() {
        T next = *ptr_;
        ++next;
        return set(next);
    }

    trace& operator--() {
        T next = *ptr_;
        --next;
        return set(next);
    }

    T operator++(int) {
        T old = *ptr_;
        T next = *ptr_;
        ++next;
        set(next);
        return old;
    }

    T operator--(int) {
        T old = *ptr_;
        T next = *ptr_;
        --next;
        set(next);
        return old;
    }

    std::uint64_t id() const {
        return node_.id;
    }

    const std::string& name() const {
        return node_.name;
    }

private:
    void destroy() noexcept {
        if (!active_ || ptr_ == nullptr) {
            return;
        }
        try {
            detail::record_destroy(node_, *ptr_, {});
        } catch (...) {
        }
        active_ = false;
    }

    T* ptr_ = nullptr;
    Node node_;
    bool active_ = true;
};

template <typename T>
class var {
public:
    var(std::string name, T initial, SourceLocation location = {})
        : value_(std::move(initial)),
          node_(detail::make_node(std::move(name), detail::type_name<T>())) {
        detail::record_create(node_, value_, std::move(location));
    }

    var(const var&) = delete;
    var& operator=(const var&) = delete;

    var(var&& other) noexcept
        : value_(std::move(other.value_)),
          node_(std::move(other.node_)),
          active_(other.active_) {
        other.active_ = false;
    }

    var& operator=(var&& other) noexcept {
        if (this != &other) {
            destroy();
            value_ = std::move(other.value_);
            node_ = std::move(other.node_);
            active_ = other.active_;
            other.active_ = false;
        }
        return *this;
    }

    ~var() {
        destroy();
    }

    operator T() const {
        return value_;
    }

    const T& get() const {
        return value_;
    }

    const T* operator->() const {
        return &value_;
    }

    const T& operator*() const {
        return value_;
    }

    T read(SourceLocation location = {}) const {
        detail::record_read(node_, value_, std::move(location));
        return value_;
    }

    var& set(const T& value, SourceLocation location = {}) {
        detail::assign_value(value_, value, node_, std::move(location));
        return *this;
    }

    var& operator=(const T& value) {
        return set(value);
    }

    template <typename U>
    var& operator+=(U&& value) {
        T next = static_cast<T>(value_ + std::forward<U>(value));
        return set(next);
    }

    template <typename U>
    var& operator-=(U&& value) {
        T next = static_cast<T>(value_ - std::forward<U>(value));
        return set(next);
    }

    template <typename U>
    var& operator*=(U&& value) {
        T next = static_cast<T>(value_ * std::forward<U>(value));
        return set(next);
    }

    template <typename U>
    var& operator/=(U&& value) {
        T next = static_cast<T>(value_ / std::forward<U>(value));
        return set(next);
    }

    var& operator++() {
        T next = value_;
        ++next;
        return set(next);
    }

    var& operator--() {
        T next = value_;
        --next;
        return set(next);
    }

    T operator++(int) {
        T old = value_;
        T next = value_;
        ++next;
        set(next);
        return old;
    }

    T operator--(int) {
        T old = value_;
        T next = value_;
        --next;
        set(next);
        return old;
    }

    std::uint64_t id() const {
        return node_.id;
    }

    const std::string& name() const {
        return node_.name;
    }

private:
    void destroy() noexcept {
        if (!active_) {
            return;
        }
        try {
            detail::record_destroy(node_, value_, {});
        } catch (...) {
        }
        active_ = false;
    }

    T value_;
    Node node_;
    bool active_ = true;
};

template <typename T>
class box {
public:
    box(std::string name, T initial, SourceLocation location = {})
        : value_(new T(std::move(initial))),
          node_(detail::make_node(std::move(name), detail::type_name<T>())) {
        detail::record_create(node_, *value_, std::move(location));
    }

    box(const box&) = delete;
    box& operator=(const box&) = delete;

    box(box&& other) noexcept
        : value_(std::move(other.value_)),
          node_(std::move(other.node_)),
          active_(other.active_) {
        other.active_ = false;
    }

    box& operator=(box&& other) noexcept {
        if (this != &other) {
            destroy();
            value_ = std::move(other.value_);
            node_ = std::move(other.node_);
            active_ = other.active_;
            other.active_ = false;
        }
        return *this;
    }

    ~box() {
        destroy();
    }

    operator T() const {
        return *value_;
    }

    const T& get() const {
        return *value_;
    }

    const T* operator->() const {
        return value_.get();
    }

    const T& operator*() const {
        return *value_;
    }

    box& set(const T& value, SourceLocation location = {}) {
        detail::assign_value(*value_, value, node_, std::move(location));
        return *this;
    }

    box& operator=(const T& value) {
        return set(value);
    }

    std::uint64_t id() const {
        return node_.id;
    }

    const std::string& name() const {
        return node_.name;
    }

private:
    void destroy() noexcept {
        if (!active_ || !value_) {
            return;
        }
        try {
            detail::record_destroy(node_, *value_, {});
        } catch (...) {
        }
        active_ = false;
    }

    std::unique_ptr<T> value_;
    Node node_;
    bool active_ = true;
};

template <typename T>
class ptr {
public:
    ptr(std::string name, T* initial = nullptr, SourceLocation location = {})
        : ptr_(initial), node_(detail::make_node(std::move(name), detail::type_name<T*>())) {
        detail::record_create(node_, ptr_, std::move(location));
    }

    ptr(const ptr&) = delete;
    ptr& operator=(const ptr&) = delete;

    ptr(ptr&& other) noexcept
        : ptr_(other.ptr_), node_(std::move(other.node_)), active_(other.active_) {
        other.ptr_ = nullptr;
        other.active_ = false;
    }

    ptr& operator=(ptr&& other) noexcept {
        if (this != &other) {
            destroy();
            ptr_ = other.ptr_;
            node_ = std::move(other.node_);
            active_ = other.active_;
            other.ptr_ = nullptr;
            other.active_ = false;
        }
        return *this;
    }

    ~ptr() {
        destroy();
    }

    explicit operator bool() const {
        return ptr_ != nullptr;
    }

    const T* get() const {
        return ptr_;
    }

    const T& pointee() const {
        return *ptr_;
    }

    ptr& reset(T* value, SourceLocation location = {}) {
        auto old_value = detail::make_value_snapshot(ptr_);
        auto new_value = detail::make_value_snapshot(value);
        ptr_ = value;
        detail::record_value_event(
            EventType::Assign, node_, std::move(old_value), std::move(new_value), std::move(location));
        return *this;
    }

    bool set(const T& value, SourceLocation location = {}) {
        if (ptr_ == nullptr) {
            return false;
        }
        detail::assign_value(*ptr_, value, node_, std::move(location));
        return true;
    }

    std::uint64_t id() const {
        return node_.id;
    }

    const std::string& name() const {
        return node_.name;
    }

private:
    void destroy() noexcept {
        if (!active_) {
            return;
        }
        try {
            detail::record_value_event(
                EventType::Destroy,
                node_,
                detail::make_value_snapshot(ptr_),
                detail::empty_snapshot(node_.type_name),
                {});
        } catch (...) {
        }
        active_ = false;
    }

    T* ptr_ = nullptr;
    Node node_;
    bool active_ = true;
};

template <typename T>
trace<T> trace_var(T& ref, const std::string& name = {}, SourceLocation location = {}) {
    return trace<T>(ref, name, std::move(location));
}

template <typename T>
var<typename std::decay<T>::type> make_var(
    const std::string& name,
    T&& value,
    SourceLocation location = {}) {
    using value_t = typename std::decay<T>::type;
    return var<value_t>(name, value_t(std::forward<T>(value)), std::move(location));
}

template <typename T>
box<typename std::decay<T>::type> make_box(
    const std::string& name,
    T&& value,
    SourceLocation location = {}) {
    using value_t = typename std::decay<T>::type;
    return box<value_t>(name, value_t(std::forward<T>(value)), std::move(location));
}

} // namespace cpptrace

#define CPPTRACE_HERE \
    ::cpptrace::SourceLocation{__FILE__, static_cast<std::uint_least32_t>(__LINE__), __func__}

#define CPPTRACE_VAR(name, value) \
    auto name = ::cpptrace::make_var(#name, (value), CPPTRACE_HERE)

#define CPPTRACE_WRAP(name) \
    ::cpptrace::trace_var((name), #name, CPPTRACE_HERE)

#define CPPTRACE_SET(target, value) \
    (target).set((value), CPPTRACE_HERE)

#define CPPTRACE_PTR(name, value) \
    auto name = ::cpptrace::ptr<std::remove_pointer<decltype(value)>::type>(#name, (value), CPPTRACE_HERE)
