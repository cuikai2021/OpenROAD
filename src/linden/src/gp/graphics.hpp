#pragma once
#include "linden/db/db.hpp"
#include <memory>

namespace linden::gp {

class GraphicsInner;

class Graphics {
public:
    explicit Graphics(std::shared_ptr<linden::db::Database>);
    ~Graphics();

    static bool enabled() noexcept;

    void one_more_step(size_t iter) noexcept;
    void finalize() noexcept;

private:
    GraphicsInner* inner_;
};

} // namespace linden::gp
