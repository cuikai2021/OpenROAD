#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "odb/db.h"
#include "odb/dbTypes.h"
#include "odb/geom.h"

namespace linden::ffi {
struct GridSite;
struct PlaceIterator;
} // namespace linden::ffi

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

struct PlaceRange
{
    std::optional<int> x_min;
    std::optional<int> y_min;
    std::optional<int> x_max;
    std::optional<int> y_max;

    PlaceRange() noexcept = default;
    PlaceRange(std::optional<int> x_min, std::optional<int> y_min, std::optional<int> x_max, std::optional<int> y_max)
    : x_min(x_min), y_min(y_min), x_max(x_max), y_max(y_max) {}
    PlaceRange(const odb::Rect& rect)
    : x_min(rect.xMin()), y_min(rect.yMin()), x_max(rect.xMax()), y_max(rect.yMax()) {}
};

struct CellPlaced
{
    PointPlace point_place;
    std::string instance;

    CellPlaced(PointPlace p, std::string inst) noexcept
    : point_place(std::move(p)), instance(std::move(inst))
    {
    }
};

class PlaceIterator;

class EcoPlacer
{
private:
    ::linden::ffi::GridSite* _gs;
    friend class PlaceIterator;

public:
    /**
     * Constructor.
     * Initializes the EcoPlacer object and loads LEF and DEF files.
     * @param lefs Paths to LEF files
     * @param defs Paths to DEF files
     */
    explicit EcoPlacer(const std::vector<std::filesystem::path>& lefs, const std::vector<std::filesystem::path>& defs) noexcept;
    explicit EcoPlacer(const std::vector<std::string>& lefs, const std::vector<std::string>& defs) noexcept;
    explicit EcoPlacer() noexcept;
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
     * Get Cell Placed Info
     *
     * @param master Name of the cell to get.
     * @return std::optional<CellPlaced> containing the cell placement details if found; std::nullopt otherwise.
     */
    std::optional<CellPlaced> get_cell(const std::string_view& master) const;

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

    // some interface
    std::optional<PointPlace> find_place(std::string_view instance, const odb::Rect& rect);
    std::optional<PointPlace> find_place(std::string_view instance, const odb::dbSet<odb::dbBox>& boxes);
};

class PlaceIterator
{
private:
    ::linden::ffi::PlaceIterator* _pi;

public:
    /**
     * Constructor.
     * Initializes a PlaceIterator object, which iterates over the potential placement locations
     * for a specified instance within a defined search area.
     *
     * @param ep Reference to the EcoPlacer object, which manages placement operations.
     * @param instance Instance name of the unit to be placed.
     * @param point The most ideal location for placing instance.
     * @param range Defines the search area, including optional minimum and maximum x and y boundaries.
     *              If no range is provided, the entire available area is considered.
     */
    PlaceIterator(
        const EcoPlacer& ep,
        const std::string_view& instance,
        const odb::Point& point,
        const PlaceRange& range = PlaceRange());
    ~PlaceIterator() noexcept;

    /**
     * Get the next available placement location.
     *
     * @return An optional PointPlace object representing the next legal placement location.
     *         If there are no more locations to iterate over, returns std::nullopt.
     */
    std::optional<PointPlace> next();
};

}; // end namespace linden
