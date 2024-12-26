#include "congestion_optimizer.hpp"
#include <spdlog/spdlog.h>
#include "banyan/Banyan.h"
#include "pblefdef.pb.h"
#include "ord/OpenRoad.hh"
#include "lrl/CircuitModifier/CircuitModifier.hh"
#include "lrl/Laurel.hh"
#include "toml++/toml.h"
#include <boost/process.hpp>
#include <boost/process/system.hpp>
#include <filesystem>
#include <sstream>
#include <iostream>
#include <string>

IMPORT_STR_FLAG_WITH_DEFAULT(DEBLAIS_ROOT_DIR);

namespace fs = std::filesystem;
void just_copy_file(const fs::path& src_path, const fs::path& dst_path) {
    std::ifstream src_file(src_path, std::ios::binary);
    std::ofstream dst_file(dst_path, std::ios::binary);
    dst_file << src_file.rdbuf();
}

void copy_files_recursive(const fs::path& src_dir, const fs::path& dst_dir) {
    for (const auto& entry : fs::recursive_directory_iterator(src_dir)) {
        const auto& path = entry.path();
        if (fs::is_regular_file(path)) {
            fs::path target_path = dst_dir / path.lexically_relative(src_dir);
            fs::create_directories(target_path.parent_path());
            just_copy_file(path, target_path);
        }
}
}

namespace banyan {
    namespace congestion_optimizer {
        const std::filesystem::path DEBLAIS_ROUTABILITY_DRIVEN_DP_PATH = "/build/bin/deblais_rddp";
        const int DEFAULT_MAX_ITERATION = 15;
        const int DEFAULT_MAX_RUNTIME = 60; // In minutes
        const char* ENV_MAX_ITERATION = "BANYAN_CONGESTIONOPT_MAX_ITERATION";
        const char* ENV_MAX_RUNTIME = "BANYAN_CONGESTIONOPT_MAX_RUNTIME";
        const double WACE4_POS[] = {0.005, 0.01, 0.02, 0.05};

        // util for handling path like `/blah/blah/blah/iter_2.lef` or `/home/you/data/iter_3.def`,
        // whoose format would always be `iter_{$n_iter}.{$extension}`.
        // Dirty but effective, that's only what we need.
        int get_iter_from_filename(std::filesystem::path const& p) {
            return std::stoi((const char*)p.filename().replace_extension("").c_str() + 5);
        }

        odb::dbOrientType::Value pb_to_dbOrientType(pblefdef::Orient pb) noexcept
        {
            switch (pb) {
                case pblefdef::Orient::NORTH: return odb::dbOrientType::R0;
                case pblefdef::Orient::SOUTH: return odb::dbOrientType::R180;
                case pblefdef::Orient::EAST: return odb::dbOrientType::R270;
                case pblefdef::Orient::WEST: return odb::dbOrientType::R90;
                case pblefdef::Orient::FLIPPED_NORTH: return odb::dbOrientType::MY;
                case pblefdef::Orient::FLIPPED_SOUTH: return odb::dbOrientType::MX;
                case pblefdef::Orient::FLIPPED_EAST: return odb::dbOrientType::MYR90;
                case pblefdef::Orient::FLIPPED_WEST: return odb::dbOrientType::MXR90;
                default: {
                    FASSERT(false).what("unreachable!");
                    return odb::dbOrientType::R0;
                }
            }
        }

