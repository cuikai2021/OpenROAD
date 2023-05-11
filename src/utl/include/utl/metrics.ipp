#include <string_view>
#include <string>
#include <variant>
#include <map>
#include <deque>
#include <cinttypes>
#include <iostream>
#include <type_traits>

namespace metrics {
class Reporter;

namespace impl {

class Value;

struct PrimitiveAssign {
    explicit PrimitiveAssign(Value&);
    void set_int(int64_t);
    void set_str(std::string_view);
    void set_bool(bool);
    void set_float(double);

    Value& v_;
};

template<class T>
constexpr bool is_int() {
    return std::is_integral_v<T> && !std::is_same_v<T, bool>;
}

template<class T>
constexpr bool is_str() {
    return std::is_same_v<T, std::string>
        || std::is_same_v<T, std::string_view>
        || std::is_same_v<T, char const *>;
}


class Value {
public:
    Value();
    ~Value();

    template<class T>
    Value& operator=(T v) noexcept {
        PrimitiveAssign ass(*this);

        static_assert(
            is_int<T>()
            || is_str<T>()
            || std::is_same_v<T, bool>
            || std::is_floating_point_v<T>
            , "unsupported type");

        if constexpr (is_int<T>()) {
            ass.set_int(v);
        } else if constexpr (is_str<T>()) {
            ass.set_str(v);
        } else if constexpr (std::is_same_v<T, bool>) {
            ass.set_bool(v);
        } else if constexpr (std::is_floating_point_v<T>) {
            ass.set_float(v);
        }

        return *this;
    }

    Value& operator[](const std::string_view&) noexcept;
    Value& append() noexcept;

public:
    // for internal use only
    void dump(std::ostream&, int64_t indent) const noexcept;

    struct Unset {};

    std::variant<
        Unset,
        int64_t,
        double,
        bool,
        std::string,
        std::map<std::string, Value>,
        std::deque<Value>> value_;
};

} // namespace impl
} // namespace metrics
