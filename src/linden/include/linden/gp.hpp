#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <filesystem>

namespace linden::gp {

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
    // If unset, it is regarded as uniform.
    // It is uniform, by default.
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
            DIRICHLET,
            EIGEN_VEC,
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
        void prettyPrint(std::string&) const noexcept;
    };
    InitialPlacePolicy initial_place_policy;

    bool timing_driven;

    // When `continuation` is set, linden GP will store its
    // inner state to this file.
    // So, if `linden_global_placement` is called with `-continue`,
    // it will restore the previous state and move forward.
    //
    // `continue_mode` requires this feature.
    // Please refer to `continue_mode` for details.
    //
    // `congestion_repair` requires this feature.
    // If `congestion_repair` is invoked without explicitly setting
    // this option, a default continuation file will be used.
    std::optional<std::filesystem::path> continuation;


    // Typically, one will do the following steps:
    // 1. Globally set `continuation` (eg., by TCL cmd `linden::set_continuation`)
    //    and target density (eg., by TCL cmd `linden::set_to_uniform_density` or `linden::set_density`).
    // 2. run linden GP with a higher target overflow and without continue-mode.
    //    (e.g., by TCL cmd `linden_global_placement -overflow 0.9`).
    // 3. run linden GP with a lower target overflow and with continue-mode.
    //    (e.g., by TCL cmd `linden_global_placement -overflow 0.8 -continue`).
    bool continue_mode;

    bool routability_driven;

    // The overflow threshold to start estimating congestion map.
    float overflow_to_estimate_cmap;
    std::optional<std::filesystem::path> ldb_path;

    // Name of the highest layer for (baby) banyan to estimate routability.
    std::optional<std::string> highest_layer_to_route;
    OptionalSetter<std::string> mut_highest_layer_to_route() noexcept;

    Config();

    void prettyPrint(std::string&) const noexcept;

    static std::shared_ptr<Config> get_default() noexcept;
};

void linden_global_place(const Config&) noexcept;

/**
 * Generates a random but stable (global) placement.
 */
void random_place() noexcept;

} // namespace linden::gp