        void read_back(const std::filesystem::path& outdef) {
            pblefdef::DesignExchangeFormat def;
            {
                std::ifstream fin(outdef);
                if (!fin) {
                    spdlog::error("Cannot open result DEF from Linden detailed placer: {}", outdef);
                    throw std::runtime_error("Cannot open result DEF from Linden detailed placer.");
                }
                bool ret = def.ParseFromIstream(&fin);
                if (!ret) {
                    spdlog::error("Cannot parse result DEF from Linden detailed placer: {}", outdef);
                    throw std::runtime_error("Cannot parse result DEF from Linden detailed placer.");
                }
            }
            ord::OpenRoad& ord = *ord::OpenRoad::openRoad();
            lrl::NetworkModifier& cm = lrl::Laurel::getPlacementNetworkModifier();
            lrl::STANetwork sta_nw = cm.network;
            odb::dbBlock& top = *(ord.getDb()->getChip()->getBlock());
            int dbu = top.getDefUnits();
            int64_t pm_per_dbu = 1'000'000 / dbu;
            for(size_t i = 0, sz = def.components_size(); i < sz; ++i) {
                const pblefdef::DesignExchangeFormat_Component& comp = def.components(i);
                const std::string& cell_name = comp.name();
                FASSERT(comp.has_place());
                const pblefdef::DesignExchangeFormat_Place& place = comp.place();
                FASSERT(place.has_position());
                const pblefdef::Point& target_pos = place.position();
                odb::Point target_pt(target_pos.x() / pm_per_dbu, target_pos.y() / pm_per_dbu); // pm to dbu
                odb::dbOrientType target_orient(pb_to_dbOrientType(place.orient()));
                sta::Instance* sta_inst = sta_nw.findInstance(cell_name.c_str());
                FASSERT(sta_inst != nullptr)
                    .what("Linden DP so far does not add/remove cells. So, everything must be there in ODB & STA.");
                cm.moveInst(sta_inst, target_pt, target_orient.getValue());
            }
            rsz::Resizer& resizer = *ord.getResizer();
            resizer.estimateParasitics(rsz::ParasiticsSrc::placement);
        }

        Percentiles Percentiles::init(std::filesystem::path& p){
            Percentiles res;
            auto tbl = toml::parse_file(p.c_str());
            auto cgs = tbl["congestion_report"].as_array();
            cgs->for_each([&](toml::table elem) {
                std::string direction = elem["direction"].value_or<std::string>("");
                double pos = elem["top_k"].value_or<double>(0.0);
                double congestion = elem["congestion"].value_or<double>(0.0);
                pos = double(int(round(pos * 1000.0))) / 1000.0;
                if (direction == "Horizontal") {
                    res.horizontal[pos] = congestion;
                } else if (direction == "Vertical") {
                    res.vertical[pos] = congestion;
                }
            });
            return res;
        }

        std::pair<std::optional<double>, std::optional<double>> Percentiles::top_k(double k) {
            std::optional<double> horizontal, vertical;
            auto iter = this->horizontal.find(k);
            if (iter != this->horizontal.end()) {
                horizontal = iter->second;
            }
            iter = this->vertical.find(k);
            if (iter != this->vertical.end()) {
                vertical = iter->second;
            }
            return std::make_pair(horizontal, vertical);
        }

        double Percentiles::wace4() {
            int len = sizeof WACE4_POS / sizeof WACE4_POS[0];
            double res = 0.0;
            for (int i = 0; i < len; i++) {
                auto[horizontal, vertical] = top_k(WACE4_POS[i]);
                if ((!(horizontal.has_value())) || (!(vertical.has_value()))) {
                    spdlog::error("CongestionOptimizer Cannot find data at percentile? {}", WACE4_POS[i]);
                    throw std::runtime_error("Failed to stat wace4 at percentile: " + std::to_string(WACE4_POS[i]));
                }
                res += horizontal.value();
                res += vertical.value();
            }
            res /= double(len) * 2;
            return res;
        }

        std::filesystem::path get_routability_driven_dp_exe() {
            return DEBLAIS_ROUTABILITY_DRIVEN_DP_PATH;
        }

        Optimizer::Optimizer(Config cfg, banyan::Banyan* banyan_handler): cfg(cfg) {
            this-> b = banyan_handler;
            this-> congestion_reports = std::vector<Percentiles>();
        }

