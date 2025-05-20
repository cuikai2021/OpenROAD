#include "checker.hpp"
#include <spdlog/spdlog.h>
#include "envflag/envflag.hpp"
#include "ord/OpenRoad.hh"
#include "util/lefdef_dumper.hpp"
#include <boost/process.hpp>
#include <boost/process/environment.hpp>
#include <filesystem>
#include <deque>
#include <iostream>
#include <stdexcept>
#include <string>
IMPORT_STR_FLAG_WITH_DEFAULT(LINDENDP_EXE_DIR);

namespace linden {
    namespace checker {
        std::filesystem::path get_linden_placement_checker_exe() {
            std::filesystem::path dir(LINDENDP_EXE_DIR());
            return dir / "linden-placement-checker";
        }

        int exec(
            const bool supress_output,
            const std::deque<std::string>& args
        ) {
            std::filesystem::path exe = get_linden_placement_checker_exe();
            if (!std::filesystem::exists(exe)) {
                spdlog::error("Cannot find {}", exe.string());
                throw std::runtime_error("fail to run linden-placement-checker");
            }
            using std::filesystem::perms;
            if (auto status = std::filesystem::status(exe);
                (status.permissions() & (perms::owner_exec | perms::group_exec | perms::others_exec)) == perms::none)
            {
                spdlog::error("Cannot execute {}", exe.string());
                throw std::runtime_error("fail to run linden-placement-checker");
            }
            int ec = 0;
            auto env = boost::this_process::environment();
            boost::process::environment env1 = env;
            if (!supress_output) {
                ec = boost::process::system(std::string(exe), boost::process::args(args), env1);
            } else {
                ec = boost::process::system(std::string(exe), boost::process::args(args),
                env1,
                boost::process::std_out > boost::process::null,
                boost::process::std_err > boost::process::null);
            }
            switch (ec)
            {
            case 0:
                // intend to do nothing
                return ec;
            case 1:
                spdlog::warn("Placement checker met violations.");
                return ec;
                break;
            default:
                break;
            }
            spdlog::warn("`linden-placement-checker` exit code: {}", ec);
            throw std::runtime_error("fail to run linden-placement-checker");
        }
        

        int check_placement(::ord::OpenRoad& ord, bool suppress_output, std::optional<std::string> mode) {
            const LefDefDumper dumper(ord);
            if (suppress_output) {
                dumper.print_dumped_files(std::cout);
            }
            std::deque<std::string> args;
            args.emplace_back("--lef");
            for(auto const& x: dumper.lib_lefs) {
                args.push_back(x);
            }
            args.emplace_back("--def");
            auto const& def = dumper.def;
            args.push_back(def);
            if (mode.has_value()) {
                args.emplace_back("--mode");
                args.push_back(*mode);
            }
            return exec(suppress_output, args);
        }
    } //namespace checker
} // namespace linden