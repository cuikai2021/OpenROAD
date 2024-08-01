#include "linden/eco_placer.hpp"

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "odb/geom.h"

int main()
{
    const std::filesystem::path BASE_PATH = "src/linden/test/smic110";
    std::vector<std::filesystem::path> lefs = {
        BASE_PATH / "scc011u_8lm_1tm_thin_ALPA.lef",
        BASE_PATH / "scc011ums_hd_rvt_ant.lef"};
    std::vector<std::filesystem::path> defs = {BASE_PATH / "route.def"};

    linden::EcoPlacer eco_placer(lefs, defs);

    int ok = 0, fail = 0, test_count = 0;
    auto find_test = [&](const std::string_view& cell, const odb::Point& origin, const odb::Point& delta, int expected) {
        std::cout << "Test " << ++test_count << ": ";

        std::vector<linden::PointPlace> vacant_places;
        eco_placer.find_vacant_places(vacant_places, cell, origin, origin, delta.x(), delta.y());
        if (vacant_places.size() == expected) {
            ok++;
            std::cout << "Ok" << '\n';
        } else {
            fail++;
            std::cout << "Fail: " << '\n';
            std::cout << "expected: " << expected << "\n";
            std::cout << "vacant_places size = " << vacant_places.size() << "\n";
        }
    };

    std::string_view buf_hdv0 = "BUFHDV0";
    std::cout << "\nFind/Insert Test:\n";
    find_test(buf_hdv0, {235900, 402955}, {5200, 6000}, 6);
    find_test(buf_hdv0, {222195, 199973}, {5000, 6000}, 0);
    find_test(buf_hdv0, {292330, 283310}, {5330, 6560}, 8);
    eco_placer.insert_cell("cell_0", buf_hdv0, linden::PointPlace{odb::Point{288230, 286590}, odb::dbOrientType::R0});
    eco_placer.insert_cell("cell_1", buf_hdv0, linden::PointPlace{odb::Point{294790, 276750}, odb::dbOrientType::MX});
    find_test(buf_hdv0, {292330, 283310}, {5330, 6560}, 2);

    find_test(buf_hdv0, {262810, 60270}, {5330, 6561}, 41);
    eco_placer.insert_cell("cell_2", buf_hdv0, linden::PointPlace{odb::Point{261580, 56990}, odb::dbOrientType::R0});
    find_test(buf_hdv0, {262810, 60270}, {5330, 6561}, 34);

    eco_placer.remove_cell("cell_2");
    find_test(buf_hdv0, {262810, 60270}, {5330, 6561}, 41);

    std::cout << "Ok: " << ok << " Fail: " << fail << '\n';

    return 0;
}