        void Optimizer::optimize() {
            spdlog::info("CongestionOptimizer Config\n  Max_Layer: {}\n  Max_Runtime(minutes): {}\n  Max_Iteration: {}",cfg.max_layer, cfg.max_runtime.count(), cfg.max_iteration);
            auto st = std::chrono::system_clock::now();
            auto time_limit = this -> cfg.max_runtime;
            auto rddp_path = get_routability_driven_dp_exe();
            auto root_dir = this->cfg.root_dir;
            std::filesystem::path dp_cfg_path = root_dir / "cfg.toml";
            std::filesystem::path db_path = root_dir / "out.db";
            for (int i= 0; i < cfg.max_iteration; i++) {
                this -> b -> clearWires(false, false);
                // this -> b -> route_clock(cfg.max_layer, std::string_view(db_path.c_str())); 
                this -> b -> repair_congestion(cfg.inner_cfg.congestion_data.c_str(), db_path.c_str(), cfg.max_layer, true);
                cfg.dump_dp_cfg(dp_cfg_path);
                auto commands = cfg.rdp_command(dp_cfg_path);
                boost::process::child c(std::string(rddp_path), boost::process::args(cfg.rdp_command(dp_cfg_path)));
                while (c.running()) {
                    auto now = std::chrono::system_clock::now();
                    auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(now - st);
                    if (elapsed > time_limit) {
                        spdlog::info("CongestionOptimizer: Timeout");
                        c.terminate();
                        break;
                    }

                    using namespace std::chrono_literals;
                    std::this_thread::sleep_for(1000ms);
                }
                auto now = std::chrono::system_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(now - st);
                if (elapsed > time_limit) {
                    spdlog::info("CongestionOptimizer Timeout at Iter: {}", i);
                    break;
                }
                this->load_report(this->cfg.inner_cfg.out_inflation);
                spdlog::info("report len: {}", this->congestion_reports.size());
                spdlog::info(
                    "CongestionOptimizer\  
                    \n  Progress Iter {} / {}, \ 
                    \n  previous wACE4 at iter before: {}\ 
                    \n  Previous MAX Honrizontal-Congestion: {}\
                    \n  Previous MAX Vertical-Congestion: {}",
                    i, cfg.max_iteration, congestion_reports.back().wace4(),congestion_reports.back().top_k(0).first.value_or(-1.0), congestion_reports.back().top_k(0).second.value_or(-1.0)
                );
                read_back(cfg.inner_cfg.out_def);

#ifndef NDEBUG
                {  // Export a fancy `.geogit` congestionmap
                    std::filesystem::path pb_path = this->cfg.root_dir / std::filesystem::path("iter_" + std::to_string(i) + ".pb");
                    std::filesystem::path geogit_path = this->cfg.root_dir / std::filesystem::path("iter_" + std::to_string(i) + "_cmap.geogit");
                    std::string cmap_visualizer = "/build/bin/cmap_visualizer";
                    std::vector<std::string> args;
                    args.emplace_back("--input-file");
                    args.emplace_back(pb_path.c_str());
                    args.emplace_back("--output-file");
                    args.emplace_back(geogit_path.c_str());
                    auto boost_args = boost::process::args(args);
                    auto ec = boost::process::system(cmap_visualizer, boost_args);
                    spdlog::info("cmap_visualizing enabled and finished return code: {}", ec);
                }
#endif
                cfg.to_next_iteration();
            }
        }
        
        unsigned int Optimizer::choose_best() {
            double min_max = std::numeric_limits<double>::max();
            int min_id = -1;
            for (int id = 0; id < this->congestion_reports.size(); id++) {
                std::optional<double> horizontal, vertical;
                std::tie(horizontal, vertical) = this->congestion_reports[id].top_k(0.0);
                if (!horizontal.has_value() || !vertical.has_value()) {
                    spdlog::info("Failed to find maximum congestion");
                    std::runtime_error("Failed to find maximum congestion");
                }
                if (min_max > horizontal.value() + vertical.value()) {
                    min_max = horizontal.value() + vertical.value();
                    min_id = id;
                }
            }
            if (min_id == -1) {
                spdlog::error("Congestion Optimizer unable to find a best solution");
                throw std::runtime_error("Unable to find a best solution");
            }
            {
                std::optional<double> original_horizontal, original_vertical;
                std::optional<double> best_horizontal, best_vertical;
                std::tie(original_horizontal, original_vertical) = this->congestion_reports[0].top_k(0);
                std::tie(best_horizontal, best_vertical) = this->congestion_reports[min_id].top_k(0);
                spdlog::info(
                    "Congestion Optimizer\
                    \n  Choose Iter {} as best.\
                    \n  Original Max: Horizontal {} Vertical {}.\
                    \n  Best Max: Horizontal {} Vertical {}.",
                    min_id, original_horizontal.value(), original_vertical.value(),
                    best_horizontal.value(), best_vertical.value()
                );
            }
            return min_id;
        }

