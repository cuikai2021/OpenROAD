#include "netweighter.hpp"

#include <spdlog/spdlog.h>

#include <utility>

#include "../../../rsz/src/RepairDesign.hh"
#include "linden/db/db.hpp"
#include "lrl/CircuitModifier/STANetwork.hh"
#include "lrl/Laurel.hh"
#include "lrl/Timer/Timer.hh"
#include "ord/OpenRoad.hh"
#include "rsz/Resizer.hh"
#include "sta/Fuzzy.hh"
#include "sta/MinMax.hh"
#include "sta/Network.hh"
#include "sta/NetworkClass.hh"

namespace linden {
namespace {

// Netweighting
constexpr double CUR_WEIGHT_PERCENT = 1.0;
constexpr double AGGRESSIVE_RATIO = 1;

// Head
constexpr double HEAD_NETS_PERCENT = 0.1;
constexpr double HEAD_RANKING_RATIO = 2.0;
constexpr double HEAD_RANKING_OFFEST = 1.0;

// Tail
constexpr double TAIL_NETS_PERCENT = 0.35;
constexpr double TAIL_RANKING_RATIO = 0.40;
constexpr double TAIL_RANKING_OFFEST = 0.20;

struct DrvFixerRszImpl : public rsz::Resizer {
    void fix() 
    {
        rsz::Resizer::journalBegin();
        rsz::Resizer::estimateWireParasitics();
        int repaired_net_count = 0;
        int slew_violations = 0;
        int cap_violations = 0;
        int fanout_violations = 0;
        int length_violations = 0;
        repair_design_->repairDesign(
            max_wire_length_,
            0.0,
            0.0,
            0.0,
            false,
            repaired_net_count,
            slew_violations,
            cap_violations,
            fanout_violations,
            length_violations);
    }
    void revert() 
    {
        rsz::Resizer::journalRestore(
            resize_count_,
            inserted_buffer_count_,
            cloned_gate_count_,
            swap_pin_count_,
            removed_buffer_count_);
    }
    sta::Vector<sta::Vertex*> LDV() { return level_drvr_vertices_; }
    void ensureLDV() { ensureLevelDrvrVertices(); }
 };

struct DrvFixerBase {
  virtual void fix() = 0;
  virtual void revert() = 0;
  virtual ~DrvFixerBase() {}
  virtual sta::Vector<sta::Vertex*> LDV() = 0;
};

struct DrvFixerNone : public DrvFixerBase {
    DrvFixerNone(rsz::Resizer* rsz) : impl_(reinterpret_cast<DrvFixerRszImpl*>(rsz)) {}
    ~DrvFixerNone() {}
    void fix()    override {}
    void revert() override {}
    sta::Vector<sta::Vertex*> LDV() override { 
        impl_->ensureLDV();
        return impl_->LDV();
    }

