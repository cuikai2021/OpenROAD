#include "ffi.hpp"

#include <filesystem>

#include "../dp/dp.hpp"
#include "../util/lefdef_dumper.hpp"
#include "add_tie_cell.hpp"
#include "linden/dp.hpp"
#include "ord/OpenRoad.hh"

namespace linden {

namespace {
void parse_flag(std::deque<std::string>& args, DpConfig& cfg)
{
    const std::string_view NOT_TO_STOP_WHEN_ILLEGAL("--not-to-stop-when-illegal");
    const std::string_view SUPPRESS_OUTPUT("--suppress-output");

    auto check_flag = [&args](const std::string_view& flag) -> bool {
        auto it = std::find(args.begin(), args.end(), flag);
        if (it != args.end()) {
            args.erase(it);
            return true;
        }
        return false;
    };
    cfg.suppress_output = check_flag(SUPPRESS_OUTPUT);
    cfg.not_stop_when_illegal = check_flag(NOT_TO_STOP_WHEN_ILLEGAL);
}

std::filesystem::path parse_ord(std::deque<std::string>& args, DpConfig& cfg)
{
    ord::OpenRoad& ord = *ord::OpenRoad::openRoad();
    const LefDefDumper dumper(ord);
    // 1. lef
    args.emplace_back("--lef");
    args.push_back(dumper.tech_lef);
    for (const auto& lef : dumper.lib_lefs) {
        args.push_back(lef);
    }

    // 2. def
    args.emplace_back("--def");
    args.push_back(dumper.def);

    // 3. to
    std::filesystem::path output = output_def_name(dumper.basename, cfg.suppress_output);
    args.emplace_back("--to");
    args.emplace_back(output);

    return output;
}

} // namespace

int add_tie_cell_gogogo(std::deque<std::string> args)
{
    DpConfig cfg;
    parse_flag(args, cfg);
    auto output = parse_ord(args, cfg);
    return add_tie_cell(cfg, args, output);
}

} // namespace linden
