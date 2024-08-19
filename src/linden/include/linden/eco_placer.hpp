#pragma once

#include <filesystem>
#include <string>
#include <vector>

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

    PointPlace() = default;

    explicit PointPlace(const odb::Point& p, const odb::dbOrientType& orient)
    : point(p), orient(orient) {}

    void prettyPrint(std::string& out) const noexcept;

    static odb::dbOrientType int_to_orient(std::int64_t orient_int);
    static std::int64_t orient_to_int(const odb::dbOrientType& orient);
    static std::string orient_to_string(const odb::dbOrientType& orient);
};

class EcoPlacer
{
private:
    ::linden::ffi::GridSite* _gs;

public:
    /**
     * Constructor.
     * Initializes the EcoPlacer object and loads LEF and DEF files.
     * @param lefs Paths to LEF files
     * @param defs Paths to DEF files
     */
    EcoPlacer(const std::vector<std::filesystem::path>& lefs, const std::vector<std::filesystem::path>& defs) noexcept;
    EcoPlacer(const std::vector<std::string>& lefs, const std::vector<std::string>& defs) noexcept;
    ~EcoPlacer() noexcept;

    /**
     * Find vacant places.
     * Find the locations where a Cell can be legally placed in a given area, and sort the results in the order of distance to the reference line segment.
     * @param places Returns all found available locations.
     * @param instance Instance name of the unit to be placed.
     * @param p0, p1 Two endpoints of the reference line segment. It must be either horizontal or vertical, or in a degenerated case, a point.
     * @param x_delta, y_delta Halos around the reference line segment. Both a reference line segement and its halos defines the searching window.
     */
    void find_vacant_places(
        std::vector<PointPlace>& places,
        const std::string_view& instance,
        const odb::Point& p0,
        const odb::Point& p1,
        int x_delta,
        int y_delta) const;

    /**
     * Insert a cell.
     * Inserts a unit at a specified location.
     * @param master Name of the cell to be inserted.
     * @param instance Instance name of the cell to be inserted.
     * @param place Location for insertion.
     */
    void insert_cell(
        const std::string_view& master,
        const std::string_view& instance,
        const PointPlace& place);

    /**
     * Remove a cell.
     * Removes a specified cell from the design.
     * @param master Name of the cell to be removed.
     */
    void remove_cell(const std::string_view& master);
};

}; // end namespace linden
