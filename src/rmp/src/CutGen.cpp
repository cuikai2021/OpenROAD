/////////////////////////////////////////////////////////////////////////////
//
// BSD 3-Clause License
//
// Copyright (c) 2019, The Regents of the University of California
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice, this
//   list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
//
// * Neither the name of the copyright holder nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
///////////////////////////////////////////////////////////////////////////////

#include <rmp/FuncExpr2Kit.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <sta/Corner.hh>
#include <sta/PathRef.hh>
#include <sta/VertexVisitor.hh>

#include "base/abc/abc.h"
#include "base/main/abcapis.h"
#include "db_sta/dbNetwork.hh"
#include "db_sta/dbSta.hh"
#include "map/mio/mio.h"
#include "map/mio/mioInt.h"
#include "odb/db.h"
#include "ord/OpenRoad.hh"
#include "rmp/Restructure.h"
#include "rmp/blif.h"
#include "sta/ConcreteNetwork.hh"
#include "sta/FuncExpr.hh"
#include "sta/Graph.hh"
#include "sta/Liberty.hh"
#include "sta/LibertyBuilder.hh"
#include "sta/Network.hh"
#include "sta/PathEnd.hh"
#include "sta/PathExpanded.hh"
#include "sta/PathRef.hh"
#include "sta/PatternMatch.hh"
#include "sta/PortDirection.hh"
#include "sta/Sdc.hh"
#include "sta/Search.hh"
#include "sta/Sta.hh"
#include "utl/Logger.h"

/*
Cut Generator
*/

//#define CUTGEN_DEBUG 1

using utl::RMP;

using namespace abc;

