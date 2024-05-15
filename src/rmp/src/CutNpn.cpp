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
#include "rmp/FuncExpr2Kit.h"
#include "rmp/Restructure.h"
#include "rmp/blif.h"
#include "sta/ConcreteNetwork.hh"
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

#include <base/io/ioAbc.h>
#include <base/main/main.h>
#include <base/main/mainInt.h>
#include <map/amap/amapInt.h>

//#define DEBUG_RESTRUCT
//#define REGRESS_RESTRUCT

using utl::RMP;
using namespace sta;
using namespace abc;

namespace rmp {

// for each output we generate: /*unsigned* ttable, int &n_vars, char*
// &pCanonPerm, unsigned& uCanonPhase*/);
//


  LibertyCell* Cut::GenerateLibertyCell(Network* nwk,
					LibertyLibrary* cut_library){
    LibertyBuilder lib_builder;
    std::string cut_name = "cut_" + std::to_string(id_);    
    LibertyCell* cut_cell = lib_builder.makeCell(cut_library, cut_name.c_str(), nullptr);
    std::vector<FuncExpr*> root_fns;
    BuildFuncExpr(nwk,cut_cell,lib_builder,root_fns);
    //
    //TODO:
    //set up liberty cell npn terms here.
    //
    return cut_cell;
  }
  

