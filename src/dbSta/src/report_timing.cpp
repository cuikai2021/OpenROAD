#include "db_sta/report_timing.hpp"
#include "db_sta/dbSta.hh"
#include "db_sta/dbNetwork.hh"
#include "sta/Corner.hh"
#include "sta/PathVertex.hh"
#include "sta/PathRef.hh"
#include "sta/PathExpanded.hh"
#include "sta/Graph.hh"
#include "sta/Transition.hh"
#include "utl/metrics.hpp"
// #include "fassert.hpp"
#include <string>
#include <memory>
#include <iostream>

using namespace std;

namespace dbsta {

namespace {

void pin_name(
    metrics::impl::Value& rpt,
    const sta::Network& network,
    const sta::Vertex& vert
) {
    auto pin = vert.pin();
    // FASSERT(pin != nullptr);
    const sta::Instance* inst = network.instance(pin);
    if (inst != nullptr && inst != network.topInstance()) {
        rpt["cell"] = network.pathName(inst);
        rpt["pin"] = network.portName(pin);
    } else {
        rpt["port"] = network.portName(pin);
    }
    if (vert.isBidirectDriver()) {
        rpt["bidirect_driver"] = true;
    }
}

} // namespace

void report_timing(string output_file, bool suppress_details, bool suppress_path) {
    sta::Sta& sta = *sta::Sta::sta();
    sta::Network& network = *sta.network();
    metrics::Config metrics_cfg = metrics::Config::sink_to_stderr();;
    if (!output_file.empty()) {
        metrics_cfg = metrics::Config::sink_to_file(output_file);
    }
    metrics::Reporter rpt(metrics_cfg);
    auto corner = sta.cmdCorner();
    // FASSERT(corner != nullptr);
    string corner_name(corner->name());
    auto concerned_minmax = sta::MinMax::max();
    auto& corner_rpt = rpt["corners"].append();
    corner_rpt["name"] = corner_name;
    {
        auto tns = sta.totalNegativeSlack(corner, concerned_minmax);
        corner_rpt["tns"] = tns * 1e9;
    }
    {
        sta::Slack slack;
        sta::Vertex* vertex = nullptr;
        sta.worstSlack(corner, concerned_minmax, slack, vertex);
        corner_rpt["worst_slack"] = slack * 1e9;
        if(!suppress_path && vertex != nullptr) {
            auto& critical_path_rpt = corner_rpt["critical_path"];
            auto const& path = sta.vertexWorstSlackPath(vertex, concerned_minmax);
            sta::PathExpanded ex_path(&path, &sta);
            for(size_t i = 0, sz = ex_path.size(); i < sz; ++i) {
                auto& vert_rpt = critical_path_rpt.append();
                auto& ref = *ex_path.path(i);
                auto& vert = *ref.vertex(&sta);
                pin_name(vert_rpt, network, vert);
            }
        }
    }
    if (!suppress_details) {
        sta::Graph* graph = sta.ensureGraph();
        // FASSERT(graph != nullptr);
        vector<sta::Vertex*> verts;
        for(size_t i = 0, sz = graph->vertexCount(); i < sz; ++i) {
            sta::Vertex* vert = graph->vertex(i);
            if (vert != nullptr) {
                verts.push_back(vert);
            }
        }
        if (!verts.empty()) {
            auto& details_rpt = corner_rpt["details"];
            for(auto& vert: verts) {
                auto& vert_rpt = details_rpt.append();
                pin_name(vert_rpt, network, *vert);
                auto arrival = sta.vertexArrival(vert, nullptr, concerned_minmax);
                vert_rpt["arrival"] = arrival * 1e9;
                auto required = sta.vertexRequired(vert, concerned_minmax);
                vert_rpt["required"] = required * 1e9;
                auto slack = sta.vertexSlack(vert, concerned_minmax);
                vert_rpt["slack"] = slack * 1e9;
                auto& slew_rpt = vert_rpt["slew"];
                for(auto& edge: {sta::RiseFall::rise(), sta::RiseFall::fall()}) {
                    auto slew = sta.vertexSlew(vert, edge, concerned_minmax);
                    slew_rpt[edge->name()] = slew * 1e9;
                }
            }
        }
    }
}

} // namespace dbsta