  protected:
    DrvFixerRszImpl* impl_;
};

struct DrvFixerRsz : public DrvFixerBase {
    DrvFixerRsz(rsz::Resizer* rsz) : impl_(reinterpret_cast<DrvFixerRszImpl*>(rsz)) {}
    ~DrvFixerRsz() {}
    void fix()    override { impl_->fix(); }
    void revert() override { impl_->revert(); }
    sta::Vector<sta::Vertex*> LDV() override { return impl_->LDV(); }
  protected:
    DrvFixerRszImpl* impl_;
};

} // namespace

using Slack = float;

int update_weights(db::Database& ldb) noexcept
{
    lrl::GBAReporter& gba = lrl::Laurel::getGBAReporter();
    auto nw = gba.network();

    sta::Network* network = nw->network();
    auto rsz = ord::OpenRoad::openRoad()->getResizer();
    DrvFixerBase* drv_fixer;
    if (nw->getDbNetwork()->hasHierarchy()) {
        spdlog::debug("[gpl/Netw] Resizer hierarchy drv fixer not support yet.");
        drv_fixer = new DrvFixerNone(rsz);
    } else {
        drv_fixer = new DrvFixerRsz(rsz);
    }
    ///////////////////////       DRV       ///////////////////////
    drv_fixer->fix();

    ///////////////////////      Timing     ////////////////////////
    std::map<const sta::Net*, Slack> net_slack;
    std::vector<const sta::Net*> nets;
    for (int i = drv_fixer->LDV().size() - 1; i >= 0; i--) {
        sta::Vertex* drvr = drv_fixer->LDV()[i];
        sta::Pin* drvr_pin = drvr->pin();
        sta::Net* net = network->isTopLevelPort(drvr_pin) ? network->net(network->term(drvr_pin))
                                                          : network->net(drvr_pin);
        if (net && !drvr->isConstant() && !nw->getDbNetwork()->isSpecial(net)
            && !nw->isClock(drvr_pin)) {
            net_slack.emplace(net, nw->vertexSlack(drvr, sta::MinMax::max()));
            nets.emplace_back(net);
        }
    }

    spdlog::debug("[gpl/Netw] nets size : {}", nets.size());
    spdlog::debug("[gpl/Netw] net_slack size: {}", net_slack.size());
    std::sort(nets.begin(), nets.end(), [&net_slack](const sta::Net* n1, const sta::Net* n2) {
        return net_slack.at(n1) < net_slack.at(n2);
    });
    ///////////////////////      Revert     ///////////////////////
    drv_fixer->revert();
    delete drv_fixer;

    ///////////////////////      LindenDB    ////////////////////////
    // all
    const double nets_size = nets.size();
    spdlog::info("[gpl/Netw] nets_size : {}", nets_size);
    spdlog::info("[gpl/Netw] slack_worst: {:.9f} ns", net_slack.find(nets.front())->second * 1e9);

    // head
    const auto nets_head_iter = nets.begin() + static_cast<size_t>(nets_size * HEAD_NETS_PERCENT);
    const sta::Slack slack_head = net_slack.find(*nets_head_iter)->second;
    const size_t head_nets_size = static_cast<size_t>(nets_head_iter - nets.begin());
    spdlog::debug("[gpl/Netw] head_nets_size = {}", head_nets_size);
    spdlog::debug("[gpl/Netw] slack_head: {:.9f} ns", slack_head * 1e9);

    // tail
    const auto nets_tail_iter =
        nets.begin() + static_cast<size_t>(nets_size * (1.0 - TAIL_NETS_PERCENT));
    const sta::Slack slack_tail = net_slack.at(*nets_tail_iter);
    const size_t tail_nets_size = static_cast<size_t>(nets.end() - nets_tail_iter);
    spdlog::debug("[gpl/Netw] tail_nets_size = {}", tail_nets_size);
    spdlog::debug("[gpl/Netw] slack_tail: {:.9f} ns", slack_tail * 1e9);

    int clock_net_cnt = 0;
    int one_pin_net_cnt = 0;
    int slack_not_found_net_cnt = 0;
    int slack_head_net_cnt = 0;
    int slack_cent_net_cnt = 0;
    int slack_tail_net_cnt = 0;

    double total_head_weight = 0;
    double total_tail_weight = 0;

    const double aggressive_ratio = AGGRESSIVE_RATIO;
    auto calc_weight = [&](sta::Net* net) -> double {
        double weight = 1.0;

        auto slack = net_slack.find(net)->second;

        if (sta::fuzzyGreaterEqual(slack, slack_tail)) {
            size_t rank = std::lower_bound(
                              nets_tail_iter,
                              nets.end(),
                              net,
                              [&net_slack](const sta::Net* n1, const sta::Net* n2) {
                                  return net_slack.at(n1) < net_slack.at(n2);
                              })
                - nets_tail_iter;
            const double ranking = (tail_nets_size - rank) * 1.0 / tail_nets_size;
            weight = ranking * TAIL_RANKING_RATIO + TAIL_RANKING_OFFEST;

            total_tail_weight += weight;
            slack_tail_net_cnt++;
        } else if (sta::fuzzyGreaterEqual(slack, slack_head)) {
            weight = 1.0;
            slack_cent_net_cnt++;
        } else {
            size_t rank = std::lower_bound(
                              nets.begin(),
                              nets_head_iter,
                              net,
                              [&net_slack](const sta::Net* n1, const sta::Net* n2) {
                                  return net_slack.at(n1) < net_slack.at(n2);
                              })
                - nets.begin();
            const double ranking = (head_nets_size - rank) * 1.0 / head_nets_size;
            weight = ranking * HEAD_RANKING_RATIO + HEAD_RANKING_OFFEST;

            total_head_weight += weight;
            slack_head_net_cnt++;
        }

        const double nets_ranking = (nets_size
                                     - (std::lower_bound(
                                            nets.begin(),
                                            nets.end(),
                                            net,
                                            [&net_slack](const sta::Net* n1, const sta::Net* n2) {
                                                return net_slack.at(n1) < net_slack.at(n2);
                                            })
                                        - nets.begin()))
            * 1.0 / nets_size;
        return weight + (weight * aggressive_ratio * nets_ranking);
    };

    // update Net Weight
    spdlog::info("[gpl/Netw] LDB Nets: {}", ldb.nets().size());
    for (auto& x : ldb.mut_nets()) {
        auto& l_net = x.get();
        odb::dbNet* o_net = reinterpret_cast<odb::dbNet*>(l_net.odb_net());
        auto sta_net = nw.net(o_net);
        if (nw->isClock(sta_net)) {
            l_net.mut_weight() = (1 + aggressive_ratio * 0.5);
            clock_net_cnt++;
            continue;
        }
        if (l_net.pins().size() <= 1) {
            l_net.mut_weight() = 1.0;
            one_pin_net_cnt++;
            continue;
        }

        auto it = net_slack.find(sta_net);
        if (it == net_slack.end()) {
            l_net.mut_weight() = 1.0;
            slack_not_found_net_cnt++;
            continue;
        }

        const auto cur_weight = calc_weight(sta_net);
        const auto old_weight = l_net.weight();

        const auto cwp = CUR_WEIGHT_PERCENT;
        const auto the_weight = cwp * cur_weight + (1 - cwp) * old_weight;

        // Mut Weight
        l_net.mut_weight() = the_weight;
    }
    spdlog::info("[gpl/Netw] clock_net_cnt: {}", clock_net_cnt);
    spdlog::info("[gpl/Netw] one_pin_net_cnt: {}", one_pin_net_cnt);
    spdlog::info("[gpl/Netw] slack_not_found_net_cnt: {}", slack_not_found_net_cnt);

    spdlog::info("[gpl/Netw] slack_head_net_cnt: {}", slack_head_net_cnt);
    spdlog::info("[gpl/Netw] slack_cent_net_cnt: {}", slack_cent_net_cnt);
    spdlog::info("[gpl/Netw] slack_tail_net_cnt: {}", slack_tail_net_cnt);

    return 0;
}
} // namespace linden