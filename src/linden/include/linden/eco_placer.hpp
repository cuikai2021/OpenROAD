#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "fassert.hpp"
#include "odb/db.h"
#include "odb/dbTypes.h"
#include "odb/geom.h"

namespace linden::ffi {
class GridSite;
}

namespace linden {

struct PointPlace
{
    odb::Point point;
    odb::dbOrientType orient;

    explicit PointPlace(const odb::Point& p, const odb::dbOrientType& orient)
    : point(p), orient(orient) {}

    static odb::dbOrientType int_to_dbOrientType(std::int64_t intValue)
    {
        switch (intValue) {
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
                (intValue).what("wrong value");
        }
        return odb::dbOrientType::R0;
    }

    static std::int64_t dbOrientType_to_int(const odb::dbOrientType& orientValue)
    {
        switch (orientValue) {
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
};

class EcoPlacer
{
private:
    ::linden::ffi::GridSite* _gs;

public:
    EcoPlacer(const std::vector<std::filesystem::path>& lefs, const std::vector<std::filesystem::path>& defs) noexcept;
    ~EcoPlacer() noexcept;

    void find_vacant_places(
        std::vector<PointPlace>& places,
        const std::string_view& cell,
        const odb::Point& p0,
        const odb::Point& p1,
        int x_delta,
        int y_delta) const;

    void insert_cell(
        const std::string_view& cell_name,
        const std::string_view& cell,
        const PointPlace& place);
    void remove_cell(const std::string_view& cell_name);
};

} // end namespace linden
