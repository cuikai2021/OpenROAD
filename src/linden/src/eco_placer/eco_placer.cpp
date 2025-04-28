#include "linden/eco_placer.hpp"

#include <optional>
#include <utility>

#include "../util/lefdef_dumper.hpp"
#include "fassert.hpp"
#include "linden/dp/lib.rs.h"
#include "ord/OpenRoad.hh"

namespace linden {
namespace {

rust::Vec<rust::String> path_to_rust_vec(
    const std::vector<std::filesystem::path>& input)
{
    rust::Vec<rust::String> output;
    for (const auto& item : input) {
        output.push_back((std::string) item);
    }
    return output;
}

rust::Vec<rust::String> string_to_rust_vec(
    const std::vector<std::string>& input)
{
    rust::Vec<rust::String> output;
    for (const auto& item : input) {
        output.push_back((std::string) item);
    }
    return output;
}

PointPlace convert_to_point_place(const ::linden::ffi::PointPlace& p)
{
    return PointPlace{odb::Point(p.x, p.y), PointPlace::int_to_orient(p.orient)};
}

::linden::ffi::PointPlace convert_to_ffi_point_place(
    const PointPlace& pointPlace)
{
    return ::linden::ffi::PointPlace{
        pointPlace.point.x(),
        pointPlace.point.y(),
        PointPlace::orient_to_int(pointPlace.orient)};
}

::linden::ffi::OptionI64 convert_to_option_i64(const std::optional<int>& opt)
{
    ::linden::ffi::OptionI64 result;
    if (opt.has_value()) {
        result.is_none = false;
        result.some = static_cast<::std::int64_t>(opt.value());
    } else {
        result.is_none = true;
    }
    return result;
}

::linden::ffi::PlaceRange convert_to_ffi_place_range(const PlaceRange& range)
{
    return ::linden::ffi::PlaceRange{
        convert_to_option_i64(range.x_min),
        convert_to_option_i64(range.y_min),
        convert_to_option_i64(range.x_max),
        convert_to_option_i64(range.y_max)};
}

std::optional<PointPlace> convert_to_option_place(const ::linden::ffi::OptionPlace place)
{
    if (place.is_none) {
        return std::nullopt;
    }
    return convert_to_point_place(place.some);
}

} // namespace

void PointPlace::prettyPrint(std::string& out) const noexcept
{
    out.append(std::to_string(point.x()));
    out.append(std::string_view(" "));
    out.append(std::to_string(point.y()));
    out.append(std::string_view(" "));
    out.append(orient_to_string(orient));
}

std::string PointPlace::orient_to_string(const odb::dbOrientType& orient)
{
    switch (orient) {
        case odb::dbOrientType::R0:
            return "R0";
        case odb::dbOrientType::R180:
            return "R180";
        case odb::dbOrientType::R90:
            return "R90";
        case odb::dbOrientType::R270:
            return "R270";
        case odb::dbOrientType::MY:
            return "MY";
        case odb::dbOrientType::MX:
            return "MX";
        case odb::dbOrientType::MXR90:
            return "MXR90";
        case odb::dbOrientType::MYR90:
            return "MYR90";
        default:
            FASSERT(false).what("Not Support this odb::dbOrientType");
    }
    return "";
}

odb::dbOrientType PointPlace::int_to_orient(std::int64_t orient_int)
{
    switch (orient_int) {
        case 0:
            return odb::dbOrientType::R0;
        case 1:
            return odb::dbOrientType::R180;
        case 2:
            return odb::dbOrientType::R90;
        case 3:
            return odb::dbOrientType::R270;
        case 4:
            return odb::dbOrientType::MY;
        case 5:
            return odb::dbOrientType::MX;
        case 6:
            return odb::dbOrientType::MXR90;
        case 7:
            return odb::dbOrientType::MYR90;
        default:
            FASSERT(false)
            (orient_int).what("wrong value");
    }
    return odb::dbOrientType::R0;
}

std::int64_t PointPlace::orient_to_int(const odb::dbOrientType& orient)
{
    switch (orient) {
        case odb::dbOrientType::R0:
            return 0;
        case odb::dbOrientType::R180:
            return 1;
        case odb::dbOrientType::R90:
            return 2;
        case odb::dbOrientType::R270:
            return 3;
        case odb::dbOrientType::MY:
            return 4;
        case odb::dbOrientType::MX:
            return 5;
        case odb::dbOrientType::MXR90:
            return 6;
        case odb::dbOrientType::MYR90:
            return 7;
        default:
            FASSERT(false).what("Not Support this odb::dbOrientType");
    }
    return 0;
}

EcoPlacer::EcoPlacer(
    const std::vector<std::filesystem::path>& lefs,
    const std::vector<std::filesystem::path>& defs) noexcept
: _gs(nullptr)
{
    _gs = ::linden::ffi::init_grid_site(path_to_rust_vec(lefs), path_to_rust_vec(defs));
}

EcoPlacer::EcoPlacer(
    const std::vector<std::string>& lefs,
    const std::vector<std::string>& defs) noexcept
: _gs(nullptr)
{
    _gs = ::linden::ffi::init_grid_site(string_to_rust_vec(lefs), string_to_rust_vec(defs));
}

EcoPlacer::EcoPlacer() noexcept
: _gs(nullptr)
{
    ord::OpenRoad& ord = *ord::OpenRoad::openRoad();
    const LefDefDumper dumper(ord);
    std::vector<std::filesystem::path> lefs{dumper.tech_lef};
    std::vector<std::filesystem::path> defs{dumper.def};
    for (const auto& lef : dumper.lib_lefs) {
        lefs.push_back(lef);
    }
    _gs = ::linden::ffi::init_grid_site(path_to_rust_vec(lefs), path_to_rust_vec(defs));
}

EcoPlacer::~EcoPlacer() noexcept
{
    ::linden::ffi::free_grid_site(_gs);
}

void EcoPlacer::find_vacant_places(
    std::vector<PointPlace>& places,
    const std::string_view& instance,
    const odb::Point& p0,
    const odb::Point& p1,
    int x_delta,
    int y_delta) const
{
    auto vacant_places = _gs->find_vacant_places(
        ::rust::Str{instance.data(), instance.size()},
        ::linden::ffi::Rect{p0.x(), p0.y(), p1.x(), p1.y()},
        (int64_t) x_delta,
        (int64_t) y_delta);

    for (const auto& vacant_place : vacant_places) {
        places.emplace_back(convert_to_point_place(vacant_place));
    }
}

std::optional<CellPlaced> EcoPlacer::get_cell(const std::string_view& master) const 
{
    auto res = _gs->get_cell(::rust::Str{master.data(), master.size()});
    if (res.is_none) {
        return std::nullopt;
    }
    return CellPlaced {
        convert_to_point_place(res.some.pp),
        std::string(res.some.model),
    };
}

void EcoPlacer::insert_cell(
    const std::string_view& master,
    const std::string_view& instance,
    const PointPlace& place)
{
    _gs->insert_cell(
        ::rust::Str{master.data(), master.size()},
        ::rust::Str{instance.data(), instance.size()},
        convert_to_ffi_point_place(place));
}

void EcoPlacer::remove_cell(const std::string_view& master)
{
    _gs->remove_cell(::rust::Str{master.data(), master.size()});
}

PlaceIterator::PlaceIterator(
    const EcoPlacer& ep,
    const std::string_view& instance,
    const odb::Point& point,
    const PlaceRange& range)
: _pi(nullptr)
{
    _pi = ::linden::ffi::init_place_iterator(
        ep._gs,
        ::rust::Str{instance.data(), instance.size()},
        ::linden::ffi::Point{point.x(), point.y()},
        convert_to_ffi_place_range(range));
}
PlaceIterator::~PlaceIterator() noexcept
{
    ::linden::ffi::free_place_iterator(_pi);
}

std::optional<PointPlace> PlaceIterator::next()
{
    auto place = _pi->next();
    return convert_to_option_place(place);
}

} // namespace linden
