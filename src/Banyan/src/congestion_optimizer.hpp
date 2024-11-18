#pragma once

#include <vector>
#include <map>
#include <optional>
#include <ostream>
#include <filesystem>
#include <string_view>
namespace banyan {
    class Banyan;

    namespace congestion_optimizer {
        extern const std::filesystem::path TEMP_FOLDER;
        class Percentiles {
            private:
            std::map<double, double> horizontal;
            std::map<double, double> vertical;

            public:
            static Percentiles init(std::filesystem::path& p);
            std::pair<std::optional<double>, std::optional<double>> top_k(double k);
            double wace4();
        };

        class DPConfig {
            public:
            std::vector<std::filesystem::path> lefs;
            std::filesystem::path def;
            std::filesystem::path congestion_data;
            std::optional<std::filesystem::path> inflation_data;
            std::filesystem::path out_def;
            std::filesystem::path out_inflation;
            void to_next_iteration();
        };

        class Config {
            public:
            explicit Config(DPConfig inner_cfg, std::string_view max_layer);

            DPConfig inner_cfg;
            std::string_view max_layer;
            std::chrono::minutes max_runtime;
            unsigned int max_iteration;
            void to_next_iteration();
            void dump_dp_cfg(std::filesystem::path& output_path);
            std::vector<std::string> rdp_command(std::filesystem::path rdp_cfg);
            friend std::ostream& operator<< (std::ostream &os, Config &cfg);
        };

        class Optimizer {
            private:
                banyan::Banyan* b;
                std::vector<Percentiles> congestion_reports;
                void load_report(std::filesystem::path&);
                Config cfg;
            public:
                Optimizer(Config cfg, banyan::Banyan* banyan_handler);
                unsigned int choose_best();
                void write_back_best();
                void optimize();
        };
    }
}