namespace rmp {

bool IsInverter(unsigned* p, int n_vars);
bool IsBuffer(unsigned* p, int n_vars);

/*
Generate the cut set walking back 2 levels of logic.
Allow walk through inverter/buffer chains.

Core Functions:
Enumerate Expansions -- simulates cartesian product of cut expansion
BuildCutFromExpansionElement -- builds a cut for each cartesian product
AmendCutForEscapeRoots -- patches a cut so any paths which go elsewhere get
appended as roots.

AccumulateInvertersBuffers -- walks through inverter and buffer chains.
ExpandRegularPin -- expands wavefront through a gate.

*/

void CutGen::GenerateInstanceWaveFrontCutSet(sta::Instance* cur_inst,
                                             std::vector<Cut*>& inst_cut_set)
{
  // filter pathological case
  if (SelfFeedBack(cur_inst))
    return;

  // build candidate wave front for cut enumeration
  std::vector<const sta::Pin*> candidate_wavefront;
  sta::InstancePinIterator* cpin_iter = network_->pinIterator(cur_inst);
  const sta::Pin* root_pin = nullptr;
  while (cpin_iter->hasNext()) {
    const sta::Pin* c_pin = cpin_iter->next();
    if (Boundary(c_pin))
      continue;

    if (network_->direction(c_pin)->isInput())
      candidate_wavefront.push_back(c_pin);
    else {
      root_pin = c_pin;
#ifdef CUTGEN_DEBUG
      printf("Registering root_pin %s\n", network_->pathName(root_pin));
#endif
    }
  }

  // Generate the various permutations of the expansion set
  // We partition the set into keep / expand nodes and generate
  // all combinations. This is like cartesian product cut generation
  // but going from op to ip.

  expansionSet expansion_set;
  EnumerateExpansions(candidate_wavefront, expansion_set);

  //
  // Build cut for each expansion
  //
  for (auto exp_el : expansion_set) {
    std::vector<const sta::Pin*> keep = exp_el.first;
    std::vector<const sta::Pin*> expand = exp_el.second;
    Cut* cur_cut
        = BuildCutFromExpansionElement(cur_inst, root_pin, keep, expand);
    if (cur_cut) {
      // Legalize cuts to handle escape nodes.
      AmendCutForEscapeRoots(cur_cut);
      inst_cut_set.push_back(cur_cut);
    }
  }
}

// Add fanout not in cut to root set
void CutGen::AmendCutForEscapeRoots(Cut* cur_cut)
{
  std::set<const sta::Pin*> cut_roots;
  std::set<const sta::Pin*> cut_leaves;
  std::set<sta::Instance*> cut_volume;

  for (auto i : cur_cut->volume_) {
    cut_volume.insert(i);
  }
  for (auto l : cur_cut->roots_) {
    cut_roots.insert(l);
  }
  for (auto l : cur_cut->leaves_) {
    cut_leaves.insert(l);
  }
  for (auto i : cur_cut->volume_) {
    sta::InstancePinIterator* pin_it = network_->pinIterator(i);
    while (pin_it->hasNext()) {
      sta::Pin* cur_pin = pin_it->next();
      if (network_->direction(cur_pin)->isOutput()) {
        if (cut_roots.find(cur_pin) != cut_roots.end())
          continue;
        if (cut_leaves.find(cur_pin) != cut_leaves.end())
          continue;

        // Check that every volume sink is either driven by something in the
        // volume or a leaf
        sta::PinSet visited_drvrs;
        sta::PinSeq loads;
        sta::PinSeq drvrs;
        sta::FindNetDrvrLoads visitor(
            cur_pin, visited_drvrs, loads, drvrs, network_);
        network_->visitConnectedPins(cur_pin, visitor);
        for (auto sink_pin : loads) {
          sta::Instance* sink_instance = network_->instance(sink_pin);
          if (cut_volume.find(sink_instance) == cut_volume.end()) {
            if (cut_roots.find(cur_pin) == cut_roots.end()) {
              cur_cut->roots_.push_back(cur_pin);
#ifdef CUTGEN_DEBUG
              printf("Adding escape root %s to cut %d\n",
                     network_->name(cur_pin),
                     cur_cut->id_);
#endif
              cut_roots.insert(cur_pin);
            }
          }
        }
      }
    }
  }
}

Cut* CutGen::BuildCutFromExpansionElement(
    sta::Instance* cur_inst,              // instance driving root pin
    const sta::Pin* root_pin,             // The current output
    std::vector<const sta::Pin*>& keep,   // set of pins to keep
    std::vector<const sta::Pin*>& expand  // pins to expand
)
{
  std::set<const sta::Pin*> leaf_driver_pins;     // The leaves
  std::set<const sta::Pin*> visited_driver_pins;  // the driver pins
  std::set<sta::Instance*> volume_insts;          // The volume, visited

  static int id;
  id++;

  Cut* ret = new Cut();
  ret->id_ = id;

#ifdef CUTGEN_DEBUG
  printf("Building cut %d from root %s\n", id, network_->pathName(root_pin));
  printf("Keep set:\n");
  for (auto k : keep) {
    printf("%s \n", network_->pathName(k));
  }
  printf("Expand set:\n");
  for (auto e : expand) {
    printf("%s \n", network_->pathName(e));
  }
#endif

  ret->roots_.push_back(const_cast<sta::Pin*>(root_pin));

  // if root is an output of a regular gate then put in the
  // volume
  if (network_->direction(root_pin)->isOutput()) {
    ret->volume_.push_back(cur_inst);
    volume_insts.insert(cur_inst);
    visited_driver_pins.insert(root_pin);
  }

  for (auto k : keep) {
    // get the drivers.
    // Skip any inverters and include in the volume
    sta::PinSet* drvrs = network_->drivers(k);
    if (drvrs) {
      sta::PinSet::Iterator drvr_iter(drvrs);
      if (drvr_iter.hasNext()) {
        const sta::Pin* driving_pin = drvr_iter.next();
        sta::Pin* driving_pin_out = nullptr;
        sta::Pin* start_pin = const_cast<sta::Pin*>(driving_pin);
        while (AccumulateInverterBuffers(start_pin,
                                         leaf_driver_pins,
                                         volume_insts,
                                         visited_driver_pins,
                                         ret,
                                         driving_pin_out)) {
          start_pin = driving_pin_out;
        }

        if (driving_pin_out) {
          visited_driver_pins.insert(driving_pin_out);
          if (leaf_driver_pins.find(driving_pin_out)
              == leaf_driver_pins.end()) {
#ifdef CUTGEN_DEBUG
            printf("keep Registering leaf driver pin %s\n",
                   network_->pathName(driving_pin_out));
#endif
            ret->leaves_.push_back(const_cast<sta::Pin*>(driving_pin_out));
            leaf_driver_pins.insert(driving_pin_out);
          }
        }
      }
    }
  }

  std::vector<const sta::Pin*> driver_pins_to_expand;

  for (auto e : expand) {
    sta::PinSet* drvrs = network_->drivers(e);
    if (drvrs) {
      sta::PinSet::Iterator drvr_iter(drvrs);
      if (drvr_iter.hasNext()) {
        const sta::Pin* driving_pin = drvr_iter.next();
        if (leaf_driver_pins.find(driving_pin) == leaf_driver_pins.end()
            && visited_driver_pins.find(driving_pin)
                   == visited_driver_pins.end()) {
          driver_pins_to_expand.push_back(driving_pin);
        }
      }
    }
  }

  for (auto d_pin : driver_pins_to_expand) {
    sta::Pin* driving_pin = const_cast<sta::Pin*>(d_pin);
    sta::Instance* candidate_buffer_inverter = network_->instance(d_pin);
    sta::Pin* fanin_driving_pin = driving_pin;
    if (IsBufferInverterInstance(candidate_buffer_inverter)) {
#ifdef CUTGEN_DEBUG
      static int debug;
      debug++;
      printf("D %d 1 Walking through inverter %s\n",
             debug,
             network_->pathName(candidate_buffer_inverter));
#endif
      while (AccumulateInverterBuffers(driving_pin,
                                       leaf_driver_pins,
                                       volume_insts,
                                       visited_driver_pins,
                                       ret,
                                       fanin_driving_pin)) {
        driving_pin = fanin_driving_pin;
      }
    }
    if (fanin_driving_pin) {
      ExpandRegularPin(const_cast<sta::Pin*>(fanin_driving_pin),
                       leaf_driver_pins,
                       volume_insts,
                       visited_driver_pins,
                       ret);
    }
  }
  return ret;
}

/*
Walk through buffers/inverters and stick them in the volume.
Returns:

True if ok to iteratively call. driving_pin_out set to next
pin.

False: if hit a boundary or non inverter/buffer cell
if driving_pin_out set to nullptr then we have nothing more to do
(hit a boundary) else we have hit a regular gate and return the
regular gate output in driving_pin_out.
*/

bool CutGen::AccumulateInverterBuffers(
    sta::Pin* bufferinverter_op_pin,
    std::set<const sta::Pin*>& leaf_driver_pins,
    std::set<sta::Instance*>& volume_insts,
    std::set<const sta::Pin*>& visited_driver_pins,

    Cut* cut,
    sta::Pin*& driving_pin_out)
{
  driving_pin_out = nullptr;
  sta::Instance* bufferinverter_inst
      = network_->instance(bufferinverter_op_pin);

  // case1: when we have been here before, avoid loops
  if (visited_driver_pins.find(bufferinverter_op_pin)
      != visited_driver_pins.end()) {
    driving_pin_out = nullptr;
    return false;
  }

  if (IsBufferInverterInstance(bufferinverter_inst)) {
    // An object we can walk through (buffer or inverter).

    if (volume_insts.find(bufferinverter_inst) == volume_insts.end()) {
      // accumulate the inverter buffer into the volume
      volume_insts.insert(bufferinverter_inst);
      cut->volume_.push_back(bufferinverter_inst);
      // only mark as visited once we have processed the pin
      visited_driver_pins.insert(bufferinverter_op_pin);
    }

    sta::InstancePinIterator* cpin_iter
        = network_->pinIterator(bufferinverter_inst);
    while (cpin_iter->hasNext()) {
      const sta::Pin* c_pin = cpin_iter->next();
      if (network_->direction(c_pin)->isInput()) {
        sta::PinSet* drvrs = network_->drivers(c_pin);
        sta::PinSet::Iterator drvr_iter(drvrs);
        if (drvr_iter.hasNext()) {
          sta::Pin* driving_pin = const_cast<sta::Pin*>(drvr_iter.next());
          sta::Instance* driving_instance = network_->instance(driving_pin);

          // if we have been here before signal nothing more to do
          if (visited_driver_pins.find(driving_pin)
              != visited_driver_pins.end()) {
            driving_pin_out = nullptr;
            return false;
          }
          // A new boundary, register the leaf
          else if (Boundary(driving_pin)) {
            visited_driver_pins.insert(driving_pin);
            if (leaf_driver_pins.find(driving_pin) == leaf_driver_pins.end()) {
              leaf_driver_pins.insert(driving_pin);
#ifdef CUTGEN_DEBUG
              printf("(boundary) Registering leaf driver pin %s\n",
                     network_->pathName(driving_pin));
#endif
              cut->leaves_.push_back(const_cast<sta::Pin*>(driving_pin));
            }
            driving_pin_out = nullptr;
            return false;
          } else {
            // proceed to continue next gate
            driving_pin_out = driving_pin;
            return true;
          }
        }
      }
    }
  } else {
    // not an inverter/buffer, stop iterating
    // pick up from fanin gate
    driving_pin_out = bufferinverter_op_pin;
    return false;
  }

  // default.
  driving_pin_out = nullptr;
  return false;
}

void CutGen::ExpandRegularPin(sta::Pin* driving_pin,
                              std::set<const sta::Pin*>& leaf_driver_pins,
                              std::set<sta::Instance*>& volume_insts,
                              std::set<const sta::Pin*>& visited_driver_pins,
                              Cut* cut)
{
  if (visited_driver_pins.find(driving_pin) != visited_driver_pins.end()) {
#ifdef CUTGEN_DEBUG
    printf("Not expanding visited pin %s\n", network_->name(driving_pin));
#endif
    return;
  }

  // a boundary (eg memory, or primary i/o)
  // or even something already connected to a leaf.
  sta::Instance* driving_inst = network_->instance(driving_pin);

  // Case 1 : a boundary
  if (Boundary(driving_pin)
      || InstanceOpConnectedToCutBoundary(driving_inst, cut)) {
#ifdef CUTGEN_DEBUG
    printf("Expand Regular pin hit boundary %s\n",
           network_->name(driving_inst));
#endif
    if (leaf_driver_pins.find(driving_pin) == leaf_driver_pins.end()) {
      leaf_driver_pins.insert(driving_pin);
#ifdef CUTGEN_DEBUG
      printf("2 (boundary) Registering leaf driver pin %s\n",
             network_->pathName(driving_pin));
#endif
      cut->leaves_.push_back(const_cast<sta::Pin*>(driving_pin));
      visited_driver_pins.insert(driving_pin);
    }
    return;
  }

  // harmless but should be in unique_inst check
  if (network_->direction(driving_pin)->isOutput()) {
    sta::LibertyPort* lp = network_->libertyPort(driving_pin);
    cut->internal_pins_[driving_pin] = lp;
  }

  if (volume_insts.find(driving_inst) == volume_insts.end()) {
    volume_insts.insert(driving_inst);
    cut->volume_.push_back(driving_inst);
    visited_driver_pins.insert(driving_pin);

#ifdef CUTGEN_DEBUG
    printf("Expanding from regular pin. Accumulating inst %s\n",
           network_->name(driving_inst));
#endif

    sta::InstancePinIterator* pin_iter = network_->pinIterator(driving_inst);
    while (pin_iter->hasNext()) {
      const sta::Pin* dpin = pin_iter->next();
      if (network_->direction(dpin)->isInput()) {
        // get the driver add to cut leaves
        sta::PinSet* drvrs = network_->drivers(dpin);
        if (drvrs) {
          sta::PinSet::Iterator drvr_iter(drvrs);
          if (drvr_iter.hasNext()) {
            const sta::Pin* cur_pin = drvr_iter.next();

            if (network_->direction(cur_pin)->isOutput()
                || network_->term(cur_pin)) {
              if (visited_driver_pins.find(cur_pin)
                  != visited_driver_pins.end())
                continue;

#ifdef CUTGEN_DEBUG
              printf("Looking at driving pin %s\n",
                     network_->pathName(cur_pin));
#endif

              sta::Pin* driving_pin_out = nullptr;
              sta::Pin* start_pin = const_cast<sta::Pin*>(cur_pin);
              while (AccumulateInverterBuffers(start_pin,
                                               leaf_driver_pins,
                                               volume_insts,
                                               visited_driver_pins,
                                               cut,
                                               driving_pin_out)) {
                start_pin = driving_pin_out;
              }
              if (driving_pin_out) {
                if (visited_driver_pins.find(driving_pin_out)
                    == visited_driver_pins.end()) {
                  visited_driver_pins.insert(driving_pin_out);

#ifdef CUTGEN_DEBUG
                  printf("Candidate leaf pin %s\n",
                         network_->pathName(driving_pin_out));
#endif
                  if (leaf_driver_pins.find(driving_pin_out)
                      == leaf_driver_pins.end()) {
#ifdef CUTGEN_DEBUG
                    printf("4 (Expand pin) Registering leaf driver pin %s\n",
                           network_->pathName(driving_pin_out));
#endif
                    cut->leaves_.push_back(
                        const_cast<sta::Pin*>(driving_pin_out));
                    leaf_driver_pins.insert(driving_pin_out);
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}

bool CutGen::IsBufferInverterInstance(sta::Instance* inst)
{
  sta::InstancePinIterator* cpin_iter = network_->pinIterator(inst);
  int var_count = 0;
  int op_count = 0;
  const sta::Pin* ip_pin;
  const sta::Pin* op_pin;
  while (cpin_iter->hasNext()) {
    const sta::Pin* c_pin = cpin_iter->next();
    if (network_->direction(c_pin)->isInput()) {
      var_count++;
      ip_pin = c_pin;
    } else {
      op_count++;
      op_pin = c_pin;
    }
  }
  (void) ip_pin;
  if (var_count == 1 && op_count == 1) {
    sta::LibertyPort* lp = network_->libertyPort(op_pin);
    if (lp && lp->function()) {
      unsigned* kit_table = nullptr;
      std::map<int, std::string> ix2var;
      FuncExpr2Kit FuncExpr2Kit(lp->function(), var_count, kit_table, ix2var);
      unsigned num_bits_to_set = (0x01 << var_count);
      unsigned mask = 0x00;
      for (unsigned j = 0; j < num_bits_to_set; j++)
        mask = mask | (0x1 << j);
      unsigned result = *kit_table & mask;
      if (IsBuffer(kit_table, var_count) || IsInverter(kit_table, var_count)) {
        return true;
      }
    }
  }
  return false;
}

static inline int TruthWordNum(unsigned* p, int nVars)
{
  (void) p;
  return nVars <= 5 ? 1 : (1 << (nVars - 5));
}

bool IsBuffer(unsigned* p, int n_vars)
{
  bool ret = true;
  int n_words = TruthWordNum(p, n_vars);
  assert(n_words > 0);
  for (int i = 0; i < n_words; i++) {
    if (p[i] != 0xAAAAAAAAAAAAAAAA)
      return false;
  }
  return ret;
}

bool IsInverter(unsigned* p, int n_vars)
{
  bool ret = true;
  int n_words = TruthWordNum(p, n_vars);
  assert(n_words > 0);
  for (int i = 0; i < n_words; i++) {
    if (p[i] != 0x55555555)
      return false;
  }
  return ret;
}

bool CutGen::EnumerateExpansions(
    std::vector<const sta::Pin*>& candidate_wavefront,
    expansionSet& expansion_set)
{
  // equivalent to the cartesian product, but organized for physical level.
  // remove trivial cases
  if (candidate_wavefront.size() == 1) {
    std::vector<const sta::Pin*> keep;
    std::vector<const sta::Pin*> expand;
    const sta::Pin* p1 = candidate_wavefront[0];
    expand.push_back(p1);
    expansion_set.push_back(
        std::pair<std::vector<const sta::Pin*>, std::vector<const sta::Pin*>>(
            keep, expand));
    keep.clear();
    expand.clear();
    keep.push_back(p1);
    expansion_set.push_back(
        std::pair<std::vector<const sta::Pin*>, std::vector<const sta::Pin*>>(
            keep, expand));
  } else if (candidate_wavefront.size() == 2) {
    const sta::Pin* p1 = candidate_wavefront[0];
    const sta::Pin* p2 = candidate_wavefront[1];
    std::vector<const sta::Pin*> keep;
    std::vector<const sta::Pin*> expand;

    // case analysis
    /*
    keep.push_back(p1);
    keep.push_back(p2);
    expansion_set.push_back(std::pair<std::vector<const
    sta::Pin*>,std::vector<const sta::Pin*> >(keep,expand));

    keep.clear();
    expand.clear();
    keep.push_back(p1);
    expand.push_back(p2);
    expansion_set.push_back(std::pair<std::vector<const
    sta::Pin*>,std::vector<const sta::Pin*> >(keep,expand));

    keep.clear();
    expand.clear();
    */

    keep.push_back(p2);
    expand.push_back(p1);
    expansion_set.push_back(
        std::pair<std::vector<const sta::Pin*>, std::vector<const sta::Pin*>>(
            keep, expand));

    keep.clear();
    expand.clear();

    expand.push_back(p1);
    expand.push_back(p2);
    expansion_set.push_back(
        std::pair<std::vector<const sta::Pin*>, std::vector<const sta::Pin*>>(
            keep, expand));

    return true;
  }

  //
  // N == 3
  //
  // K:{p1,p2,p3} E:{}
  // K:{p1} E:{p2,p3}
  // K:{p1,p2}  E: {p3}
  // K: {p1,p3} E: {p2}

  // E:{p1,p2,p3} K:{}
  // E:{p1} K:{p2,p3}
  // E:{p1,p2}  K: {p3}
  // E: {p1,p3} K: {p2}

  else if (candidate_wavefront.size() == 3) {
    const sta::Pin* p1 = candidate_wavefront[0];
    const sta::Pin* p2 = candidate_wavefront[1];
    const sta::Pin* p3 = candidate_wavefront[2];
    std::vector<const sta::Pin*> keep;
    std::vector<const sta::Pin*> expand;

    // case analysis
    /*
    keep.push_back(p1);
    keep.push_back(p2);
    keep.push_back(p3);
    expansion_set.push_back(std::pair<std::vector<const
    sta::Pin*>,std::vector<const sta::Pin*> >(keep,expand));

    keep.clear();
    expand.clear();
    */

    keep.push_back(p1);
    expand.push_back(p2);
    expand.push_back(p3);
    expansion_set.push_back(
        std::pair<std::vector<const sta::Pin*>, std::vector<const sta::Pin*>>(
            keep, expand));

    keep.clear();
    expand.clear();
    keep.push_back(p1);
    keep.push_back(p2);
    expand.push_back(p3);
    expansion_set.push_back(
        std::pair<std::vector<const sta::Pin*>, std::vector<const sta::Pin*>>(
            keep, expand));

    keep.clear();
    expand.clear();

    keep.push_back(p1);
    keep.push_back(p3);
    expand.push_back(p2);
    expansion_set.push_back(
        std::pair<std::vector<const sta::Pin*>, std::vector<const sta::Pin*>>(
            keep, expand));

    keep.clear();
    expand.clear();
    expand.push_back(p1);
    expand.push_back(p2);
    expand.push_back(p3);
    expansion_set.push_back(
        std::pair<std::vector<const sta::Pin*>, std::vector<const sta::Pin*>>(
            keep, expand));

    keep.clear();
    expand.clear();
    expand.push_back(p1);
    keep.push_back(p2);
    keep.push_back(p3);
    expansion_set.push_back(
        std::pair<std::vector<const sta::Pin*>, std::vector<const sta::Pin*>>(
            keep, expand));

    keep.clear();
    expand.clear();
    expand.push_back(p1);
    expand.push_back(p2);
    keep.push_back(p3);
    expansion_set.push_back(
        std::pair<std::vector<const sta::Pin*>, std::vector<const sta::Pin*>>(
            keep, expand));

    keep.clear();
    expand.clear();
    expand.push_back(p1);
    expand.push_back(p3);
    keep.push_back(p2);
    expansion_set.push_back(
        std::pair<std::vector<const sta::Pin*>, std::vector<const sta::Pin*>>(
            keep, expand));
    return true;
  } else if (candidate_wavefront.size() == 4) {
    //
    // N == 4
    //
    // K:{p1,p2,p3,p4} E:{}

    // K:{p1} E:{p2,p3,p4}
    // K:{p1,p2}  E: {p3,p4}
    // K:{p1,p2,p3} E: {p4}

    // K:{p2} E: {p1,p3,p4}
    // K:{p2,p3} E: {p1,p4}
    // K:{p2,p3,p4} E: {p1}

    // K:{p3} E: {p1,p2,p4}
    // K:{p3,p1} E: {p2,p4}
    // K:{p3,p1,p2} E: {p4}

    // K:{} E{p1,p2,p3,p4}

    // E:{p1,p2,p3} K:{}
    // E:{p1} K:{p2,p3}
    // E:{p1,p2}  K: {p3}
    // E: {p1,p3} K: {p2}
  }
  return false;
}

bool CutGen::InstanceOpConnectedToCutBoundary(const sta::Instance* cur_inst,
                                              Cut* cur_cut)
{
  std::unique_ptr<sta::InstancePinIterator> ipin_iter{
      network_->pinIterator(cur_inst)};
  while (ipin_iter->hasNext()) {
    const sta::Pin* cur_pin = ipin_iter->next();
    if (network_->direction(cur_pin)->isOutput()) {
      if (cur_cut->LeafPin(cur_pin)) {
        return true;
      }
      if (cur_cut->RootPin(cur_pin)) {
        return true;
      }
    }
  }
  return false;
}

unsigned CutGen::OpPinCount(const sta::Instance* cur_inst)
{
  unsigned ret = 0;
  std::unique_ptr<sta::InstancePinIterator> ipin_iter{
      network_->pinIterator(cur_inst)};
  while (ipin_iter->hasNext()) {
    const sta::Pin* cur_pin = ipin_iter->next();
    if (network_->direction(cur_pin)->isOutput()) {
      ret++;
    }
  }
  return ret;
}

bool CutGen::SelfFeedBack(const sta::Instance* cur_inst)
{
  std::set<const sta::Pin*> ops;
  sta::InstancePinIterator* ipin_iter = network_->pinIterator(cur_inst);
#ifdef CUTGEN_DEBUG
  printf("Checking self feedback for %s\n", network_->name(cur_inst));
#endif
  while (ipin_iter->hasNext()) {
    const sta::Pin* cur_pin = ipin_iter->next();
    if (network_->direction(cur_pin)->isOutput()) {
      sta::PinSet visited_drvrs;
      sta::PinSeq loads;
      sta::PinSeq drvrs;
      sta::FindNetDrvrLoads visitor(
          cur_pin, visited_drvrs, loads, drvrs, network_);
      network_->visitConnectedPins(cur_pin, visitor);
      for (auto sink_pin : loads) {
        sta::Instance* sink_instance = network_->instance(sink_pin);
#ifdef CUTGEN_DEBUG
        printf("Sink load %s\n", network_->name(sink_instance));
#endif
        if (sink_instance == cur_inst) {
          return true;
        }
      }
    }
  }
  return false;
}

//
// Find out if a pin is a boundary
// Note there is one other boundary case:
// when a pin is marked as a leaf of cut.
//
bool CutGen::Boundary(const sta::Pin* cur_pin)
{
  // a terminal
  if (network_->term(cur_pin))
    return true;

  sta::Instance* cur_inst = network_->instance(cur_pin);

  // a multiple output gate
  if (OpPinCount(cur_inst) > 1) {
    return true;
  }

  // a feedback loop
  if (SelfFeedBack(cur_inst)) {
    return true;
  }
  // a memory
  if (cur_inst) {
    sta::LibertyCell* cell = network_->libertyCell(cur_inst);
    if (cell->isMemory() || cell->hasSequentials())
      return true;
  }
  return false;
}

sta::FuncExpr* Cut::ExpandFunctionExprR(
    sta::Network* nwk,
    const sta::Pin* op_pin,
    sta::FuncExpr* op_pin_fe,
    std::map<const sta::Pin*, sta::LibertyPort*>& leaf_pin_to_port,
    std::map<const sta::Pin*, sta::FuncExpr*>& visited,
    sta::LibertyCell* cut_cell,
    sta::LibertyBuilder& lib_builder)
{
  if (op_pin && (visited.find(op_pin) != visited.end())) {
    return visited[op_pin];
  }
  sta::Instance* cur_inst = nwk->instance(op_pin);
  sta::InstancePinIterator* pin_iter = nwk->pinIterator(cur_inst);
  // generate logic functions for each input
  while (pin_iter->hasNext()) {
    const sta::Pin* ip_pin = pin_iter->next();
    if (nwk->direction(ip_pin)->isInput()) {
      sta::PinSet* drvrs = nwk->drivers(ip_pin);
      if (drvrs) {
        sta::PinSet::Iterator drvr_iter(drvrs);
        if (drvr_iter.hasNext()) {
          const sta::Pin* cur_pin = drvr_iter.next();
          // stop on leaf pins
          if (LeafPin(cur_pin)) {
            sta::LibertyPort* lp_pin = nwk->libertyPort(ip_pin);
            leaf_pin_to_port[cur_pin] = lp_pin;
          }
          // push back through internal pins
          else if (nwk->direction(cur_pin)->isOutput()) {
            if (internal_pins_.find(cur_pin) != internal_pins_.end()) {
              sta::LibertyPort* lp = nwk->libertyPort(cur_pin);
              if (lp && lp->function()) {
                sta::FuncExpr* cur_pin_fn
                    = ExpandFunctionExprR(nwk,
                                          cur_pin,
                                          lp->function(),
                                          leaf_pin_to_port,
                                          visited,
                                          cut_cell,
                                          lib_builder);
                // register the internal pin function
                visited[cur_pin] = cur_pin_fn;
              }
            }
          }
        }
      }
    }
  }
  // make function for gate from inputs
  sta::FuncExpr* gate_fn = BuildFuncExprR(nwk,
                                          cur_inst,
                                          op_pin,
                                          op_pin_fe,
                                          leaf_pin_to_port,
                                          visited,
                                          cut_cell,
                                          lib_builder);
  visited[op_pin] = gate_fn;
  return gate_fn;
}

sta::FuncExpr* Cut::BuildFuncExprR(
    sta::Network* nwk,
    sta::Instance* cur_inst,
    const sta::Pin* op_pin,
    sta::FuncExpr* op_pin_fe,
    std::map<const sta::Pin*, sta::LibertyPort*>& leaf_pin2port,
    std::map<const sta::Pin*, sta::FuncExpr*>& visited_pins,
    sta::LibertyCell* cut_cell,
    sta::LibertyBuilder& lib_builder)
{
  sta::FuncExpr* ret = nullptr;

  if (op_pin && (visited_pins.find(op_pin) != visited_pins.end()))
    return visited_pins[op_pin];

  if (op_pin_fe) {
    switch (op_pin_fe->op()) {
      case sta::FuncExpr::op_port: {
        sta::LibertyPort* lp = op_pin_fe->port();

        if (op_pin == nullptr) {
          // fish out the port on the cell
          sta::InstancePinIterator* pin_iter = nwk->pinIterator(cur_inst);
          while (pin_iter->hasNext()) {
            const sta::Pin* ip_pin = pin_iter->next();
            if (nwk->direction(ip_pin)->isInput()) {
              sta::LibertyPort* candidate_lp = nwk->libertyPort(ip_pin);
              if (candidate_lp == lp) {
                op_pin = ip_pin;
              }
            }
          }
        }

        assert(op_pin);

        if ((visited_pins.find(op_pin) != visited_pins.end()))
          return visited_pins[op_pin];

        if (LeafPin(op_pin)) {
          sta::FuncExpr* ret = op_pin_fe->copy();
          visited_pins[op_pin] = ret;
          leaf_pin2port[op_pin] = lp;
          return ret;
        }

        sta::PinSet* drvrs = nwk->drivers(op_pin);
        if (drvrs) {
          sta::PinSet::Iterator drvr_iter(drvrs);
          const sta::Pin* driving_pin = drvr_iter.next();
          //
          // driving pin is a leaf.
          //
          if (LeafPin(driving_pin)) {
            sta::LibertyPort* dup_port
                = lib_builder.makePort(cut_cell, lp->name());
            sta::FuncExpr* ret = sta::FuncExpr::makePort(dup_port);
            visited_pins[op_pin] = ret;
            leaf_pin2port[op_pin] = dup_port;
            return ret;
          }
          sta::LibertyPort* driving_lp = nwk->libertyPort(driving_pin);
          if (driving_lp) {
            sta::Instance* driving_inst = nwk->instance(driving_pin);
            sta::FuncExpr* op_fe = BuildFuncExprR(nwk,
                                                  driving_inst,
                                                  driving_pin,
                                                  driving_lp->function(),
                                                  leaf_pin2port,
                                                  visited_pins,
                                                  cut_cell,
                                                  lib_builder);
            visited_pins[driving_pin] = op_fe;
            return op_fe;
          }
        }
      } break;

      case sta::FuncExpr::op_not: {
        // left
        sta::FuncExpr* arg = BuildFuncExprR(nwk,
                                            cur_inst,
                                            nullptr,
                                            op_pin_fe->left(),
                                            leaf_pin2port,
                                            visited_pins,
                                            cut_cell,
                                            lib_builder);
        return sta::FuncExpr::makeNot(arg);
      } break;
      case sta::FuncExpr::op_and: {
        sta::FuncExpr* left = BuildFuncExprR(nwk,
                                             cur_inst,
                                             nullptr,
                                             op_pin_fe->left(),
                                             leaf_pin2port,
                                             visited_pins,
                                             cut_cell,
                                             lib_builder);

        sta::FuncExpr* right = BuildFuncExprR(nwk,
                                              cur_inst,
                                              nullptr,
                                              op_pin_fe->right(),
                                              leaf_pin2port,
                                              visited_pins,
                                              cut_cell,
                                              lib_builder);

        sta::FuncExpr* ret = sta::FuncExpr::makeAnd(left, right);
        return ret;
      } break;
      case sta::FuncExpr::op_or: {
        sta::FuncExpr* left = BuildFuncExprR(nwk,
                                             cur_inst,
                                             nullptr,
                                             op_pin_fe->left(),
                                             leaf_pin2port,
                                             visited_pins,
                                             cut_cell,
                                             lib_builder);

        sta::FuncExpr* right = BuildFuncExprR(nwk,
                                              cur_inst,
                                              nullptr,
                                              op_pin_fe->right(),
                                              leaf_pin2port,
                                              visited_pins,
                                              cut_cell,
                                              lib_builder);
        sta::FuncExpr* ret = sta::FuncExpr::makeOr(left, right);
        return ret;
      } break;
      default:
        return nullptr;
    }
  }
  return ret;
}

void Cut::BuildFuncExpr(sta::Network* nwk,
                        sta::LibertyCell* cut_cell,
                        sta::LibertyBuilder& liberty_builder,
                        std::vector<sta::FuncExpr*>& root_fns)
{
  std::map<const sta::Pin*, sta::LibertyPort*> leaf_pin_to_port;
  std::map<const sta::Pin*, sta::FuncExpr*> visited_pins;
  for (auto i : roots_) {
    sta::LibertyPort* lp = nwk->libertyPort(i);
    if (lp && lp->function()) {
      sta::FuncExpr* lp_fn = lp->function();
      sta::FuncExpr* fe = ExpandFunctionExprR(nwk,
                                              i,
                                              lp_fn,
                                              leaf_pin_to_port,
                                              visited_pins,
                                              cut_cell,
                                              liberty_builder);
      root_fns.push_back(fe);
    }
  }
}

bool Cut::LeafPin(const sta::Pin* cur_pin)
{
  for (auto i : leaves_)
    if (i == cur_pin)
      return true;
  return false;
}

bool Cut::RootPin(const sta::Pin* cur_pin)
{
  for (auto i : roots_)
    if (i == cur_pin)
      return true;
  return false;
}

}  // namespace rmp
