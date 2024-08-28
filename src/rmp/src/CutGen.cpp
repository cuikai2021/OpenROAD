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
#include "sta/Graph.hh"
#include "sta/Liberty.hh"
#include "sta/Network.hh"
#include "sta/PathEnd.hh"
#include "sta/PathExpanded.hh"
#include "sta/PathRef.hh"
#include "sta/PatternMatch.hh"
#include "sta/PortDirection.hh"
#include "sta/Sdc.hh"
#include "sta/Search.hh"
#include "sta/Sta.hh"
#include "sta/FuncExpr.hh"
#include "sta/LibertyBuilder.hh"
#include "utl/Logger.h"

#include <rmp/FuncExpr2Kit.h>

/*
Cut Generator
*/


//#define CUTGEN_DEBUG 1

using utl::RMP;

using namespace abc;

namespace rmp {

void CutGen::GenerateInstanceWaveFrontCutSet(sta::Instance* cur_inst,
                                             std::vector<Cut*>& inst_cut_set)
{
  //filter pathological case
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

  // Expand the wave front: Keep set, Expand Set
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
    //Legalize cuts to handle escape nodes.
    AmendCutForEscapeRoots(cur_cut);
    inst_cut_set.push_back(cur_cut);
  }
}

//Add fanout not in cut to root set
void
CutGen::AmendCutForEscapeRoots(Cut* cur_cut){
  std::set<const sta::Pin*> cut_roots;
  std::set<sta::Instance*> cut_volume;
  for (auto i : cur_cut -> volume_) {
    cut_volume.insert(i);
  }
  for (auto l : cur_cut -> roots_) {
    cut_roots.insert(l);
  }
  for (auto i : cur_cut -> volume_) {
    sta::InstancePinIterator* pin_it = network_->pinIterator(i);
    while (pin_it->hasNext()) {
      sta::Pin* cur_pin = pin_it->next();
      if (network_->direction(cur_pin)->isOutput()) {
	if (cut_roots.find(cur_pin) != cut_roots.end())
	  continue;
	// Check that every volume sink is either driven by something in the volume
	// or a leaf 
	sta::PinSet visited_drvrs;
	sta::PinSeq loads;
	sta::PinSeq drvrs;
	sta::FindNetDrvrLoads visitor (cur_pin, visited_drvrs, loads, drvrs, network_);
	network_ -> visitConnectedPins(cur_pin,visitor);
	for (auto sink_pin: loads){
          sta::Instance* sink_instance = network_ ->instance(sink_pin);
	  if (cut_volume.find(sink_instance) == cut_volume.end()) {
	    if (cut_roots.find(cur_pin) == cut_roots.end()){
	      cur_cut -> roots_.push_back(cur_pin);
#ifdef CUTGEN_DEBUG        
        printf("Adding escape root %s to cut %d\n",network_ -> name(cur_pin),
               cur_cut -> id_);
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
    sta::Instance* cur_inst,
    const sta::Pin* root_pin,
    std::vector<const sta::Pin*>& keep,  // set of pins to keep
    std::vector<const sta::Pin*>& expand)
{
  std::set<const sta::Pin*> unique_driver_pins;
  std::set<sta::Instance*> unique_insts;

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
  for (auto k : keep) {
    // get the drivers..
    sta::PinSet* drvrs = network_->drivers(k);
    if (drvrs) {
      sta::PinSet::Iterator drvr_iter(drvrs);
      if (drvr_iter.hasNext()) {
        const sta::Pin* driving_pin = drvr_iter.next();
        if (unique_driver_pins.find(driving_pin) == unique_driver_pins.end()) {
#ifdef CUTGEN_DEBUG
          printf("Registering driver pin %s\n",
                 network_->pathName(driving_pin));
#endif
          ret->leaves_.push_back(const_cast<sta::Pin*>(driving_pin));
          unique_driver_pins.insert(driving_pin);
        }
      }
    }
  }

  // if root is an output of a regular gate then put in the
  // volume
  if (network_->direction(root_pin)->isOutput())
    ret->volume_.push_back(cur_inst);

  unique_insts.insert(cur_inst);

  for (auto e : expand) {
    sta::PinSet* drvrs = network_->drivers(e);
    if (drvrs) {
      sta::PinSet::Iterator drvr_iter(drvrs);
      if (drvr_iter.hasNext()) {
        const sta::Pin* driving_pin = drvr_iter.next();

        // filter: a boundary (eg memory, or primary i/o)
        if (Boundary(driving_pin)) {
          if (unique_driver_pins.find(driving_pin)
              == unique_driver_pins.end()) {
            unique_driver_pins.insert(driving_pin);
            ret->leaves_.push_back(const_cast<sta::Pin*>(driving_pin));
          }
          continue;
        }

        if (unique_driver_pins.find(driving_pin) == unique_driver_pins.end()) {
          unique_driver_pins.insert(driving_pin);
#ifdef CUTGEN_DEBUG
          printf("Expanding driving pin %s\n", network_->pathName(driving_pin));
#endif

          // expand through buffers and inverters
          driving_pin = WalkThroughBuffersAndInverters(
              ret, driving_pin, unique_driver_pins);

          // filter: a boundary (eg memory, or primary i/o)
          if (Boundary(driving_pin)) {
#ifdef CUTGEN_DEBUG
            printf("Hit boundary after going through buffers/inverters\n");
#endif
            unique_driver_pins.insert(driving_pin);
            if (!ret -> LeafPin(driving_pin)){
              ret->leaves_.push_back(const_cast<sta::Pin*>(driving_pin));
            }
            continue;
          }

          
          if (network_->direction(driving_pin)->isOutput()) {
            sta::LibertyPort* lp = network_->libertyPort(driving_pin);
            ret->internal_pins_[driving_pin] = lp;
          }

          // get the driving instance
          sta::Instance* driving_inst = network_->instance(driving_pin);
#ifdef CUTGEN_DEBUG
          printf("Expanding driving instance %s\n",
                 network_->pathName(driving_inst));
#endif

          // add to volume, uniquely
          if (unique_insts.find(driving_inst) == unique_insts.end()) {
            unique_insts.insert(driving_inst);
            ret->volume_.push_back(driving_inst);
            sta::InstancePinIterator* pin_iter
                = network_->pinIterator(driving_inst);
            while (pin_iter->hasNext()) {
              const sta::Pin* dpin = pin_iter->next();
              if (network_->direction(dpin)->isInput()) {
#ifdef CUTGEN_DEBUG
                printf("Expanding through driving instance ip %s\n",
                       network_->pathName(dpin));
#endif
                // get the driver add to cut leaves
                sta::PinSet* drvrs = network_->drivers(dpin);
                if (drvrs) {
                  sta::PinSet::Iterator drvr_iter(drvrs);
                  if (drvr_iter.hasNext()) {
                    const sta::Pin* cur_pin = drvr_iter.next();

                    // could be a primary input pin or an output pin
                    if (network_->direction(cur_pin)->isOutput()
                        || network_->term(cur_pin)) {
#ifdef CUTGEN_DEBUG
                      printf("Looking at driving pin %s\n",
                             network_->pathName(cur_pin));
#endif
                      if (unique_driver_pins.find(cur_pin)
                          == unique_driver_pins.end()) {
                        ret->leaves_.push_back(const_cast<sta::Pin*>(cur_pin));
                        unique_driver_pins.insert(cur_pin);
#ifdef CUTGEN_DEBUG
                        printf("Registering unique ip pin %s\n",
                               network_->pathName(cur_pin));
#endif
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
  }
  return ret;
}

static inline int   TruthWordNum(unsigned* p, int nVars )  {(void)p; return nVars <= 5 ? 1 : (1 << (nVars - 5));}

bool IsBuffer(unsigned* p, int n_vars){
  bool ret = true;
  int n_words = TruthWordNum(p,n_vars);
  assert(n_words > 0);
  for (int i=0; i < n_words; i++){
    if (p[i] != 0xAAAAAAAAAAAAAAAA)
      return false;
  }
  return ret;
}

bool IsInverter(unsigned* p, int n_vars){
    bool ret = true;
    int n_words = TruthWordNum(p,n_vars);
    assert(n_words > 0);
    for (int i=0; i < n_words; i++){
      if (p[i] != 0x55555555)
	return false;
    }
    return ret;
}
//
// walk through a driving pin
//
const sta::Pin* CutGen::WalkThroughBuffersAndInverters(
    Cut* cut,
    const sta::Pin* op_pin,
    std::set<const sta::Pin*>& unique_set)
{
    sta::Instance* cur_inst = network_ -> instance(op_pin);
  sta::InstancePinIterator *cpin_iter = network_ -> pinIterator(cur_inst);
  int var_count=0;
  const sta::Pin* ip_pin;
  while (cpin_iter -> hasNext()){
    const sta::Pin *c_pin = cpin_iter -> next();
    if (network_ -> direction(c_pin) -> isInput()){
      var_count++;
      ip_pin = c_pin;
    }
  }
  if (var_count == 1){
    sta::LibertyPort* lp = network_ -> libertyPort(op_pin);
    if (lp && lp -> function()){
      unsigned* kit_table = nullptr;
      std::map<int,std::string> ix2var;
      FuncExpr2Kit FuncExpr2Kit(lp -> function(), var_count, kit_table,ix2var);
            unsigned num_bits_to_set = (0x01 << var_count );			        
      unsigned mask=0x00;
      for (unsigned j=0; j < num_bits_to_set; j++)
	mask = mask | (0x1<< j);
      unsigned result = *kit_table & mask;

      if (IsBuffer(kit_table,var_count) || IsInverter(kit_table,var_count)){
	if (ip_pin){
#ifdef CUTGEN_DEBUG    	  
	  printf("Walking through buffer/inverter\n");
#endif	  
	  sta::PinSet* drvrs = network_ -> drivers(ip_pin);	  
	  sta::PinSet::Iterator drvr_iter(drvrs);
	  if (drvr_iter.hasNext()){
	    const sta::Pin* driving_pin = drvr_iter.next();
	    if (unique_set.find(driving_pin) ==
		unique_set.end()){
	      unique_set.insert(driving_pin);
	      cut -> volume_.push_back(cur_inst);
	      cut -> internal_pins_[driving_pin] = lp;	  	      
	      return WalkThroughBuffersAndInverters(cut,driving_pin,unique_set);
	    }
	    else
	      return driving_pin;
	  }
	}
      }
    }
  }
  //default do nothing
  return op_pin;
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
    expansion_set.push_back(std::pair<std::vector<const
    sta::Pin*>,std::vector<const sta::Pin*> >(keep,expand));
    keep.clear();
    expand.clear();
    keep.push_back(p1);
    expansion_set.push_back(std::pair<std::vector<const
    sta::Pin*>,std::vector<const sta::Pin*> >(keep,expand));
  }  
  else if (candidate_wavefront.size() == 2) {
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

bool CutGen::SelfFeedBack(const sta::Instance* cur_inst){

  std::set<const sta::Pin*> ops;
  sta::InstancePinIterator* ipin_iter = network_->pinIterator(cur_inst);
#ifdef CUTGEN_DEBUG  
  printf("Checking self feedback for %s\n",
         network_ -> name(cur_inst));
#endif  
  while (ipin_iter->hasNext()) {
    const sta::Pin* cur_pin = ipin_iter->next();
    if (network_->direction(cur_pin)->isOutput()) {
      	sta::PinSet visited_drvrs;
	sta::PinSeq loads;
	sta::PinSeq drvrs;
        sta::FindNetDrvrLoads visitor(cur_pin,visited_drvrs,loads,drvrs,network_);
        network_ -> visitConnectedPins(cur_pin,visitor);
        for (auto sink_pin: loads){
          sta::Instance* sink_instance = network_ -> instance(sink_pin);
#ifdef CUTGEN_DEBUG            
          printf("Sink load %s\n", network_ -> name(sink_instance));
#endif          
          if (sink_instance == cur_inst){
            return true;
          }
        }
    }
  }
  return false;
}


bool CutGen::Boundary(const sta::Pin* cur_pin)
{
  // a terminal
  if (network_->term(cur_pin))
    return true;
  // a memory
  sta::Instance* cur_inst = network_->instance(cur_pin);
  if (SelfFeedBack(cur_inst)){
    return true;
  }
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
                                        std::map<const sta:: Pin*, sta::LibertyPort*>& leaf_pin_to_port,
                                        std::map<const sta::Pin*, sta::FuncExpr*>& visited,
                                        sta::LibertyCell* cut_cell,
                                        sta::LibertyBuilder& lib_builder
				   )
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
                sta::FuncExpr* cur_pin_fn = ExpandFunctionExprR(nwk,
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
  sta::FuncExpr* gate_fn = BuildFuncExprR(
				     nwk, cur_inst, op_pin, op_pin_fe, leaf_pin_to_port, visited, cut_cell,lib_builder);
  visited[op_pin] = gate_fn;
  return gate_fn;
}

sta::FuncExpr* Cut::BuildFuncExprR(sta::Network* nwk,
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
            sta::LibertyPort* dup_port = lib_builder.makePort(cut_cell, lp->name());
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
                        std::vector<sta::FuncExpr*>& root_fns) {
  std::map<const sta::Pin*, sta::LibertyPort*> leaf_pin_to_port;
  std::map<const sta::Pin*, sta::FuncExpr*> visited_pins;
  for (auto i : roots_) {
    sta::LibertyPort* lp = nwk->libertyPort(i);
    if (lp && lp->function()) {
      sta::FuncExpr* lp_fn = lp->function();
      sta::FuncExpr* fe = ExpandFunctionExprR(
					 nwk, i, lp_fn, leaf_pin_to_port,
					 visited_pins, cut_cell,liberty_builder
					 );
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

}  // namespace rmp