  void Cut::BuildFuncExpr(Network* nwk,
			  LibertyCell* cut_cell,
			  LibertyBuilder& liberty_builder,
			  std::vector<FuncExpr*>& root_fns) {
  std::map<const Pin*, LibertyPort*> leaf_pin_to_port;
  std::map<const Pin*, FuncExpr*> visited_pins;
  for (auto i : roots_) {
    LibertyPort* lp = nwk->libertyPort(i);
    if (lp && lp->function()) {
      FuncExpr* lp_fn = lp->function();
      FuncExpr* fe = ExpandFunctionExprR(
					 nwk, i, lp_fn, leaf_pin_to_port,
					 visited_pins, cut_cell,liberty_builder
					 );
      root_fns.push_back(fe);
    }
  }
}

FuncExpr* Cut::ExpandFunctionExprR(
    Network* nwk,
    const Pin* op_pin,
    FuncExpr* op_pin_fe,
    std::map<const Pin*, LibertyPort*>& leaf_pin_to_port,
    std::map<const Pin*, FuncExpr*>& visited,
    LibertyCell* cut_cell,
    LibertyBuilder& lib_builder
				   )
{
  if (op_pin && (visited.find(op_pin) != visited.end())) {
    return visited[op_pin];
  }
  Instance* cur_inst = nwk->instance(op_pin);
  InstancePinIterator* pin_iter = nwk->pinIterator(cur_inst);
  // generate logic functions for each input
  while (pin_iter->hasNext()) {
    const Pin* ip_pin = pin_iter->next();
    if (nwk->direction(ip_pin)->isInput()) {
      PinSet* drvrs = nwk->drivers(ip_pin);
      if (drvrs) {
        PinSet::Iterator drvr_iter(drvrs);
        if (drvr_iter.hasNext()) {
          const Pin* cur_pin = drvr_iter.next();
          // stop on leaf pins
          if (LeafPin(cur_pin)) {
            LibertyPort* lp_pin = nwk->libertyPort(ip_pin);
            leaf_pin_to_port[cur_pin] = lp_pin;
          }
          // push back through internal pins
          else if (nwk->direction(cur_pin)->isOutput()) {
            if (internal_pins_.find(cur_pin) != internal_pins_.end()) {
              LibertyPort* lp = nwk->libertyPort(cur_pin);
              if (lp && lp->function()) {
                FuncExpr* cur_pin_fn = ExpandFunctionExprR(nwk,
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
  FuncExpr* gate_fn = BuildFuncExprR(
				     nwk, cur_inst, op_pin, op_pin_fe, leaf_pin_to_port, visited, cut_cell,lib_builder);
  visited[op_pin] = gate_fn;
  return gate_fn;
}

FuncExpr* Cut::BuildFuncExprR(Network* nwk,
                              Instance* cur_inst,
                              const Pin* op_pin,
                              FuncExpr* op_pin_fe,
                              std::map<const Pin*, LibertyPort*>& leaf_pin2port,
                              std::map<const Pin*, FuncExpr*>& visited_pins,
			      LibertyCell* cut_cell,			      
                              LibertyBuilder& lib_builder)
{
  FuncExpr* ret = nullptr;

  if (op_pin && (visited_pins.find(op_pin) != visited_pins.end()))
    return visited_pins[op_pin];

  if (op_pin_fe) {
    switch (op_pin_fe->op()) {
      case FuncExpr::op_port: {
        LibertyPort* lp = op_pin_fe->port();

        if (op_pin == nullptr) {
          // fish out the port on the cell
          InstancePinIterator* pin_iter = nwk->pinIterator(cur_inst);
          while (pin_iter->hasNext()) {
            const Pin* ip_pin = pin_iter->next();
            if (nwk->direction(ip_pin)->isInput()) {
              LibertyPort* candidate_lp = nwk->libertyPort(ip_pin);
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
          FuncExpr* ret = op_pin_fe->copy();
          visited_pins[op_pin] = ret;
          leaf_pin2port[op_pin] = lp;
          return ret;
        }

        PinSet* drvrs = nwk->drivers(op_pin);
        if (drvrs) {
          PinSet::Iterator drvr_iter(drvrs);
          const Pin* driving_pin = drvr_iter.next();
          //
          // driving pin is a leaf.
          //
          if (LeafPin(driving_pin)) {
            LibertyPort* dup_port = lib_builder.makePort(cut_cell, lp->name());
            FuncExpr* ret = FuncExpr::makePort(dup_port);
            visited_pins[op_pin] = ret;
            leaf_pin2port[op_pin] = dup_port;
            return ret;
          }
          LibertyPort* driving_lp = nwk->libertyPort(driving_pin);
          if (driving_lp) {
            Instance* driving_inst = nwk->instance(driving_pin);
            FuncExpr* op_fe = BuildFuncExprR(nwk,
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

      case FuncExpr::op_not: {
        // left
        FuncExpr* arg = BuildFuncExprR(nwk,
                                       cur_inst,
                                       nullptr,
                                       op_pin_fe->left(),
                                       leaf_pin2port,
                                       visited_pins,
				       cut_cell,
                                       lib_builder);
        return FuncExpr::makeNot(arg);
      } break;
      case FuncExpr::op_and: {
        FuncExpr* left = BuildFuncExprR(nwk,
                                        cur_inst,
                                        nullptr,
                                        op_pin_fe->left(),
                                        leaf_pin2port,
                                        visited_pins,
					cut_cell,
                                        lib_builder);

        FuncExpr* right = BuildFuncExprR(nwk,
                                         cur_inst,
                                         nullptr,
                                         op_pin_fe->right(),
                                         leaf_pin2port,
                                         visited_pins,
					 cut_cell,
                                         lib_builder);

        FuncExpr* ret = FuncExpr::makeAnd(left, right);
        return ret;
      } break;
      case FuncExpr::op_or: {
        FuncExpr* left = BuildFuncExprR(nwk,
                                        cur_inst,
                                        nullptr,
                                        op_pin_fe->left(),
                                        leaf_pin2port,
                                        visited_pins,
					cut_cell,
                                        lib_builder);

        FuncExpr* right = BuildFuncExprR(nwk,
                                         cur_inst,
                                         nullptr,
                                         op_pin_fe->right(),
                                         leaf_pin2port,
                                         visited_pins,
					 cut_cell,
                                         lib_builder);
        FuncExpr* ret = FuncExpr::makeOr(left, right);
        return ret;
      } break;
      default:
        return nullptr;
    }
  }
  return ret;
}

bool Cut::LeafPin(const Pin* cur_pin)
{
  for (auto i : leaves_)
    if (i == cur_pin)
      return true;
  return false;
}

}  // namespace rmp
