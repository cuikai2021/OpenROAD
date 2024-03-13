#pragma once

#include <memory>
#include <optional>
#include <string_view>

namespace linden {
namespace gp {

template<class T>
class OptionalSetter {
public:
    explicit OptionalSetter(std::optional<T>& out, T init)
    :   out_(out),
        buf_(std::move(init))
    {}

    OptionalSetter(OptionalSetter<T>&&) =default;
    OptionalSetter<T>& operator=(OptionalSetter<T>&&) =delete;

    OptionalSetter() =delete;
    OptionalSetter(const OptionalSetter<T>&) =delete;
    OptionalSetter<T>& operator=(const OptionalSetter<T>&) =delete;

    ~OptionalSetter() {
        out_ = std::move(buf_);
    }

    T& operator=(const T& v) noexcept {
        buf_ = v;
        return buf_;
    }

    T& operator=(T&& v) noexcept {
        buf_ = v;
        return buf_;
    }

private:
    std::optional<T>& out_;
    T buf_;
};

struct Config {
    // if unset, it is regarded as uniform.
    std::optional<float> density;

    bool is_uniform() const noexcept {
        return !density;
    }

    void set_to_uniform() noexcept {
        density.reset();
    }

    OptionalSetter<float> mut_density() noexcept;

    float overflow;

    struct InitialPlacePolicy {
        enum V {
            DEF,
            // FIXME: to enable this
            // EIGEN_VEC,
            DIRICHLET,
        };

        V value;

        explicit InitialPlacePolicy()
        :   value(DIRICHLET)
        {}

        explicit InitialPlacePolicy(InitialPlacePolicy::V v)
        :   value(v)
        {}

        InitialPlacePolicy(const InitialPlacePolicy&) =default;
        InitialPlacePolicy(InitialPlacePolicy&&) =default;
        InitialPlacePolicy& operator=(const InitialPlacePolicy&) =default;
        InitialPlacePolicy& operator=(InitialPlacePolicy&&) =default;
        ~InitialPlacePolicy() =default;

        static InitialPlacePolicy from_str(const std::string_view&) noexcept;

        std::string_view to_str_view() const noexcept;
    };
    InitialPlacePolicy initial_place_policy;


    Config();

    static std::shared_ptr<Config> get_default() noexcept;
};

} // namespace gp
} // namespace linden