        void Optimizer::load_report(std::filesystem::path& report_path) {
            auto percentiles = Percentiles::init(report_path);
            this->congestion_reports.push_back(percentiles);
        }

        void Optimizer::write_back_best() {
            unsigned int best_id = this->choose_best();
            {
                double best_wace4 = this->congestion_reports[best_id].wace4();
                double init_wace4 = this->congestion_reports[0].wace4();
                spdlog::info("Congestion Optimizer Finished: Choose Iter No.{} to write back. wACE4 {} -> {}", best_id, init_wace4, best_wace4);
            }
            if (best_id == 0) {
                return;
            }
            std::filesystem::path path =  this->cfg.root_dir / ("iter_" + std::to_string(best_id) + ".pbdef");
            read_back(path);
        }


        void DPConfig::to_next_iteration(const std::filesystem::path& root_dir) {
            int current_iter = get_iter_from_filename(this->out_def);
            int next_iter = current_iter + 1;
            this->congestion_data = root_dir / ("iter_" + std::to_string(current_iter) + ".pb");
            this->inflation_data = std::optional<std::filesystem::path>(root_dir / ("iter_" + std::to_string(current_iter) + ".inflation"));
            this->out_def = root_dir / ("iter_" + std::to_string(next_iter) + ".pbdef");
            this->out_inflation = root_dir / ("iter_" + std::to_string(next_iter) + ".inflation");
        }

        Config::Config(DPConfig inner_cfg, std::string_view max_layer, std::filesystem::path root_dir){
            int max_iteration = DEFAULT_MAX_ITERATION;
            char const* max_iteration_env = std::getenv(ENV_MAX_ITERATION);
            if (max_iteration_env != NULL) {
                max_iteration = std::stoi(max_iteration_env);
            }

            int max_runtime = DEFAULT_MAX_RUNTIME;
            char const* max_runtime_env = std::getenv(ENV_MAX_RUNTIME);
            if (max_runtime_env != NULL) {
                max_runtime = std::stoi(max_runtime_env);
            }
            this -> root_dir = std::move(root_dir);
            this -> inner_cfg = std::move(inner_cfg);
            this -> max_layer = max_layer;
            this -> max_runtime = std::chrono::minutes(max_runtime);
            this -> max_iteration = max_iteration;
        }

        std::vector<std::string> Config::rdp_command(std::filesystem::path rdp_cfg) {
            std::vector<std::string> res = std::vector<std::string>();
            res.clear();
            res.push_back("--input-config");
            res.push_back(std::string(rdp_cfg.c_str()));
            res.push_back("--to");
            res.push_back(std::string(this->inner_cfg.out_def.c_str()));
            res.push_back("--cell-only");
            return res;
        }

        void Config::to_next_iteration() {
            this->inner_cfg.to_next_iteration(this->root_dir);
        }

        std::ostream& operator<< (std::ostream &out, Config &cfg) {
            out << "congestion_optimizer.Config"
                "\n  Max_layer: " << cfg.max_layer <<
                "\n  Max_iteration: " << cfg.max_iteration <<
                "\n  Max_Time(Minutes): " << cfg.max_runtime.count();
            return out;
        }

        void Config::dump_dp_cfg(std::filesystem::path& output_path) {
            auto root = toml::table { {"version", "1.2"} };
            auto& lefs = this->inner_cfg.lefs;
            auto& def = this->inner_cfg.def;
            auto& congestion_data = this->inner_cfg.congestion_data;
            auto& inflation_data = this->inner_cfg.inflation_data;
            {
                toml::array lefs_;
                for (auto const& lef: lefs) {
                    lefs_.push_back(lef.string());
                }
                root.emplace("lefs", lefs_);
            }
            {
                toml::array defs_;
                defs_.push_back(def.string());
                root.emplace("defs", defs_);
            }
            root.emplace("routability-data", congestion_data.c_str());
            if (inflation_data.has_value()) {
                root.emplace("inflation-data", inflation_data.value().c_str());
            }
            {
                std::ofstream fout(output_path);
                fout << root;
                fout.close();
            }
        };
    };
}