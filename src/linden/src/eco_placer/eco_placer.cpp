#include "linden/eco_placer.hpp"

#include "linden/dp/lib.rs.h"

namespace linden {
namespace {
PointPlace convert_to_point_place(const ::linden::ffi::PointPlace& p)
{
    return PointPlace{odb::Point(p.x, p.y), PointPlace::int_to_dbOrientType(p.orient)};
}

::linden::ffi::PointPlace convert_to_ffi_point_place(
    const PointPlace& pointPlace)
{
    return ::linden::ffi::PointPlace{
        pointPlace.point.x(),
        pointPlace.point.y(),
        PointPlace::dbOrientType_to_int(pointPlace.orient)};
}

rust::Vec<rust::String> convert_to_rust_vec(
    const std::vector<std::filesystem::path>& input)
{
    rust::Vec<rust::String> output;
    for (const auto& item : input) {
        output.push_back((std::string) item);
    }
    return output;
}

} // namespace

EcoPlacer::EcoPlacer(
    const std::vector<std::filesystem::path>& lefs,
    const std::vector<std::filesystem::path>& defs) noexcept
: _gs(nullptr)
{
    _gs = ::linden::ffi::init_grid_site(convert_to_rust_vec(lefs), convert_to_rust_vec(defs));
}

EcoPlacer::~EcoPlacer() noexcept
{
    ::linden::ffi::free_grid_site(_gs);
}

void EcoPlacer::find_vacant_places(
    std::vector<PointPlace>& places,
    const std::string_view& cell,
    const odb::Point& p0,
    const odb::Point& p1,
    int x_delta,
    int y_delta) const
{
    auto vacant_places = _gs->find_vacant_places(
        ::rust::Str{cell.data(), cell.size()},
        ::linden::ffi::Rect{p0.x(), p0.y(), p1.x(), p1.y()},
        (int64_t) x_delta,
        (int64_t) y_delta);

    for (const auto& vacant_place : vacant_places) {
        places.emplace_back(convert_to_point_place(vacant_place));
    }
}

void EcoPlacer::insert_cell(
    const std::string_view& cell_name,
    const std::string_view& cell,
    const PointPlace& place)
{
    _gs->insert_cell(
        ::rust::Str{cell_name.data(), cell_name.size()},
        ::rust::Str{cell.data(), cell.size()},
        convert_to_ffi_point_place(place));
}

void EcoPlacer::remove_cell(const std::string_view& cell_name)
{
    _gs->remove_cell(::rust::Str{cell_name.data(), cell_name.size()});
}

} // namespace linden
