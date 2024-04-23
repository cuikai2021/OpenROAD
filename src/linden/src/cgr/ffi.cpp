#include "ffi.hpp"
#include "linden/analyze_banyan_cmap_file.hpp"
#include <spdlog/spdlog.h>
#include <filesystem>

using namespace std;

namespace linden::cgr {

int analyze_file_gogogo(const char* cmap_file, const char* report_file) noexcept
{
    if (cmap_file == nullptr) {
        spdlog::error(
            "missing the cmap pb file, pls specify it with -cmap_pb_file");
        return 1;
    }
    filesystem::path cmap(cmap_file);
    if (!filesystem::exists(cmap)) {
        spdlog::error("The cmap file does not exist.");
        return 1;
    }

    filesystem::path report;
    if (report_file != nullptr) {
        report = report_file;
    }

    if (analyze_banyan_cmap_file(cmap, report)) {
        return 0;
    } else {
        return 1;
    }
}

} // namespace linden::cgr

