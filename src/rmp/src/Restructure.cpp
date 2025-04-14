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

#include "rmp/Restructure.h"

#include <base/io/ioAbc.h>
#include <base/main/main.h>
#include <base/main/mainInt.h>
#include <map/amap/amapInt.h>
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
#include <vector>

#include "base/abc/abc.h"
#include "base/main/abcapis.h"
#include "db_sta/dbNetwork.hh"
#include "db_sta/dbSta.hh"
#include "map/mio/mio.h"
#include "map/mio/mioInt.h"
#include "odb/db.h"
#include "ord/OpenRoad.hh"
#include "rmp/FuncExpr2Kit.h"
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
#include "utl/Logger.h"

//#define DEBUG_RESTRUCT
//#define DEBUG_GENERIC_RESTRUCT 1
//#define REGRESS_RESTRUCT
//#define DEBUG_CUT_RESYNTH
// extern "C"              void Abc_TruthNpnRPrint( char * pCanonPermInit,
// unsigned uCanonPhase, int nVars );

using utl::RMP;

using namespace abc;

namespace rmp {

void Restructure::init(utl::Logger* logger,
                       sta::dbSta* open_sta,
                       odb::dbDatabase* db,
                       rsz::Resizer* resizer)
{
  logger_ = logger;
  db_ = db;
  open_sta_ = open_sta;
  resizer_ = resizer;
}

void Restructure::deleteComponents()
{
}
Restructure::~Restructure()
{
  deleteComponents();
}

void Restructure::reset()
{
  lib_file_names_.clear();
  path_insts_.clear();
}

//
// Cut Driven Retructuring
//-----------------------
//
// Parameters:
// For cut generation:
// single_op -> single output or multiple output.
// unconstrained -> if true then walk back to invariant points during cut
// generation
//                  else use wavefront (cartesian product of non trivial fanin
//                  gates).
// oracle remap -> if true look up cut match
// script -> if true use abc script (else do simply amap remapping).
//
//

void Restructure::cutresynthrun(char* target_library,
                                char* script,
                                sta::Pin* head_pin,
                                bool single_op,
                                bool unconstrained,
                                bool oracle_remap,
                                bool verbose,
                                bool verify)
{
  reset();

  sta::Sta* sta = sta::Sta::sta();
  sta::Network* network = sta->network();
  sta::Graph* graph = sta->ensureGraph();
  sta->ensureLevelized();
  sta->searchPreamble();
  logger_->report("Cut based Restructuring.");

  sta::LibertyLibrary* cut_library = nullptr;
  if (oracle_remap) {
    cut_library = new sta::LibertyLibrary("cut_library", "cut_library.lib");
  }

#ifdef REGRESS_RESTRUCT
  {
    FILE* cond_file = fopen("cmd.txt", "w+");
    std::string tl = target_library;
    size_t last_idx = tl.find_last_of('/');
    if (last_idx != std::string::npos)
      tl = tl.substr(last_idx + 1);
    std::string default_script
        = "read_lib " + tl + " ;strash ; balance; amap -Q 9.9 ";
    fprintf(
        cond_file, "Script: %s \n", script ? script : default_script.c_str());
    fclose(cond_file);
  }
#endif
  LibRead liberty_library(logger_, target_library, false);

  Cut* cut_to_remap = nullptr;

  std::vector<Cut*> cut_set;
  if (head_pin) {
    generateWaveFrontSingleOpCutSet(network, head_pin, cut_set);
    logger_->report("Generated {} constrained cuts.", cut_set.size());
  } else {
    generateUnconstrainedCuts(cut_set, single_op);
    logger_->report("Generated  {} unconstrained cuts.", cut_set.size());
  }

  //
  // For test just pick one cut: choice one with biggest volume
  //

  Cut* best_cut = nullptr;
  for (auto c : cut_set) {
    if (best_cut == nullptr)
      best_cut = c;
    else if (c->volume_.size() > best_cut->volume_.size())
      best_cut = c;
  }
  if (!best_cut)
    return;

  // remap the cut set.
  // For now only remap the biggest cut

  if (best_cut->volume_.size() > 0) {
    logger_->report(
        "Processing cut {} of volume {} with {} roots and {} leaves.",
        best_cut->id_,
        best_cut->volume_.size(),
        best_cut->roots_.size(),
        best_cut->leaves_.size());
    cut_to_remap = best_cut;

    if (verify) {
      cut_to_remap->Print(stdout, network);
      char fname[1000];
      sprintf(fname, "beforemap_cut_%d.v", cut_to_remap->id_);
      printf("Input cut to be remapped saved in verification file %s\n", fname);
      FILE* fp = fopen(fname, "w+");
      cut_to_remap->PrintVerilog(fp, network);
      fclose(fp);
    }

    //
    // We are going to remap "cut_to_remap"
    //
    // Get the timing numbers for the cut from the sta
    // Explicitly pass them into optimizer
    std::vector<std::pair<const sta::Pin*, TimingRecord*>> timing_requirements;
    annotateCutTiming(cut_to_remap, timing_requirements);

    // Remap the cut with the physical timing constraints
    // onto the target library.
    bool script_present = false;
    std::string script_name;
    if (script && strlen(script) > 0) {
      script_name = script;
      script_present = true;
    }
    if (!block_) {
      block_ = db_->getChip()->getBlock();
    }

    PhysRemap prm(logger_,
                  network,
                  cut_to_remap,
                  timing_requirements,
                  target_library,
                  liberty_library,
                  script_present,
                  script_name,
                  true,
                  block_,
                  locell_,
                  loport_,
                  hicell_,
                  hiport_,
                  verify);
    prm.Remap();
  }
}

static const char* unpack_data(const char* c, void* ptr, size_t sz)
{
  unsigned char* u = (unsigned char*) ptr;
  const unsigned char* eu = u + sz;
  for (; u != eu; ++u) {
    char d = *(c++);
    unsigned char uu;
    if ((d >= '0') && (d <= '9'))
      uu = (unsigned char) ((d - '0') << 4);
    else if ((d >= 'a') && (d <= 'f'))
      uu = (unsigned char) ((d - ('a' - 10)) << 4);
    else
      return (char*) 0;
    d = *(c++);
    if ((d >= '0') && (d <= '9'))
      uu |= (unsigned char) (d - '0');
    else if ((d >= 'a') && (d <= 'f'))
      uu |= (unsigned char) (d - ('a' - 10));
    else
      return (char*) 0;
    *u = uu;
  }
  return c;
}

void convertPtrFromString(const char* c, void** ptr)
{
  assert(*c == '_');
  c++;
  c = unpack_data(c, ptr, sizeof(void*));
}

void Restructure::run(
    char* liberty_file_name,
    float slack_threshold,
    unsigned max_depth,
    char* workdir_name,
    char* abc_logfile,
    bool cut_restructure,  // if true, use cut restructuring
    // cut mapping options
    char* head_pin_str,  // if set we start resynthesis from here
    bool unconstrained,  // if true, don't use wavefront method for cuts
    bool single_op_cut,  // if true, only use single output cuts,
    bool oracle,         // if true use oracle remapping method
    char* script,
    bool verify)  // if set we apply script method.

{
  if (cut_restructure == true) {
    sta::Pin* head_pin = nullptr;
    void* ptr = nullptr;
    if (head_pin_str == nullptr || *head_pin_str == '0'
        || *head_pin_str == '\0')
      head_pin = nullptr;
    else {
#ifdef DEBUG_GENERIC_RESTRUCT
      printf("Converting head string pointer from %s\n", head_pin_str);
#endif
      convertPtrFromString(head_pin_str, &ptr);
    }
    if (ptr) {
      head_pin = (sta::Pin*) ptr;
    }
    cutresynthrun(liberty_file_name,
                  script,
                  head_pin,
                  single_op_cut,
                  unconstrained,
                  oracle,
                  false,
                  verify);
  } else {
    reset();
    block_ = db_->getChip()->getBlock();
    if (!block_)
      return;
    logfile_ = abc_logfile;
    // unused ?
    sta::Slack worst_slack = slack_threshold;
    lib_file_names_.emplace_back(liberty_file_name);
    work_dir_name_ = workdir_name;
    work_dir_name_ = work_dir_name_ + "/";
    if (is_area_mode_)  // Only in area mode
      removeConstCells();
    getBlob(max_depth);
    if (path_insts_.size()) {
      runABC();
      postABC(worst_slack);
    }
  }
}

void Restructure::getBlob(unsigned max_depth)
{
  open_sta_->ensureGraph();
  open_sta_->ensureLevelized();
  open_sta_->searchPreamble();

  //
  //  sta::PinSet ends;
  //
  sta::PinSet ends(open_sta_->getDbNetwork());

  getEndPoints(ends, is_area_mode_, max_depth);
  if (ends.size()) {
    sta::PinSet boundary_points = !is_area_mode_
                                      ? resizer_->findFanins(ends)
                                      : resizer_->findFaninFanouts(ends);
    // fanin_fanouts.insert(ends.begin(), ends.end()); // Add seq cells
    logger_->report("Found {} pins in extracted logic.",
                    boundary_points.size());
    for (const sta::Pin* pin : boundary_points) {
      odb::dbITerm* term = nullptr;
      odb::dbBTerm* port = nullptr;
      odb::dbModITerm* moditerm = nullptr;
      odb::dbModBTerm* modbterm = nullptr;
      open_sta_->getDbNetwork()->staToDb(pin, term, port, moditerm, modbterm);
      if (term && !term->getInst()->getMaster()->isBlock())
        path_insts_.insert(term->getInst());
    }
    logger_->report("Found {} instances for restructuring.",
                    path_insts_.size());
  }
}

void Restructure::runABC()
{
  input_blif_file_name_ = work_dir_name_ + std::string(block_->getConstName())
                          + "_crit_path.blif";
  std::vector<std::string> files_to_remove;

  debugPrint(logger_,
             utl::RMP,
             "remap",
             1,
             "Constants before remap {}",
             countConsts(block_));

  Blif blif_(logger_, open_sta_, locell_, loport_, hicell_, hiport_);
  blif_.setReplaceableInstances(path_insts_);
  blif_.writeBlif(input_blif_file_name_.c_str(), !is_area_mode_);
  debugPrint(
      logger_, RMP, "remap", 1, "Writing blif file {}", input_blif_file_name_);
  files_to_remove.emplace_back(input_blif_file_name_);

  // abc optimization
  std::vector<Mode> modes;
  std::vector<pid_t> child_proc;

  if (is_area_mode_) {
    // Area Mode
    modes = {Mode::AREA_1, Mode::AREA_2, Mode::AREA_3};
  } else {
    // Delay Mode
    modes = {Mode::DELAY_1, Mode::DELAY_2, Mode::DELAY_3, Mode::DELAY_4};
  }

  child_proc.resize(modes.size(), 0);

  std::string best_blif;
  int best_inst_count = std::numeric_limits<int>::max();
  float best_delay_gain = std::numeric_limits<float>::max();

  debugPrint(
      logger_, RMP, "remap", 1, "Running ABC with {} modes.", modes.size());

  for (size_t curr_mode_idx = 0; curr_mode_idx < modes.size();
       curr_mode_idx++) {
    output_blif_file_name_
        = work_dir_name_ + std::string(block_->getConstName())
          + std::to_string(curr_mode_idx) + "_crit_path_out.blif";

    opt_mode_ = modes[curr_mode_idx];

    const std::string abc_script_file
        = work_dir_name_ + std::to_string(curr_mode_idx) + "ord_abc_script.tcl";
    if (logfile_ == "")
      logfile_ = work_dir_name_ + "abc.log";

    debugPrint(logger_,
               RMP,
               "remap",
               1,
               "Writing ABC script file {}.",
               abc_script_file);

    if (writeAbcScript(abc_script_file)) {
      // call linked abc
      Abc_Start();
      Abc_Frame_t* abc_frame = Abc_FrameGetGlobalFrame();
      const std::string command = "source " + abc_script_file;
      child_proc[curr_mode_idx]
          = Cmd_CommandExecute(abc_frame, command.c_str());
      if (child_proc[curr_mode_idx]) {
        logger_->error(RMP, 26, "Error executing ABC command {}.", command);
        return;
      }
      Abc_Stop();
      // exit linked abc
      files_to_remove.emplace_back(abc_script_file);
    }
  }  // end modes

  // Inspect ABC results to choose blif with least instance count
  for (int curr_mode_idx = 0; curr_mode_idx < modes.size(); curr_mode_idx++) {
    // Skip failed ABC runs
    if (child_proc[curr_mode_idx] != 0) {
      continue;
    }

    output_blif_file_name_
        = work_dir_name_ + std::string(block_->getConstName())
          + std::to_string(curr_mode_idx) + "_crit_path_out.blif";
    const std::string abc_log_name = logfile_ + std::to_string(curr_mode_idx);

    int level_gain = 0;
    float delay = std::numeric_limits<float>::max();
    int num_instances = 0;
    bool success = readAbcLog(abc_log_name, level_gain, delay);
    if (success) {
      success
          = blif_.inspectBlif(output_blif_file_name_.c_str(), num_instances);
      logger_->report(
          "Optimized to {} instances in iteration {} with max path depth "
          "decrease of {}, delay of {}.",
          num_instances,
          curr_mode_idx,
          level_gain,
          delay);

      if (success) {
        if (is_area_mode_) {
          if (num_instances < best_inst_count) {
            best_inst_count = num_instances;
            best_blif = output_blif_file_name_;
          }
        } else {
          // Using only DELAY_4 for delay based gain since other modes not
          // showing good gains
          if (modes[curr_mode_idx] == Mode::DELAY_4) {
            best_delay_gain = delay;
            best_blif = output_blif_file_name_;
          }
        }
      }
    }
    files_to_remove.emplace_back(output_blif_file_name_);
  }

  if (best_inst_count < std::numeric_limits<int>::max()
      || best_delay_gain < std::numeric_limits<float>::max()) {
    // read back netlist
    debugPrint(logger_, RMP, "remap", 1, "Reading blif file {}.", best_blif);
    blif_.readBlif(best_blif.c_str(), block_);
    debugPrint(logger_,
               utl::RMP,
               "remap",
               1,
               "Number constants after restructure {}.",
               countConsts(block_));
  } else {
    logger_->info(
        RMP, 21, "All re-synthesis runs discarded, keeping original netlist.");
  }

  for (const auto& file_to_remove : files_to_remove) {
    if (!logger_->debugCheck(RMP, "remap", 1))
      if (std::remove(file_to_remove.c_str()) != 0) {
        logger_->error(RMP, 37, "Fail to remove file {}", file_to_remove);
      }
  }
}

void Restructure::postABC(float worst_slack)
{
  // Leave the parasitics up to date.
  resizer_->estimateWireParasitics();
}

void Restructure::getEndPoints(sta::PinSet& ends,
                               bool area_mode,
                               unsigned max_depth)
{
  auto sta_state = open_sta_->search();
  sta::VertexSet* end_points = sta_state->endpoints();
  std::size_t path_found = end_points->size();
  logger_->report("Number of paths for restructure are {}", path_found);
  for (auto& end_point : *end_points) {
    if (!is_area_mode_) {
      sta::PathRef path_ref
          = open_sta_->vertexWorstSlackPath(end_point, sta::MinMax::max());
      sta::Path* path = path_ref.path();
      sta::PathExpanded expanded(path, open_sta_);
      // Members in expanded include gate output and net so divide by 2
      logger_->report("Found path of depth {}", expanded.size() / 2);
      if (expanded.size() / 2 > max_depth) {
        ends.insert(end_point->pin());
        // Use only one end point to limit blob size for timing
        break;
      }
    } else {
      ends.insert(end_point->pin());
    }
  }

  // unconstrained end points
  if (is_area_mode_) {
    auto errors = open_sta_->checkTiming(false /*no_input_delay*/,
                                         false /*no_output_delay*/,
                                         false /*reg_multiple_clks*/,
                                         true /*reg_no_clks*/,
                                         true /*unconstrained_endpoints*/,
                                         false /*loops*/,
                                         false /*generated_clks*/);
    debugPrint(logger_, RMP, "remap", 1, "Size of errors = {}", errors.size());
    if (!errors.empty() && errors[0]->size() > 1) {
      sta::CheckError* error = errors[0];
      bool first = true;
      for (auto pinName : *error) {
        debugPrint(logger_, RMP, "remap", 1, "Unconstrained pin: {}", pinName);
        if (!first && open_sta_->getDbNetwork()->findPin(pinName)) {
          ends.insert(open_sta_->getDbNetwork()->findPin(pinName));
        }
        first = false;
      }
    }
    if (errors.size() > 1 && errors[1]->size() > 1) {
      sta::CheckError* error = errors[1];
      bool first = true;
      for (auto pinName : *error) {
        debugPrint(logger_, RMP, "remap", 1, "Unclocked pin: {}", pinName);
        if (!first && open_sta_->getDbNetwork()->findPin(pinName)) {
          ends.insert(open_sta_->getDbNetwork()->findPin(pinName));
        }
        first = false;
      }
    }
  }
  logger_->report("Found {} end points for restructure", ends.size());
}

int Restructure::countConsts(odb::dbBlock* top_block)
{
  int const_nets = 0;
  for (auto block_net : top_block->getNets()) {
    if (block_net->getSigType().isSupply())
      const_nets++;
  }

  return const_nets;
}

void Restructure::removeConstCells()
{
  if (!hicell_.size() || !locell_.size())
    return;

  odb::dbMaster* hicell_master = nullptr;
  odb::dbMTerm* hiterm = nullptr;
  odb::dbMaster* locell_master = nullptr;
  odb::dbMTerm* loterm = nullptr;

  for (auto&& lib : block_->getDb()->getLibs()) {
    hicell_master = lib->findMaster(hicell_.c_str());

    locell_master = lib->findMaster(locell_.c_str());
    if (locell_master && hicell_master)
      break;
  }
  if (!hicell_master || !locell_master)
    return;

  hiterm = hicell_master->findMTerm(hiport_.c_str());
  loterm = locell_master->findMTerm(loport_.c_str());
  if (!hiterm || !loterm)
    return;

  open_sta_->clearLogicConstants();
  open_sta_->findLogicConstants();
  std::set<odb::dbInst*> constInsts;
  int const_cnt = 1;
  for (auto inst : block_->getInsts()) {
    int outputs = 0;
    int const_outputs = 0;
    auto master = inst->getMaster();
    sta::LibertyCell* cell = open_sta_->getDbNetwork()->libertyCell(
        open_sta_->getDbNetwork()->dbToSta(master));
    if (cell->hasSequentials())
      continue;

    for (auto&& iterm : inst->getITerms()) {
      if (iterm->getSigType() == odb::dbSigType::POWER
          || iterm->getSigType() == odb::dbSigType::GROUND)
        continue;

      if (iterm->getIoType() != odb::dbIoType::OUTPUT)
        continue;
      outputs++;
      auto pin = open_sta_->getDbNetwork()->dbToSta(iterm);
      sta::LogicValue pinVal = open_sta_->simLogicValue(pin);
      if (pinVal == sta::LogicValue::one || pinVal == sta::LogicValue::zero) {
        odb::dbNet* net = iterm->getNet();
        if (net) {
          odb::dbMaster* const_master = (pinVal == sta::LogicValue::one)
                                            ? hicell_master
                                            : locell_master;
          odb::dbMTerm* const_port
              = (pinVal == sta::LogicValue::one) ? hiterm : loterm;
          std::string inst_name = "rmp_const_" + std::to_string(const_cnt);
          debugPrint(logger_,
                     RMP,
                     "remap",
                     2,
                     "Adding cell {} inst {} for {}",
                     const_master->getName(),
                     inst_name,
                     inst->getName());
          auto new_inst
              = odb::dbInst::create(block_, const_master, inst_name.c_str());
          if (new_inst) {
            iterm->disconnect();
            new_inst->getITerm(const_port)->connect(net);
          } else
            logger_->warn(RMP, 35, "Could not create instance {}.", inst_name);
        }
        const_outputs++;
        const_cnt++;
      }
    }
    if (outputs > 0 && outputs == const_outputs)
      constInsts.insert(inst);
  }
  open_sta_->clearLogicConstants();

  debugPrint(
      logger_, RMP, "remap", 2, "Removing {} instances...", constInsts.size());

  for (auto inst : constInsts)
    removeConstCell(inst);
  logger_->report("Removed {} instances with constant outputs.",
                  constInsts.size());
}

void Restructure::removeConstCell(odb::dbInst* inst)
{
  for (auto iterm : inst->getITerms())
    iterm->disconnect();
  odb::dbInst::destroy(inst);
}

bool Restructure::writeAbcScript(std::string file_name)
{
  std::ofstream script(file_name.c_str());

  if (!script.is_open()) {
    logger_->error(RMP, 20, "Cannot open file {} for writing.", file_name);
    return false;
  }

  for (const auto& lib_name : lib_file_names_) {
    // abc read_lib prints verbose by default, -v toggles to off to avoid read
    // time being printed
    std::string read_lib_str = "read_lib -v " + lib_name + "\n";
    script << read_lib_str;
  }

  script << "read_blif -n " << input_blif_file_name_ << std::endl;

  if (logger_->debugCheck(RMP, "remap", 1))
    script << "write_verilog " << input_blif_file_name_ + std::string(".v")
           << std::endl;

  writeOptCommands(script);

  script << "write_blif " << output_blif_file_name_ << std::endl;

  if (logger_->debugCheck(RMP, "remap", 1))
    script << "write_verilog " << output_blif_file_name_ + std::string(".v")
           << std::endl;

  script.close();

  return true;
}

void Restructure::writeOptCommands(std::ofstream& script)
{
  std::string choice
      = "alias choice \"fraig_store; resyn2; fraig_store; resyn2; fraig_store; "
        "fraig_restore\"";
  std::string choice2
      = "alias choice2 \"fraig_store; balance; fraig_store; resyn2; "
        "fraig_store; resyn2; fraig_store; resyn2; fraig_store; "
        "fraig_restore\"";
  script << "bdd; sop" << std::endl;

  script << "alias resyn2 \"balance; rewrite; refactor; balance; rewrite; "
            "rewrite -z; balance; refactor -z; rewrite -z; balance\""
         << std::endl;
  script << choice << std::endl;
  script << choice2 << std::endl;

  if (opt_mode_ == Mode::AREA_3)
    script << "choice2" << std::endl;  // << "scleanup" << std::endl;
  else
    script << "resyn2" << std::endl;  // << "scleanup" << std::endl;

  switch (opt_mode_) {
    case Mode::DELAY_1: {
      script << "map -D 0.01 -A 0.9 -B 0.2 -M 0 -p" << std::endl;
      script << "buffer -p -c" << std::endl;
      break;
    }
    case Mode::DELAY_2: {
      script << "choice" << std::endl;
      script << "map -D 0.01 -A 0.9 -B 0.2 -M 0 -p" << std::endl;
      script << "choice" << std::endl;
      script << "map -D 0.01" << std::endl;
      script << "buffer -p -c" << std::endl << "topo" << std::endl;
      break;
    }
    case Mode::DELAY_3: {
      script << "choice2" << std::endl;
      script << "map -D 0.01 -A 0.9 -B 0.2 -M 0 -p" << std::endl;
      script << "choice2" << std::endl;
      script << "map -D 0.01" << std::endl;
      script << "buffer -p -c" << std::endl << "topo" << std::endl;
      break;
    }
    case Mode::DELAY_4: {
      script << "choice2" << std::endl;
      script << "amap -F 20 -A 20 -C 5000 -Q 0.1 -m" << std::endl;
      script << "choice2" << std::endl;
      script << "map -D 0.01 -A 0.9 -B 0.2 -M 0 -p" << std::endl;
      script << "buffer -p -c" << std::endl;
      break;
    }
    case Mode::AREA_2:
    case Mode::AREA_3: {
      script << "choice2" << std::endl;
      script << "amap -m -Q 0.1 -F 20 -A 20 -C 5000" << std::endl;
      script << "choice2" << std::endl;
      script << "amap -m -Q 0.1 -F 20 -A 20 -C 5000" << std::endl;
      break;
    }
    case Mode::AREA_1:
    default: {
      script << "choice2" << std::endl;
      script << "amap -m -Q 0.1 -F 20 -A 20 -C 5000" << std::endl;
      break;
    }
  }
}

void Restructure::setMode(const char* mode_name)
{
  is_area_mode_ = true;

  if (!strcmp(mode_name, "timing")) {
    is_area_mode_ = false;
    opt_mode_ = Mode::DELAY_1;
  } else if (!strcmp(mode_name, "area"))
    opt_mode_ = Mode::AREA_1;
  else {
    logger_->warn(RMP, 36, "Mode {} not recognized.", mode_name);
  }
}

void Restructure::setTieHiPort(sta::LibertyPort* tieHiPort)
{
  if (tieHiPort) {
    hicell_ = tieHiPort->libertyCell()->name();
    hiport_ = tieHiPort->name();
  }
}

void Restructure::setTieLoPort(sta::LibertyPort* tieLoPort)
{
  if (tieLoPort) {
    locell_ = tieLoPort->libertyCell()->name();
    loport_ = tieLoPort->name();
  }
}

bool Restructure::readAbcLog(std::string abc_file_name,
                             int& level_gain,
                             float& final_delay)
{
  std::ifstream abc_file(abc_file_name);
  if (abc_file.bad()) {
    logger_->error(RMP, 16, "cannot open file {}", abc_file_name);
    return false;
  }
  logger_->report("Reading ABC log {}.", abc_file_name);
  std::string buf;
  const char delimiter = ' ';
  bool status = true;
  std::vector<double> level;
  std::vector<float> delay;

  // read the file line by line
  while (std::getline(abc_file, buf)) {
    // convert the line in to stream:
    std::istringstream ss(buf);
    std::vector<std::string> tokens;

    // read the line, word by word
    while (std::getline(ss, buf, delimiter))
      tokens.push_back(buf);

    if (!tokens.empty() && tokens[0] == "Error:") {
      status = false;
      logger_->warn(RMP,
                    25,
                    "ABC run failed, see log file {} for details.",
                    abc_file_name);
      break;
    }
    if (tokens.size() > 7 && tokens[tokens.size() - 3] == "lev"
        && tokens[tokens.size() - 2] == "=") {
      level.emplace_back(std::stoi(tokens[tokens.size() - 1]));
    }
    if (tokens.size() > 7) {
      std::string prev_token = "";
      for (std::string token : tokens) {
        if (prev_token == "delay" && token.at(0) == '=') {
          std::string delay_str = token;
          if (delay_str.size() > 1) {
            delay_str.erase(
                delay_str.begin());  // remove first char which is '='
            delay.emplace_back(std::stof(delay_str));
          }
          break;
        }
        prev_token = std::move(token);
      }
    }
  }

  if (level.size() > 1) {
    level_gain = level[0] - level[level.size() - 1];
  }
  if (delay.size() > 0) {
    final_delay = delay[delay.size() - 1];  // last value in file
  }
  return status;
}

/*
  Timing driven restructuring
  ---------------------------
  This is a source level integration of abc without using blif
  interface. It includes new cut generation, explicit timing
  constraint annotation and invocation of abc remapping.
*/

//
// Routines for cut generation: Restructure::generateCuts(std::vector<Cut*>);
//

class AccumulateSortedSlackEndPoints : public sta::VertexVisitor
{
 public:
  AccumulateSortedSlackEndPoints(
      sta::Network* nwk,
      std::multimap<float, sta::Vertex*>& sorted_slack_times,
      std::multimap<sta::Vertex*, float>& vertex_slack_times,
      std::set<sta::Vertex*>& end_points)
      : nwk_(nwk),
        sorted_slack_times_(sorted_slack_times),
        vertex_slack_times_(vertex_slack_times),
        end_points_(end_points)
  {
  }
  void visit(sta::Vertex* visit);
  VertexVisitor* copy() const { return nullptr; }

 private:
  sta::Network* nwk_;
  std::multimap<float, sta::Vertex*>& sorted_slack_times_;
  std::multimap<sta::Vertex*, float>& vertex_slack_times_;
  std::set<sta::Vertex*>& end_points_;
  friend class Restructure;
};
void AccumulateSortedSlackEndPoints::visit(sta::Vertex* v)
{
  sta::Sta* sta = sta::Sta::sta();

  sta::Pin* cur_pin = v->pin();
  sta::Slack max_s = sta->pinSlack(cur_pin, sta::MinMax::max());
  sta::Slack min_s = sta->pinSlack(cur_pin, sta::MinMax::min());
  sta::Slack s = (max_s == sta::INF || max_s == -sta::INF) ? min_s : max_s;

#ifdef DEBUG_RESTRUCT
  printf("Design Worst Slack time %s %f\n", delayAsString(s, sta), s);
#endif

  if (Restructure::isRegInput(nwk_, v) || Restructure::isPrimary(nwk_, v)) {
    sorted_slack_times_.insert(std::pair<float, sta::Vertex*>(s, v));
    vertex_slack_times_.insert(std::pair<sta::Vertex*, float>(v, s));
    end_points_.insert(v);
  }
}

class AccumulateSortedSlackStartPoints : public sta::VertexVisitor
{
 public:
  AccumulateSortedSlackStartPoints(std::set<sta::Vertex*>& end_points)
      : end_points_(end_points)
  {
  }
  void visit(sta::Vertex* visit);
  VertexVisitor* copy() const { return nullptr; }

 private:
  std::set<sta::Vertex*>& end_points_;
  friend class Restructure;
};
void AccumulateSortedSlackStartPoints::visit(sta::Vertex* v)
{
  end_points_.insert(v);
}

bool Restructure::isRegInput(sta::Network* nwk, sta::Vertex* v)
{
  sta::LibertyPort* port = nwk->libertyPort(v->pin());
  if (port) {
    sta::LibertyCell* cell = port->libertyCell();
    const sta::TimingArcSetSeq& seq = cell->timingArcSets(nullptr, port);
    for (auto arc_set : seq) {
      if (arc_set->role()->genericRole() == sta::TimingRole::setup())
        return true;
    }
  }
  return false;
}

bool Restructure::isRegOutput(sta::Network* nwk, sta::Vertex* v)
{
  sta::LibertyPort* port = nwk->libertyPort(v->pin());
  if (port) {
    sta::LibertyCell* cell = port->libertyCell();
    const sta::TimingArcSetSeq& seq = cell->timingArcSets(nullptr, port);
    for (auto arc_set : seq) {
      if (arc_set->role()->genericRole() == sta::TimingRole::regClkToQ())
        return true;
    }
  }
  return false;
}

bool Restructure::isPrimary(sta::Network* nwk, sta::Vertex* v)
{
  return (nwk->isTopLevelPort(v->pin()));
}

/*
  Single output cut generation
  ----------------------------
  Walk back through instance input, building cut set.
*/
void Restructure::generateWaveFrontSingleOpCutSet(sta::Network* nwk,
                                                  sta::Instance* root,
                                                  std::vector<Cut*>& cut_set)
{
  CutGen cut_generator(nwk, nullptr);
  cut_generator.GenerateInstanceWaveFrontCutSet(root, cut_set);
}

// Build cut from output pin
void Restructure::generateWaveFrontSingleOpCutSet(sta::Network* nwk,
                                                  sta::Pin* root,
                                                  std::vector<Cut*>& cut_set)
{
  if (nwk->direction(root)->isOutput()) {
    sta::Instance* cur_inst = nwk->instance(root);
    generateWaveFrontSingleOpCutSet(nwk, cur_inst, cut_set);
  }
}

//
// Multiple output cut generation
//

/*
  From a nominated pin.
  Generate Multiple output cut starting from a pin.
  Optionally generate just a single output pin.
  Walk right the way through the network to invariant
  points (don't just go back a short way using
  wavefront).
*/

void Restructure::generateUnconstrainedCut(sta::Pin* head_pin,
                                           bool single_op,
                                           std::vector<Cut*>& cut_set)
{
  sta::Graph* graph = open_sta_->ensureGraph();
  open_sta_->ensureLevelized();
  open_sta_->searchPreamble();
  int cut_id = 0;
  sta::Sta* sta = sta::Sta::sta();
  sta::Network* nwk = open_sta_->getDbNetwork();

  // generate a single output cut from a start pin, head_pin
  if (single_op) {
    // get the driver (note a root is a memory or similar)
    // regular case starting from an output pin
    sta::PinSet* drivers = nwk->drivers(head_pin);
    if (drivers) {
      sta::PinSet::Iterator drvr_iter(drivers);
      sta::Pin* driving_pin = const_cast<sta::Pin*>(drvr_iter.next());
      sta::Instance* cur_inst = nwk->instance(driving_pin);
      generateWaveFrontSingleOpCutSet(nwk, cur_inst, cut_set);
      return;
    } else {
      sta::Instance* cur_inst = nwk->instance(head_pin);
      if (nwk->isTopInstance(cur_inst)) {
        sta::Term* below_term = nwk->term(head_pin);
        if (below_term) {
          sta::Net* below_net = nwk->net(below_term);
          // get driver
          sta::NetPinIterator* fanin_pin_iter = nwk->pinIterator(below_net);
          while (fanin_pin_iter->hasNext()) {
            const sta::Pin* driving_pin = fanin_pin_iter->next();
            if (nwk->direction(driving_pin)->isOutput()) {
              cur_inst = nwk->instance(driving_pin);
              generateWaveFrontSingleOpCutSet(nwk, cur_inst, cut_set);
              return;
            }
          }
        }
      }
    }
  } else {
    // generate a multiple output cut
    // sort based on slack time: most negative first.
    std::set<sta::Vertex*> end_points;
    std::multimap<float, sta::Vertex*> sorted_slack_times;  // slack -> vertex
    std::multimap<sta::Vertex*, float> vertex_slack_times;  // vertex -> slack
    // visitor
    AccumulateSortedSlackEndPoints accumulate_sorted_slack_end_points(
        nwk, sorted_slack_times, vertex_slack_times, end_points);
    sta::Sta* sta = sta::Sta::sta();
    // accumulate the start and end points
    open_sta_->ensureGraph();
    open_sta_->search()->visitEndpoints(&accumulate_sorted_slack_end_points);
    ResetCutTemporaries();
    // harvest the leaves
    walkBackwardsToTimingEndPointsR(graph, nwk, head_pin, 0);
    pin_visited_.clear();
    // walk forward from each leaf. Harvest the volume in forward pass
    for (auto leaf_pin_int : leaves_) {
      walkForwardsToTimingEndPointsR(graph, nwk, leaf_pin_int.first, 0);
    }
    AmendCutForEscapeLeaves(nwk);
    // check if cut degenerate.
    Cut* cut = extractCut(cut_id);
    logger_->report(
        "Extracted cut {} with {} roots and {} leaves and Volume {} ",
        cut->id_,
        cut->roots_.size(),
        cut->leaves_.size(),
        cut->volume_.size());
    if (!cut || cut->roots_.size() == 0) {
      logger_->report("done with cut generation. Generated {} cuts", cut_id);
      return;
    }
    cut_set.push_back(cut);
  }
}

/*
  Generic Timing Driven Multiple output cut generation
  ---------------------------------------------------
  1. queue = Set up end points sorted by criticallity
  2. Pick most critical end point
  3. Walk back to invariant drivers. Set up cut Leaves
  4. Walk forwards to end points. Set up cut roots.
  5. Extract cut and add to cut set.
  6. Remove cut roots from queue.
  7. If queue not empty go to step 1.
  8. Return cut set.
  (This is equivalent to a blob in OpenROAD speak).
*/

void Restructure::generateUnconstrainedCuts(std::vector<Cut*>& cut_set,
                                            bool single_op)
{
  sta::Graph* graph = open_sta_->ensureGraph();
  open_sta_->ensureLevelized();
  open_sta_->searchPreamble();
  int cut_id = 0;
  std::set<sta::Vertex*> end_points;
  sta::Network* nwk = open_sta_->getDbNetwork();

  // sort based on slack time: most negative first.
  std::multimap<float, sta::Vertex*> sorted_slack_times;  // slack -> vertex
  std::multimap<sta::Vertex*, float> vertex_slack_times;  // vertex -> slack

  // visitor
  AccumulateSortedSlackEndPoints accumulate_sorted_slack_end_points(
      nwk, sorted_slack_times, vertex_slack_times, end_points);
  sta::Sta* sta = sta::Sta::sta();

  // accumulate the start and end points
  open_sta_->ensureGraph();
  open_sta_->search()->visitEndpoints(&accumulate_sorted_slack_end_points);
#ifdef DEBUG_GENERIC_RESTRUCT
  printf("Dump of end points\n");
  for (auto sst : sorted_slack_times)
    printf("Vertex %d slack time %s\n",
           graph->id(sst.second),
           delayAsString(sst.first, sta));
#endif
  AccumulateSortedSlackStartPoints accumulate_sorted_slack_start_points(
      end_points);
  open_sta_->ensureGraph();
  open_sta_->search()->visitStartpoints(&accumulate_sorted_slack_start_points);

#ifdef DEBUG_GENERIC_RESTRUCT
  printf("Starting cut construction for end point list of size %d\n",
         vertex_slack_times.size());
#endif

  while (vertex_slack_times.size() != 0) {
    logger_->report("Worst slack {}  Best slack {} Size of end point list {}",
                    delayAsString((*sorted_slack_times.begin()).first, sta),
                    delayAsString((*sorted_slack_times.end()).first, sta),
                    vertex_slack_times.size());
    // map stores smallest item first, so this is by default the least slack
    sta::Vertex* head_vertex = (*sorted_slack_times.begin()).second;
    sta::Pin* cur_pin = head_vertex->pin();
#ifdef DEBUG_GENERIC_RESTRUCT
    printf("Generating cut from pin %s on instance %s\n",
           nwk->pathName(cur_pin),
           nwk->name(nwk->instance(cur_pin)));
#endif
    if (single_op) {
      // get the driver (note a root is a memory or similar)
      // regular case
      sta::PinSet* drivers = nwk->drivers(cur_pin);
      if (drivers) {
        sta::PinSet::Iterator drvr_iter(drivers);
        sta::Pin* driving_pin = const_cast<sta::Pin*>(drvr_iter.next());
        sta::Instance* cur_inst = nwk->instance(driving_pin);
        generateWaveFrontSingleOpCutSet(nwk, cur_inst, cut_set);
        return;
      } else {
        sta::Instance* cur_inst = nwk->instance(cur_pin);
        if (nwk->isTopInstance(cur_inst)) {
          sta::Term* below_term = nwk->term(cur_pin);
          if (below_term) {
            sta::Net* below_net = nwk->net(below_term);
            // get driver
            sta::NetPinIterator* fanin_pin_iter = nwk->pinIterator(below_net);
            while (fanin_pin_iter->hasNext()) {
              const sta::Pin* driving_pin = fanin_pin_iter->next();
              if (nwk->direction(driving_pin)->isOutput()) {
                cur_inst = nwk->instance(driving_pin);
                generateWaveFrontSingleOpCutSet(nwk, cur_inst, cut_set);
                return;
              }
            }
          }
        }
      }
    } else {
      ResetCutTemporaries();
      // harvest the leaves
      walkBackwardsToTimingEndPointsR(graph, nwk, cur_pin, 0);
      pin_visited_.clear();
      // walk forward from each leaf. Harvest the volume in forward pass
      for (auto leaf_pin_int : leaves_) {
        walkForwardsToTimingEndPointsR(graph, nwk, leaf_pin_int.first, 0);
      }
      AmendCutForEscapeLeaves(nwk);

      // check if cut degenerate.
      Cut* cut = extractCut(cut_id);

      logger_->report(
          "Extracted cut {} with {} roots and {} leaves and Volume {} ",
          cut->id_,
          cut->roots_.size(),
          cut->leaves_.size(),
          cut->volume_.size());

      if (!cut || cut->roots_.size() == 0) {
        logger_->report("done with cut generation. Generated {} cuts", cut_id);

        return;
      }
      cut_set.push_back(cut);

      //      printf("**Cut extracted\n");
      //      cut -> Print(stdout,nwk);

      // remove visited vertices from stash of sorted & vertex slack times
      for (auto cut_root : cut->roots_) {
        sta::Vertex* root_vertex = graph->vertex(nwk->vertexId(graph->id(), cut_root));
        updateVertexList(root_vertex, sorted_slack_times, vertex_slack_times);
      }
      cut_id++;
      // Check cut meets all required rules
      cut->Check(nwk);
      // just consider one cut for now.
      return;
    }
  }
}

void Restructure::updateVertexList(
    sta::Vertex* root,
    std::multimap<float, sta::Vertex*>& float2vertex,
    std::multimap<sta::Vertex*, float>& vertex2float)
{
  std::map<sta::Vertex*, float>::iterator vertex2float_it
      = vertex2float.find(root);

  if (vertex2float_it != vertex2float.end()) {
    float timing_range = (*vertex2float_it).second;
    vertex2float.erase(vertex2float_it);

    int count = 0;
    // many vertices may have same slack, so go through
    // until we hit our vertex
    for (std::multimap<float, sta::Vertex*>::iterator it = float2vertex.begin();
         it != float2vertex.end();
         it++) {
      if ((*it).second == root) {
        float2vertex.erase(it);
        return;
      }
    }
  }
}

Cut* Restructure::extractCut(int cut_id)
{
  Cut* ret = new Cut();
  ret->leaves_.resize(leaves_.size());
  for (auto i : leaves_)
    ret->leaves_[i.second] = i.first;
  ret->roots_.resize(roots_.size());
  for (auto i : roots_)
    ret->roots_[i.second] = i.first;
  for (auto i : cut_volume_)
    ret->volume_.push_back(i);
  ret->id_ = cut_id;
  return ret;
}

void Restructure::walkBackwardsToTimingEndPointsR(sta::Graph* graph,
                                                  sta::Network* nwk,
                                                  sta::Pin* start_pin,
                                                  int depth)
{
#ifdef DEBUG_GENERIC_RESTRUCT
  printf("Walking back to leaf points from pin %s depth %d\n",
         nwk->pathName(start_pin),
         depth);
#endif
  if (start_pin && pin_visited_.find(start_pin) == pin_visited_.end()) {
    pin_visited_.insert(start_pin);
    sta::Port* start_port = nwk->port(start_pin);
    sta::Vertex* vertex = graph->vertex(nwk->vertexId(graph->id(), start_pin));
    sta::Instance* cur_inst = nwk->instance(start_pin);

    // hit an invariant end point
    // include q outputs of registers as invariants
    if (isRegOutput(nwk, vertex)
        || ((end_points_.find(vertex) != end_points_.end()) && depth != 0)) {
      leaves_[start_pin] = leaves_.size();
#ifdef DEBUG_GENERIC_RESTRUCT
      printf("Inserted reg op leaf: %s\n", nwk->pathName(start_pin), depth);
#endif

      return;
    }
    // hit an invariant point: the output of a primary port (top level port)
    else if (nwk->direction(start_port) == sta::PortDirection::input()
             && isPrimary(nwk, vertex) && depth != 0) {
      leaves_[start_pin] = leaves_.size();
#ifdef DEBUG_GENERIC_RESTRUCT
      printf("Inserted primary ip leaf: %s\n", nwk->pathName(start_pin), depth);
#endif
      return;
    }

    // a primary output at depth 0 push down into hierarchy
    // (special start case)
    else if (nwk->direction(start_port) == sta::PortDirection::output()
             && isPrimary(nwk, vertex) && depth == 0) {
      sta::Instance* cur_inst = nwk->instance(start_pin);
      sta::Term* below_term = nwk->term(start_pin);
      if (below_term) {
        sta::Net* below_net = nwk->net(below_term);
        // get driver
        sta::NetPinIterator* fanin_pin_iter = nwk->pinIterator(below_net);
        while (fanin_pin_iter->hasNext()) {
          const sta::Pin* driving_pin = fanin_pin_iter->next();
          if (nwk->direction(driving_pin) == sta::PortDirection::output()) {
            walkBackwardsToTimingEndPointsR(
                graph, nwk, const_cast<sta::Pin*>(driving_pin), depth + 1);
          }
        }
      }
      return;
    }
    // an instance input
    else if (nwk->direction(start_port) == sta::PortDirection::input()
             && !isPrimary(nwk, vertex)) {
      // traverse back
      sta::PinSet* drivers = nwk->drivers(start_pin);
      if (drivers) {
        sta::PinSet::Iterator drvr_iter(drivers);
        sta::Pin* driving_pin = const_cast<sta::Pin*>(drvr_iter.next());
        walkBackwardsToTimingEndPointsR(graph, nwk, driving_pin, depth + 1);
      }
      return;
    }
    //
    // an instance output
    // go through all the other instance inputs too
    //
    else if (nwk->direction(start_port) == sta::PortDirection::output()
             && !isPrimary(nwk, vertex)) {
      // push up to parent and walk back through input pins
      sta::Instance* cur_inst = nwk->instance(start_pin);
      // insert in volume knowing this is not an end point
      // now go check out the inputs and start backward traversing
      sta::InstancePinIterator* pi = nwk->pinIterator(cur_inst);
      while (pi->hasNext()) {
        sta::Pin* cur_pin = pi->next();
        if (nwk->direction(cur_pin) == sta::PortDirection::input()) {
          // get the drivers
          // walk through
          // traverse back
          sta::PinSet* drivers = nwk->drivers(cur_pin);
          if (drivers) {
            sta::PinSet::Iterator drvr_iter(drivers);
            sta::Pin* driving_pin = const_cast<sta::Pin*>(drvr_iter.next());
            walkBackwardsToTimingEndPointsR(graph, nwk, driving_pin, depth + 1);
          }
        }
      }
    }
  }
}

void Restructure::walkForwardsToTimingEndPointsR(sta::Graph* graph,
                                                 sta::Network* nwk,
                                                 sta::Pin* start_pin,
                                                 int depth)
{
  static int debug;
  debug++;
  if (start_pin && pin_visited_.find(start_pin) == pin_visited_.end()) {
    sta::Instance* cur_inst = nwk->instance(start_pin);

    pin_visited_.insert(start_pin);

    sta::Port* start_port = nwk->port(start_pin);
    sta::Vertex* vertex = graph->vertex(nwk->vertexId(graph->id() , start_pin));

#ifdef DEBUG_GENERIC_RESTRUCT
    printf(
        "D %d Walking forwards from pin %s\n", debug, nwk->pathName(start_pin));
#endif
    // hit an end point. we are done
    if ((end_points_.find(vertex) != end_points_.end()) && depth != 0) {
      // avoid make clock and reset lines points for resynthesis
      if (!((strstr(nwk->pathName(start_pin), "CK"))
            || (strstr(nwk->pathName(start_pin), "RN"))
            || (strstr(nwk->pathName(start_pin), "QN"))
            || (strstr(nwk->pathName(start_pin), "SN"))
            || (strstr(nwk->pathName(start_pin), "Q")))) {
        if (roots_.find(start_pin) == roots_.end())
          roots_[start_pin] = roots_.size();
      }
      return;
    }

    // primary input
    if (nwk->direction(start_port) == sta::PortDirection::input()
        && isPrimary(nwk, vertex)) {
      // traverse forwards
      sta::PinConnectedPinIterator* connected_pin_iter
          = nwk->connectedPinIterator(start_pin);
      while (connected_pin_iter->hasNext()) {
        sta::Pin* connected_pin
            = const_cast<sta::Pin*>(connected_pin_iter->next());
        walkForwardsToTimingEndPointsR(graph, nwk, connected_pin, depth + 1);
      }
    }

    // output of an instance
    else if (nwk->direction(start_port) == sta::PortDirection::output()) {
      sta::PinConnectedPinIterator* connected_pin_iter
          = nwk->connectedPinIterator(start_pin);
      sta::Instance* cur_inst = nwk->instance(start_pin);
      if (depth != 0)
        cut_volume_.insert(cur_inst);
      while (connected_pin_iter->hasNext()) {
        sta::Pin* connected_pin
            = const_cast<sta::Pin*>(connected_pin_iter->next());
        walkForwardsToTimingEndPointsR(graph, nwk, connected_pin, depth + 1);
      }
    }

    // an instance input. Walk through to the output. Then explore fanout
    else if (nwk->direction(start_port) == sta::PortDirection::input()
             && !isPrimary(nwk, vertex)) {
      sta::Instance* cur_inst = nwk->instance(start_pin);
      if (depth != 0)
        cut_volume_.insert(cur_inst);

      sta::InstancePinIterator* pi = nwk->pinIterator(cur_inst);
      while (pi->hasNext()) {
        sta::Pin* cur_pin = pi->next();
        if (nwk->direction(nwk->port(cur_pin))
            == sta::PortDirection::output()) {
          // get the fanout of the pin
          printf("**Walking forwards from  output pin %s\n",
                 nwk->name(cur_pin));

          // code to handle terms, to allow hierarchy traversal
          sta::Net* cur_net = nwk->net(cur_pin);
          if (cur_net) {
            // code to handle terms, to allow hierarchy traversal
            sta::NetTermIterator* fanout_term_iter = nwk->termIterator(cur_net);
            while (fanout_term_iter->hasNext()) {
              const sta::Term* fanout_term = fanout_term_iter->next();
              sta::Pin* fanout_pin = nwk->pin(fanout_term);
              sta::Vertex* vertex = graph->vertex(nwk->vertexId(graph->id(), fanout_pin));
              if ((nwk->direction(fanout_pin) == sta::PortDirection::output())
                  && isPrimary(nwk, vertex)) {
                printf("**Term: Hit primary output %s\n",
                       nwk->name(fanout_pin));
                roots_[fanout_pin] = roots_.size();
              }
            }

            sta::NetPinIterator* fanout_pin_iter = nwk->pinIterator(cur_net);
            while (fanout_pin_iter->hasNext()) {
              const sta::Pin* fanout_pin = fanout_pin_iter->next();
              if (fanout_pin != cur_pin) {
                sta::Vertex* vertex = graph->vertex(nwk->vertexId(graph->id(), fanout_pin));
                if (isPrimary(nwk, vertex))
                  printf("**Hit primary output pin %s\n",
                         nwk->name(fanout_pin));

                if (isPrimary(nwk, vertex) || isRegInput(nwk, vertex)) {
                  if ((strstr(nwk->pathName(fanout_pin), "CK"))
                      || (strstr(nwk->pathName(fanout_pin), "RN"))
                      || (strstr(nwk->pathName(fanout_pin), "QN"))
                      || (strstr(nwk->pathName(fanout_pin), "SN"))
                      || (strstr(nwk->pathName(fanout_pin), "Q")))
                    continue;
                  else {
                    if (roots_.find(const_cast<sta::Pin*>(fanout_pin))
                        == roots_.end())
                      roots_[const_cast<sta::Pin*>(fanout_pin)] = roots_.size();
                  }
                } else {
                  sta::Port* next_port = nwk->port(fanout_pin);
                  sta::Vertex* next_vertex
                      = graph->vertex(nwk->vertexId(graph->id(), fanout_pin));
                  if (nwk->direction(fanout_pin) == sta::PortDirection::output()
                      && isPrimary(nwk, next_vertex)) {
                    printf("Hit primary output root !\n");
                    roots_[const_cast<sta::Pin*>(fanout_pin)] = roots_.size();
                  } else if (nwk->direction(fanout_pin)
                             == sta::PortDirection::input()) {
                    walkForwardsToTimingEndPointsR(
                        graph,
                        nwk,
                        const_cast<sta::Pin*>(fanout_pin),
                        depth + 1);
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

//
// Routines for building abc structures from a cut
// Class Cut2Ntk
//
void Cut2Ntk::CleanUpNetNets()
{
  bool done_something = true;
  while (done_something) {
    done_something = false;
    std::vector<Abc_Obj_t*> orphans;
    for (auto pin_net : net_table_abc2sta_) {
      Abc_Obj_t* cur_net = pin_net.first;
      if (!cur_net)
        continue;
      if (Abc_ObjFaninVec(cur_net)->nSize == 0)
        continue;
      Abc_Obj_t* fanin_obj = Abc_ObjFanin0(cur_net);
      if (net_table_abc2sta_.find(fanin_obj) != net_table_abc2sta_.end()) {
        Abc_Obj_t* fanin_fanin_obj = Abc_ObjFanin0(fanin_obj);
        // delete the cur net
        // fanin_fanin_obj -> fanin_obj -> cur_net -> {FO}
        //-->
        // fanin_fanin_obj -> fanin_obj -> {FO}
        //
        Abc_ObjDeleteFanin(cur_net, fanin_obj);
        Abc_ObjTransferFanout(cur_net, fanin_obj);
        // at this point cur_net is totally disconnected
        orphans.push_back(cur_net);
        done_something = true;
      }
    }
    // kill the orphans
    for (auto n : orphans) {
      const sta::Pin* cur_pin = net_table_abc2sta_[n];
      net_table_abc2sta_.erase(n);
      net_table_sta2abc_.erase(cur_pin);
      Abc_NtkDeleteObj(n);
    }
  }
}

void Cut::Print(FILE* fp, sta::Network* nwk)
{
  fprintf(fp, "Cut %d\n", id_);
  fprintf(fp, "---\n");
  fprintf(fp, "Roots:\n");
  for (auto r : roots_)
    fprintf(fp,
            "Root %s (%s)\n",
            nwk->pathName(r),
            nwk->direction(r)->isInput() ? "IP" : "OP");
  fprintf(fp, "Leaves:\n");
  for (auto l : leaves_) {
    sta::Net* net = nwk->net(l);
    if (net == nullptr) {
      sta::Pin* unconst_pin = const_cast<sta::Pin*>(l);
      sta::Term* term = reinterpret_cast<sta::Term*>(unconst_pin);
      net = nwk->net(term);
    }
    fprintf(fp,
            "Leaf %s (%s) net %s\n",
            nwk->pathName(l),
            nwk->direction(l)->isInput() ? "IP" : "OP",
            net ? nwk->name(net) : "null");

    std::set<const sta::Pin*> visited;
    visited.insert(l);
    sta::PinConnectedPinIterator* connected_pin_iter
        = nwk->connectedPinIterator(l);
    while (connected_pin_iter->hasNext()) {
      sta::Pin* connected_pin
          = const_cast<sta::Pin*>(connected_pin_iter->next());
      if (visited.find(connected_pin) == visited.end()) {
        visited.insert(connected_pin);
        fprintf(fp, " -> %s \n", nwk->name(connected_pin));
      }
    }
  }

  fprintf(fp, "Volume:\n");
  for (auto v : volume_) {
    fprintf(
        fp, "V: %s (cell; %s)\n", nwk->pathName(v), nwk->name(nwk->cell(v)));
    sta::InstancePinIterator* ip = nwk->pinIterator(v);
    while (ip->hasNext()) {
      std::set<const sta::Pin*> visited;
      const sta::Pin* cur_pin = ip->next();
      if (nwk->direction(cur_pin)->isInput()) {
        visited.insert(cur_pin);
        sta::Net* cur_net = nwk->net(cur_pin);
        sta::PinSet* drivers = nwk->drivers(cur_net);
        if (drivers) {
          sta::PinSet::Iterator drvr_iter(drivers);
          sta::Pin* driving_pin = const_cast<sta::Pin*>(drvr_iter.next());
          fprintf(fp,
                  "Ip %s (net %s) <- %s\n",
                  nwk->name(cur_pin),
                  cur_net ? nwk->name(cur_net) : "empty",
                  nwk->name(driving_pin));
        }
      } else {
        sta::Net* cur_net = nwk->net(cur_pin);
        sta::PinConnectedPinIterator* connected_pin_iter
            = nwk->connectedPinIterator(cur_pin);
        fprintf(fp,
                "Op %s (net %s) ",
                nwk->name(cur_pin),
                cur_net ? nwk->name(cur_net) : "none");
        visited.insert(cur_pin);
        while (connected_pin_iter->hasNext()) {
          sta::Pin* connected_pin
              = const_cast<sta::Pin*>(connected_pin_iter->next());
          if (visited.find(connected_pin) == visited.end()) {
            visited.insert(connected_pin);
            fprintf(fp, " -> %s ", nwk->name(connected_pin));
          }
        }
      }
    }
    fprintf(fp, "\n");
  }
  fprintf(fp, "---\n");
}

void Cut::PrintVerilog(FILE* fp, sta::Network* nwk)
{
  std::set<std::string> internal_wire_names;
  std::set<std::string> ip_names;
  std::set<std::string> op_names;
  std::map<std::string, std::string> leaf_driver_names;
  std::map<std::string, std::string> root_driver_names;

  //  fprintf(fp,"//Verilog model for cut\n");
  fprintf(fp, "module cut_%d (\n", id_);
  //  fprintf(fp,"//outputs (roots)\n");
  for (auto r : roots_) {
    fprintf(fp, "\\%s %s \n", nwk->name(r), leaves_.size() > 0 ? "," : "");
    op_names.insert(nwk->name(r));
  }
  //  fprintf(fp,"//inputs (leaves)\n");
  int count = 1;
  for (auto l : leaves_) {
    fprintf(fp, "\\%s %s\n", nwk->name(l), (count < leaves_.size()) ? "," : "");
    ip_names.insert(nwk->name(l));
    count++;
  }
  fprintf(fp, ");\n");

  for (auto r : roots_) {
    fprintf(fp, "output \\%s ;\n", nwk->name(r));
    op_names.insert(nwk->name(r));
  }
  //  fprintf(fp,"//inputs (leaves)\n");
  for (auto l : leaves_) {
    fprintf(fp, "input \\%s ;\n", nwk->name(l));
    ip_names.insert(nwk->name(l));
  }

  bool is_internal_net = false;
  for (auto v : volume_) {
    sta::InstancePinIterator* ip = nwk->pinIterator(v);
    while (ip->hasNext()) {
      std::set<const sta::Pin*> visited;
      const sta::Pin* cur_pin = ip->next();
      sta::Net* cur_net = nwk->net(cur_pin);

      is_internal_net = true;
      if (cur_net) {
        // get driver
        sta::PinSet* drivers = nwk->drivers(cur_pin);
        sta::PinSet::Iterator drvr_iter(drivers);
        std::string net_name = nwk->name(cur_net);
        if (drivers) {
          sta::Pin* driving_pin = const_cast<sta::Pin*>(drvr_iter.next());
          if (ip_names.find(nwk->name(driving_pin)) != ip_names.end()) {
            leaf_driver_names[net_name] = nwk->name(driving_pin);
            is_internal_net = false;
          }
        }
        if (op_names.find(nwk->name(cur_pin)) != op_names.end()) {
          root_driver_names[nwk->name(cur_pin)] = nwk->name(cur_net);
          is_internal_net = false;
        }

        if (is_internal_net && (nwk->direction(cur_pin)->isOutput())) {
          internal_wire_names.insert(nwk->name(cur_net));
        }
      }
    }
  }

  //  fprintf(fp,"//wires from leaf inputs\n");
  for (auto n : leaf_driver_names) {
    if (n.first == n.second && ip_names.find(n.first) != ip_names.end())
      continue;

    fprintf(fp, "wire \\%s ; \n", n.first.c_str());
    fprintf(fp, "assign \\%s = \\%s ; \n", n.first.c_str(), n.second.c_str());
  }
  //  fprintf(fp,"//wires driving root outputs\n");
  for (auto n : root_driver_names) {
    fprintf(fp, "wire \\%s ; \n", n.second.c_str());
    fprintf(fp, "assign \\%s = \\%s ; \n", n.first.c_str(), n.second.c_str());
  }

  //  fprintf(fp,"//internal wires from leaf cell outputs\n");
  for (auto n : internal_wire_names) {
    fprintf(fp, "wire \\%s ;\n", n.c_str());
  }

  //  fprintf(fp,"//Volume:\n");
  //  fprintf(fp,"//Note wires are implicit except for port connections\n");
  for (auto v : volume_) {
    sta::Cell* cur_cell = nwk->cell(v);
    fprintf(fp, "%s \\%s (", nwk->name(cur_cell), nwk->name(v));
    sta::InstancePinIterator* ipi = nwk->pinIterator(v);
    int pin_count = nwk->pinCount(v);
    while (ipi->hasNext()) {
      const sta::Pin* cur_pin = ipi->next();
      sta::Net* cur_net = nwk->net(cur_pin);
      fprintf(fp,
              " .%s( \\%s ) %s ",
              nwk->name(nwk->port(cur_pin)),
              cur_net ? nwk->name(cur_net) : nwk->name(cur_pin),
              pin_count != 1 ? "," : "");
      pin_count--;
    }
    fprintf(fp, ");\n");
  }
  fprintf(fp, "endmodule ");
}

//
// Check axioms for cut
//
// 1. Same pin cannot be a leaf and a root.
// 2. Leaf and Root instances cannot be in volume.
//
bool Cut::Check(sta::Network* nwk)
{
  bool ret = true;
  std::set<const sta::Pin*> root_leaves;
  std::set<const sta::Pin*> leaves;
  std::set<const sta::Pin*> roots;
  std::set<sta::Instance*> volume;
  for (auto i : volume_) {
    volume.insert(i);
  }
  for (auto r : roots_) {
    roots.insert(r);
    root_leaves.insert(r);
  }
  for (auto l : leaves_) {
    leaves.insert(l);
    if (root_leaves.find(l) != root_leaves.end()) {
      printf("Error same pin (%s)  cannot be a leaf and root\n",
             nwk->pathName(l));
      ret = false;
    } else
      root_leaves.insert(l);
  }
  for (auto i : volume_) {
    sta::InstancePinIterator* pin_it = nwk->pinIterator(i);
    while (pin_it->hasNext()) {
      sta::Pin* cur_pin = pin_it->next();
      if (roots.find(cur_pin) != roots.end()
          && !(nwk->direction(cur_pin)->isOutput())) {
        printf(
            "Error: Internal volume instance pin %s cannot be in  root set\n",
            nwk->pathName(cur_pin));
        ret = false;
      } else if (leaves.find(cur_pin) != leaves.end()) {
        printf("Error: Internal volume instance pin %s cannot be in leaf set\n",
               nwk->pathName(cur_pin));
        ret = false;
      }

      // Check that every sink is either a root or something in the volume

      // check that every root is driven by something.

      // Check that every driver is either driven by something in the volume
      // or a leaf
      if (nwk->direction(cur_pin)->isInput()) {
        sta::PinSet* drivers = nwk->drivers(cur_pin);
        sta::PinSet::Iterator drvr_iter(drivers);
        if (drivers) {
          sta::Pin* driving_pin = const_cast<sta::Pin*>(drvr_iter.next());
          sta::Instance* driving_instance = nwk->instance(driving_pin);
          // leaf or volume...
          if (!(leaves.find(driving_pin) != leaves.end()
                || volume.find(driving_instance) != volume.end())) {
            printf(
                "Error: every driving pin (eg %s) should be a leaf or volume "
                "instance pin (Escape node bug)\n",
                nwk->pathName(driving_pin));
            ret = false;
          }
        } else {
          printf("Warning No driver found for pin %s\n",
                 nwk->pathName(cur_pin));
          ret = false;
        }
      }
    }
  }
  assert(ret);
  return ret;
}

void Restructure::AmendCutForEscapeLeaves(sta::Network* nwk)
{
  std::set<sta::Pin*> cut_leaves;
  std::set<sta::Instance*> cut_volume;
  for (auto i : cut_volume_) {
    cut_volume.insert(i);
  }
  for (auto l : leaves_) {
    cut_leaves.insert(l.first);
  }
  for (auto i : cut_volume_) {
    sta::InstancePinIterator* pin_it = nwk->pinIterator(i);
    while (pin_it->hasNext()) {
      sta::Pin* cur_pin = pin_it->next();
      // Check that every driver is either driven by something in the volume
      // or a leaf
      if (nwk->direction(cur_pin)->isInput()) {
        sta::PinSet* drivers = nwk->drivers(cur_pin);
        sta::PinSet::Iterator drvr_iter(drivers);
        if (drivers) {
          sta::Pin* driving_pin = const_cast<sta::Pin*>(drvr_iter.next());
          sta::Instance* driving_instance = nwk->instance(driving_pin);
          // leaf or volume...
          if (!(cut_leaves.find(driving_pin) != cut_leaves.end()
                || cut_volume.find(driving_instance) != cut_volume.end())) {
            leaves_[driving_pin] = leaves_.size();
            cut_leaves.insert(driving_pin);
          }
        }
      }
    }
  }
}

// Axiom: we require the cut to pass the cut check
Abc_Ntk_t* Cut2Ntk::BuildSopLogicNetlist()
{
  // Now build the abc view of the cut
  abc_nwk_ = (Abc_NtkAlloc(ABC_NTK_NETLIST, ABC_FUNC_SOP, 1));
  Abc_NtkSetName(abc_nwk_, Extra_UtilStrsav(cut_name_.c_str()));

  // create the Primary i/o corresponding to the terminals of the cut

  // First make the primary outputs.
  // Note we make a net for each primary output.
  // This will eventually lead to a net -> net connection, but that's ok !
  //

  for (auto po : cut_->roots_) {
    Abc_Obj_t* abc_po = Abc_NtkCreatePo(abc_nwk_);
    po_table_[po] = abc_po;
    Abc_ObjAssignName(abc_po, (char*) (sta_nwk_->pathName(po)), NULL);
    Abc_Obj_t* abc_op_net = Abc_NtkCreateNet(abc_nwk_);
    Abc_ObjAssignName(abc_op_net, (char*) (sta_nwk_->pathName(po)), NULL);
    Abc_ObjAddFanin(abc_po, abc_op_net);
    // Note that if we have something driven within the cut
    // by a po, then the driving net will be found in this table.
    net_table_sta2abc_[po] = abc_op_net;
    net_table_abc2sta_[abc_op_net] = po;
  }

  // create the nets for the primary inputs.
  for (auto pi : cut_->leaves_) {
    Abc_Obj_t* abc_pi = Abc_NtkCreatePi(abc_nwk_);
    Abc_ObjAssignName(abc_pi, (char*) (sta_nwk_->pathName(pi)), NULL);
    pi_table_[pi] = abc_pi;
    Abc_Obj_t* abc_ip_net = Abc_NtkCreateNet(abc_nwk_);

    Abc_ObjAssignName(abc_ip_net, (char*) (sta_nwk_->pathName(pi)), NULL);
    Abc_ObjAddFanin(abc_ip_net, abc_pi);
    net_table_sta2abc_[pi] = abc_ip_net;  // input pin to abc net
    net_table_abc2sta_[abc_ip_net] = pi;
  }

  // create the instances for each gate in the cut
  // while doing this make the gate input nets and hook them up.
  // Check that we have not already got a net on the driver
  // during, if so we reuse it. So this incrementally builds up
  // the internal nets (independent of order of gate traversal).

  for (auto cur_inst : cut_->volume_) {
    Abc_Obj_t* cur_gate = CreateGate(cur_inst);
  }

  //
  // Sanity checking..
  // Go through each gate and make sure its
  // inputs are driven.
  // At this point we expect everything to have a driver
  //

  for (auto cur_inst : cut_->volume_) {
    sta::Cell* cell = sta_nwk_->cell(cur_inst);
    sta::CellPortIterator* cell_iter = sta_nwk_->portIterator(cell);
    while (cell_iter->hasNext()) {
      sta::Port* cell_port = cell_iter->next();
      const sta::Pin* cur_pin
          = sta_nwk_->findPin(cur_inst, sta_nwk_->name(cell_port));
      if (sta_nwk_->direction(cur_pin)->isInput()) {
        const sta::Pin* ip_pin = cur_pin;
        sta::PinSet* drvrs = sta_nwk_->drivers(ip_pin);
        if (drvrs) {
          sta::PinSet::Iterator drvr_iter(drvrs);
          const sta::Pin* driving_pin = drvr_iter.next();
          Abc_Obj_t* driver = net_table_sta2abc_[driving_pin];
          // We rationally expect a driver for every input for every gate.
          if (!driver) {
            logger_->warn(RMP,
                          100,
                          "Cannot find net for driving pin {} for pin {} on "
                          "instance {}. \n",
                          sta_nwk_->pathName(driving_pin),
                          sta_nwk_->pathName(ip_pin),
                          sta_nwk_->pathName(cur_inst));
          }
        }
      }
    }
  }

  // Wire Up all the primary outputs (roots);
  // We may have wired up the root when creating the gates
  // in which case the abc object for the root will have
  // a fanin count > 0. And that is ok !
  //
  for (auto root_pin : cut_->roots_) {
    Abc_Obj_t* abc_sink_net = net_table_sta2abc_[root_pin];
    // make sure not already wired up (in case of PI -> PO,
    // we already have a net).
    if (Abc_ObjFaninNum(abc_sink_net) == 0) {
      sta::PinSet* drvrs = sta_nwk_->drivers(root_pin);
      if (drvrs) {
        sta::PinSet::Iterator drvr_iter(drvrs);
        const sta::Pin* driving_pin = drvr_iter.next();
        Abc_Obj_t* abc_driver = net_table_sta2abc_[driving_pin];
        assert(abc_driver);
        // note this can make a net -> net connection
        // we clean this up later.
        Abc_ObjAddFanin(abc_sink_net /* thing driven */,
                        abc_driver /* fanin */);
      }
    }
  }

  //
  // remove the net -> net connections
  // These are created from our over zealous net creation scheme
  //
  CleanUpNetNets();

  Abc_NtkFinalizeRead(abc_nwk_);
  Abc_Frame_t* gf = Abc_FrameGetGlobalFrame();
  gf->pNtkCur = abc_nwk_;
  // Check the network using the abc network checker
  assert(Abc_NtkCheck(abc_nwk_));
  // code to inspect the netlist
  Abc_NtkSetName(abc_nwk_, Extra_UtilStrsav(cut_name_.c_str()));
  // SopNwkCheck(abc_nwk_);
  Abc_Ntk_t* ret = Abc_NtkDup(abc_nwk_);
  Abc_NtkTrasferNames(abc_nwk_, ret);
  return ret;
}

Abc_Obj_t* Cut2Ntk::CreateGate(const sta::Instance* cur_inst)
{
  Abc_Obj_t* ret = nullptr;
  static int debug;
  debug++;

  sta::Cell* cur_cell = sta_nwk_->cell(cur_inst);

  // port name to abc input net (for this gate),
  // Port name order invariant.
  // we keep the nets on the driving pins

  std::map<std::string, Abc_Obj_t*> ip_nets;

  // Get the input nets in port order
  sta::CellPortIterator* cell_port_iter = sta_nwk_->portIterator(cur_cell);
  while (cell_port_iter->hasNext()) {
    const sta::Port* cell_port = cell_port_iter->next();
    const std::string port_name = sta_nwk_->name(cell_port);
    // Note that we fetch the pin for the port
    // We are keeping the port <-> pin correlation here.
    // no weird indexing, use name.
    const sta::Pin* cur_pin
        = sta_nwk_->findPin(cur_inst, sta_nwk_->name(cell_port));
    if (cur_pin) {
      if (sta_nwk_->direction(cur_pin)->isInput()) {
        // Check, have we already created a net for this pin driver?
        // If so use it, else create a new one
        sta::PinSet* drvrs = sta_nwk_->drivers(cur_pin);
        if (drvrs) {
          sta::PinSet::Iterator drvr_iter(drvrs);
          const sta::Pin* driving_pin = drvr_iter.next();
          // we stash the net on the driving pin
          // if we have the net use it.
          if (net_table_sta2abc_.find(driving_pin)
              != net_table_sta2abc_.end()) {
            Abc_Obj_t* driver = net_table_sta2abc_[driving_pin];
            ip_nets[port_name] = driver;  // ip nets indexed by port name
          } else {
            // else make it
            Abc_Obj_t* ip_net = Abc_NtkCreateNet(abc_nwk_);
            Abc_ObjAssignName(
                ip_net, (char*) (sta_nwk_->pathName(cur_pin)), NULL);
            ip_nets[port_name] = ip_net;
            // note we keep only driving pin in table
            net_table_sta2abc_[driving_pin] = ip_net;
            net_table_abc2sta_[ip_net] = driving_pin;
          }
        }
      }
    }
  }

  // now create the gate and hook it up
  cell_port_iter = sta_nwk_->portIterator(cur_cell);
  while (cell_port_iter->hasNext()) {
    const sta::Port* cell_port = cell_port_iter->next();
    const sta::Pin* cur_pin
        = sta_nwk_->findPin(cur_inst, sta_nwk_->name(cell_port));
    if (sta_nwk_->direction(cur_pin)->isOutput()) {
      const sta::Pin* op_pin = cur_pin;

      sta::LibertyPort* lp = sta_nwk_->libertyPort(op_pin);
      std::map<int, std::string> ix2portname;
      if (lp && lp->function()) {
        // Get the kit representation
        int var_count = 0;
        /*word32*/ unsigned* kit_tables;
        FuncExpr2Kit(lp->function(), var_count, kit_tables, ix2portname);
        // for now we can only handle upto 5 vars.
        assert(var_count <= 5);

        unsigned num_bits_to_set = (0x01 << var_count);
        unsigned mask = 0x00;
        for (unsigned j = 0; j < num_bits_to_set; j++)
          mask = mask | (0x1 << j);
        unsigned kit_result = *kit_tables & mask;
        //        printf("Gate %s Function %s Truth table over %u vars 0x%x\n",
        //               sta_nwk_ -> name(sta_nwk_ -> cell(cur_inst)),
        //               lp -> function() -> asString(),
        //               var_count,
        //               kit_result);
        char* fn = Abc_SopCreateFromTruth(
            static_cast<Mem_Flex_t_*>(abc_nwk_->pManFunc),
            var_count,
            &kit_result);

        ret = Abc_NtkCreateNode(abc_nwk_);
        Abc_ObjSetData(ret, fn);

        // wire up the inputs
        // look up the index of each input by port
        // order.

        for (int i = 0; i < var_count; i++) {
          // get the port for the variable index
          std::string port_name = ix2portname[i];
          // get the net for the port
          Abc_Obj_t_* ip_net = ip_nets[port_name];
          // get the driving pin for the net
          const sta::Pin* driving_pin = net_table_abc2sta_[ip_net];
          /*
            printf("ip port [%d] %s driving pin %s  abc net %d Var ix %d\n",
                 i,
                 port_name.c_str(),
                 sta_nwk_ -> pathName(driving_pin),
                 ip_net -> Id,
                 i);
          */
          //
          // Note the abc truth table order is the variable
          // index order 0..N-1
          //
          Abc_ObjAddFanin(ret, ip_net);
        }

        // wire up the output
        if (net_table_sta2abc_.find(op_pin) != net_table_sta2abc_.end()) {
          debug++;
          Abc_Obj_t* op_wire = net_table_sta2abc_[op_pin];
          Abc_ObjAddFanin(op_wire, ret);
        } else {
          // make net and wire it in
          Abc_Obj_t* sop_op_net = Abc_NtkCreateNet(abc_nwk_);
          // net name is same as output pin
          Abc_ObjAssignName(
              sop_op_net, (char*) (sta_nwk_->pathName(op_pin)), NULL);
          Abc_ObjAddFanin(sop_op_net, ret);
          // note that again we are stashing the net on the output pin
          net_table_sta2abc_[op_pin] = sop_op_net;
          net_table_abc2sta_[sop_op_net] = op_pin;
        }
      }
    }
  }
  return ret;
}

void PhysRemap::Remap()
{
  if (script_) {
#ifdef DEBUG_RESTRUCT
    printf("Applying script %s\n", script_to_apply_.c_str());
#endif
  }
  // Build the abc network for the cut. This is sta -> abc conversion
  Cut2Ntk c2nwk(logger_, nwk_, cut_, cut_name_);
  if (!cut_->Check(nwk_)) {
    logger_->report("Error. Illegal cut");
    return;
  }

  Abc_Ntk_t* sop_logic_nwk = c2nwk.BuildSopLogicNetlist();

  // Annotate the timing requirements. We have found the blif
  // interface has weird problems with annotating timing
  // constraints so we do it directly on the network.
  AnnotateTimingRequirementsOntoABCNwk(sop_logic_nwk);

  // Now apply abc commands directly (if given)
  // else remap the logic after structure hashing and balancing
  Abc_Ntk_t* cut_logic = Abc_NtkToLogic(sop_logic_nwk);

  if (verify_) {
    // dress up the blif for formal
    Abc_Ntk_t* nwk_logic = Abc_NtkToNetlist(cut_logic);
    for (int i = 0; i < Abc_NtkPiNum(nwk_logic); i++) {
      Abc_Obj_t* abc_pi = Abc_NtkPi(nwk_logic, i);
      Abc_Obj_t* fanout_obj = Abc_ObjFanout0(abc_pi);
      Nm_ManDeleteIdName(fanout_obj->pNtk->pManName, fanout_obj->Id);
      Abc_ObjAssignName(
          fanout_obj, (char*) (nwk_->pathName(cut_->leaves_[i])), NULL);
    }
    for (int i = 0; i < Abc_NtkPoNum(nwk_logic); i++) {
      Abc_Obj_t* abc_po = Abc_NtkPo(nwk_logic, i);
      Abc_Obj_t* fanin_obj = Abc_ObjFanin0(abc_po);
      Nm_ManDeleteIdName(fanin_obj->pNtk->pManName, fanin_obj->Id);
      Abc_ObjAssignName(
          fanin_obj, (char*) (nwk_->pathName(cut_->roots_[i])), NULL);
    }

    Abc_NtkSetName(nwk_logic, Extra_UtilStrsav(cut_name_.c_str()));
    char fname[1000];
    sprintf(fname, "ip_%d.blif", cut_->id_);
    printf("Writing out cut %s for regression as blif file %s\n",
           cut_name_.c_str(),
           fname);
    std::string op_name = fname;
    Io_WriteBlif(nwk_logic, const_cast<char*>(op_name.c_str()), 1, 0, 0);
  }

  if (1) {
    Abc_Ntk_t* pNtk = Abc_NtkStrash(cut_logic, 0, 0, 0);
    Abc_Ntk_t* pNtk_res = Abc_NtkBalance(pNtk, 0, 0, 1);
    Abc_FrameReplaceCurrentNetwork(Abc_FrameGetGlobalFrame(), pNtk_res);
    int status = 1;
    std::string script_to_apply = "map  -M 0 -p \n";
    status
        = Cmd_CommandExecute(Abc_FrameGetGlobalFrame(), script_to_apply.data());
    if (status == 0) {
      // get the mapped netlist from the global frame
      // make sure the final result is mapped !
      Abc_Ntk_t* mapped_netlist = Abc_FrameReadNtk(Abc_FrameGetGlobalFrame());
      intermediate_result_ = mapped_netlist;

      if (verify_) {
        char tgain[100];
        sprintf(tgain, "tg_%d.txt", cut_->id_);
        FILE* fp = fopen(tgain, "w+");
        fprintf(fp, "Timing Gain %f\n", TimingGain(fp));
        fclose(fp);
        Abc_Ntk_t* m_netlist = Abc_NtkToNetlist(mapped_netlist);
        Abc_Ntk_t* result_logic = Abc_NtkToLogic(m_netlist);
        char fname[1000];
        sprintf(fname, "op_%d.blif", cut_->id_);
        printf("Writing out mapped cut as blif file %s\n", fname);

        // set up names to match inputs
        for (int i = 0; i < Abc_NtkPiNum(result_logic); i++) {
          Abc_Obj_t* abc_pi = Abc_NtkPi(result_logic, i);
          Nm_ManDeleteIdName(abc_pi->pNtk->pManName, abc_pi->Id);

          Abc_ObjAssignName(
              abc_pi, (char*) (nwk_->pathName(cut_->leaves_[i])), NULL);

          Abc_Obj_t* fanout_obj = Abc_ObjFanout0(abc_pi);
          Nm_ManDeleteIdName(fanout_obj->pNtk->pManName, fanout_obj->Id);
          Abc_ObjAssignName(
              fanout_obj, (char*) (nwk_->pathName(cut_->leaves_[i])), NULL);
        }

        for (int i = 0; i < Abc_NtkPoNum(result_logic); i++) {
          Abc_Obj_t* abc_po = Abc_NtkPo(result_logic, i);
          Nm_ManDeleteIdName(abc_po->pNtk->pManName, abc_po->Id);

          Abc_ObjAssignName(
              abc_po, (char*) (nwk_->pathName(cut_->roots_[i])), NULL);

          Abc_Obj_t* fanin_obj = Abc_ObjFanin0(abc_po);
          Nm_ManDeleteIdName(fanin_obj->pNtk->pManName, fanin_obj->Id);
          Abc_ObjAssignName(
              fanin_obj, (char*) (nwk_->pathName(cut_->roots_[i])), NULL);
        }
        Io_Write(result_logic, const_cast<char*>(fname), IO_FILE_BLIF);
      }

      if (accept_)
        BuildCutRealization(mapped_netlist);
    } else {
#ifdef DEBUG_RESTRUCT
      printf("Error running script %s\n", script_to_apply_.c_str());
#endif
    }
  } else /*here */ {
    // default: just low level remapping.
    Abc_Ntk_t* pNtk = Abc_NtkStrash(cut_logic, 0, 0, 0);
    Abc_Ntk_t* pNtk_res = Abc_NtkBalance(pNtk, 0, 0, 1);

    Amap_Par_t Pars_local;
    Amap_ManSetDefaultParams(&Pars_local);
    // favour timing
    Pars_local.fFreeInvs = 0;   // favour timing
    Pars_local.fADratio = 9.9;  // favour timing
    Pars_local.fVerbose = 1;
    Pars_local.fUseMuxes = 1;
    // remap
    Abc_Ntk_t* mapped = Abc_NtkDarAmap(pNtk_res, &Pars_local);
    Abc_Ntk_t* mapped_netlist = Abc_NtkToNetlist(mapped);
    intermediate_result_ = mapped_netlist;

    if (verify_) {
      char tgain[100];
      sprintf(tgain, "tg_%d.txt", cut_->id_);
      FILE* fp = fopen(tgain, "w+");
      fprintf(fp, "Timing Gain %f\n", TimingGain(fp));
      fclose(fp);

      if (!Abc_NtkHasAig(mapped_netlist) && !Abc_NtkHasMapping(mapped_netlist))
        Abc_NtkToAig(mapped_netlist);
      Abc_Ntk_t* result_logic = Abc_NtkToLogic(mapped_netlist);
      char fname[1000];
      sprintf(fname, "op_%d.blif", cut_->id_);
      Io_Write(result_logic, const_cast<char*>(fname), IO_FILE_BLIF);
    }

    // stitch the result back into the physical
    // netlist, if we are instructed to do so.
    // Else let parent optimizer decide.
    if (accept_)
      BuildCutRealization(mapped_netlist);
  }
}

void PhysRemap::AnnotateTimingRequirementsOntoABCNwk(Abc_Ntk_t* sop_logic_nwk)
{
  // Axiom the order in which the abc pi/po is made is :
  // inputs: leaf order in vector
  // outputs: root order in vector
  // So leaf at [0] corresponds to first abc CI.
  // So element at [leaf_count.size()] is first abc CO.
  int ip_ix = 0;
  int op_ix = 0;
  int ix = 0;
  // timing requirements are set in PhysTiming.cc/.hh
  sta::Sta* sta = sta::Sta::sta();
#ifdef REGRESS_RESTRUCT
  char fname[1000];
  sprintf(fname, "timing_%d.txt\n", cut_->id_);
  FILE* timing_file = fopen(fname, "w+");
#endif
  for (auto tr : timing_requirements_) {
    const sta::Pin* sta_pin = tr.first;
    TimingRecord* trec = tr.second;
    // inputs set arrival time
    if (ix < cut_->leaves_.size() && trec) {
      ip_ix = ix;
      Abc_Obj_t* pi = (Abc_Obj_t*) (Vec_PtrEntry(sop_logic_nwk->vCis, ip_ix));
#ifdef REGRESS_RESTRUCT
      fprintf(timing_file,
              "Ip %d (%s) Arr  r: %s (%f) f: %s (%f) Slack %s (%f) \n",
              pi->Id,
              nwk_->name(cut_->leaves_.at(ip_ix)),
              delayAsString(trec->arrival_rise, sta),
              trec->arrival_rise,
              delayAsString(trec->arrival_fall, sta),
              trec->arrival_fall,
              delayAsString(trec->slack, sta),
              trec->slack);
#endif
      Abc_NtkTimeSetArrival(
          sop_logic_nwk, pi->Id, trec->arrival_rise, trec->arrival_fall);
    }
    // outputs set required time
    else {
      if (trec) {
        op_ix = ix - cut_->leaves_.size();
        Abc_Obj_t* po = (Abc_Obj_t*) (Vec_PtrEntry(sop_logic_nwk->vCos, op_ix));
        Abc_NtkTimeSetRequired(
            sop_logic_nwk, po->Id, trec->required_rise, trec->required_fall);
#ifdef REGRESS_RESTRUCT
        fprintf(timing_file,
                "Op %d (%s) Rqd  r: %s (%f)  f: %s (%f) Slack %s (%f)\n",
                po->Id,
                nwk_->name(cut_->roots_.at(op_ix)),
                delayAsString(trec->required_rise, sta),
                trec->required_rise,
                delayAsString(trec->required_fall, sta),
                trec->required_fall,
                delayAsString(trec->slack, sta),
                trec->slack);
#endif
      }
    }
    ix++;
  }
#ifdef REGRESS_RESTRUCT
  fclose(timing_file);
#endif
}

bool PhysRemap::getAbcResult(abc::Abc_Ntk_t_*& result)
{
  if (intermediate_result_) {
    result = intermediate_result_;
    return true;
  }
  return false;
}

float PhysRemap::TimingGain(FILE* fp)
{
  Abc_Obj_t* po;
  int op_ix = 0;
  float min_rise_gain = 0.0;
  float min_fall_gain = 0.0;
  Abc_NtkForEachPo(intermediate_result_, po, op_ix)
  {
    Abc_Time_t_* arrival_time = Abc_NodeReadArrival(po);
    Abc_Time_t_* required_time = Abc_NodeReadRequired(po);
    if (fp)
      fprintf(fp,
              "Op %d Arrival R %f  F %f  Required R %f F %f\n",
              op_ix,
              arrival_time->Rise,
              arrival_time->Fall,
              required_time->Rise,
              required_time->Fall);
    if (arrival_time && required_time) {
      float rise_gain = required_time->Rise - arrival_time->Rise;
      float fall_gain = required_time->Fall - arrival_time->Fall;
      min_rise_gain = rise_gain > min_rise_gain ? rise_gain : min_rise_gain;
      min_fall_gain = fall_gain > min_fall_gain ? fall_gain : min_fall_gain;
    }
  }

  if (fp)
    fprintf(fp,
            "Gain: Min rise gain %f Min Fall gain %f\n",
            min_rise_gain,
            min_fall_gain);

  if (min_rise_gain > min_fall_gain)
    return min_rise_gain;
  return min_fall_gain;
}

void PhysRemap::BuildCutRealization(Abc_Ntk_t* nwk)
{
  // Build the cut in the network.
  // And delete the original volume.
  Ntk2Cut n2c(
      logger_, nwk, cut_, nwk_, block_, locell_, loport_, hicell_, hiport_);
  n2c.BuildNwkElements();
}

// Routines for build odb from cut

// Library reader routines
// Read in the liberty library using abc

LibRead::LibRead(utl::Logger* logger, char* lib_file_name, bool verbose)
    : logger_(logger), lib_file_name_(lib_file_name), verbose_(verbose)
{
  Abc_Start();
  std::string cmd("read_lib -v ");

  std::string cmd_line = cmd + lib_file_name_;
  Abc_Frame_t* pAbc = Abc_FrameGetGlobalFrame();
  Cmd_CommandExecute(pAbc, cmd_line.c_str());
  // Check we have a library
  if (Abc_FrameReadLibGen() == NULL)
    logger->report("Error. Library is not available\n");
  else
    logger->report("Successfully read in library\n");
}

// Routines for abc -> sta

Ntk2Cut::Ntk2Cut(utl::Logger* logger,
                 Abc_Ntk_t* abc_nwk,
                 Cut* cut,
                 sta::Network* sta_nwk,
                 odb::dbBlock* block,
                 std::string& locell,
                 std::string& loport,
                 std::string& hicell,
                 std::string& hiport)
    : logger_(logger),
      mapped_netlist_(abc_nwk),
      cut_(cut),
      target_nwk_(sta_nwk),
      block_(block),
      locell_(locell),
      loport_(loport),
      hicell_(hicell),
      hiport_(hiport)
{
}

/*
  Make the sta network for the elements of the cut
*/

//
// Routines for checking cut building in network
//

void Ntk2Cut::WritePinEnvironment(FILE* fp, const sta::Pin* p, const char* str)
{
  fprintf(fp, "%s Pin %s\n", str, target_nwk_->name(p));
  std::set<sta::Pin*> visited;
  sta::Net* cur_net = target_nwk_->net(p);
  if (cur_net == nullptr) {
    fprintf(fp, " (Pin %s has no net)\n", target_nwk_->name(p));
  } else {
    fprintf(fp, "Net %s :\n", target_nwk_->name(cur_net));
    sta::PinSet* drivers = target_nwk_->drivers(cur_net);
    if (drivers) {
      sta::PinSet::Iterator drvr_iter(drivers);
      while (drvr_iter.hasNext()) {
        sta::Pin* driving_pin = const_cast<sta::Pin*>(drvr_iter.next());
        fprintf(fp, "\t <- %s\n", target_nwk_->name(driving_pin));
        visited.insert(driving_pin);
      }
    }
    sta::PinConnectedPinIterator* connected_pin_iter
        = target_nwk_->connectedPinIterator(p);
    while (connected_pin_iter->hasNext()) {
      sta::Pin* connected_pin
          = const_cast<sta::Pin*>(connected_pin_iter->next());
      if (visited.find(connected_pin) == visited.end()) {
        fprintf(fp, "\t -> %s \n", target_nwk_->name(connected_pin));
      }
    }
  }
}

void Ntk2Cut::WriteCutEnvironment(char* prefix, FILE* fp)
{
  fprintf(fp, "++++++++\n");
  fprintf(fp, "%s Cut %d environment:\n", prefix, cut_->id_);
  for (auto l : cut_->leaves_) {
    WritePinEnvironment(fp, l, "Leaf:");
  }
  for (auto r : cut_->roots_) {
    WritePinEnvironment(fp, r, "Root:");
  }
  fprintf(fp, "--------\n");
}

void Ntk2Cut::WriteCutInNwk(FILE* fp)
{
  std::set<const sta::Pin*> end_points;

  for (auto l : cut_->leaves_) {
    end_points.insert(l);
  }
  fprintf(fp, "+++++++++++\n");
  fprintf(fp, "Cut contents nwk\n");
  for (auto op_pin : cut_->roots_) {
    fprintf(fp, "Root: %s\n", target_nwk_->name(op_pin));
    std::set<const sta::Pin*> visited;
    WriteCutInNwkR(fp, 0, op_pin, end_points, visited);
  }
  fprintf(fp, "----------\n");
}

void Ntk2Cut::Tabs(FILE* fp, int depth)
{
  for (int i = 0; i < depth; i++) {
    fprintf(fp, "\t");
  }
}

void Ntk2Cut::ReportInstanceIpsOps(int depth, FILE* fp, sta::Instance* cur_inst)
{
  sta::InstancePinIterator* ip = target_nwk_->pinIterator(cur_inst);
  while (ip->hasNext()) {
    std::set<const sta::Pin*> visited;
    const sta::Pin* cur_pin = ip->next();
    if (target_nwk_->direction(cur_pin)->isInput()) {
      visited.insert(cur_pin);
      sta::Net* cur_net = target_nwk_->net(cur_pin);
      sta::PinSet* drivers = target_nwk_->drivers(cur_net);
      if (drivers) {
        sta::PinSet::Iterator drvr_iter(drivers);
        sta::Pin* driving_pin = const_cast<sta::Pin*>(drvr_iter.next());
        Tabs(fp, depth);
        fprintf(fp,
                "Ip %s <- %s\n",
                target_nwk_->name(cur_pin),
                target_nwk_->name(driving_pin));
      }
    } else {
      sta::Net* cur_net = target_nwk_->net(cur_pin);
      sta::PinConnectedPinIterator* connected_pin_iter
          = target_nwk_->connectedPinIterator(cur_pin);
      Tabs(fp, depth);
      fprintf(fp, "Op %s ", target_nwk_->name(cur_pin));
      visited.insert(cur_pin);
      while (connected_pin_iter->hasNext()) {
        sta::Pin* connected_pin
            = const_cast<sta::Pin*>(connected_pin_iter->next());
        if (visited.find(connected_pin) == visited.end()) {
          visited.insert(connected_pin);
          Tabs(fp, depth);
          fprintf(fp, " -> %s ", target_nwk_->name(connected_pin));
        }
      }
    }
  }
  fprintf(fp, "\n");
}

void Ntk2Cut::WriteCutInNwkR(FILE* fp,
                             int depth,
                             const sta::Pin* op_pin,
                             std::set<const sta::Pin*>& end_points,
                             std::set<const sta::Pin*>& visited)
{
  if (visited.find(op_pin) == visited.end()) {
    visited.insert(op_pin);
    sta::Instance* cur_inst = target_nwk_->instance(op_pin);
    Tabs(fp, depth), fprintf(fp, "I: %s \n", target_nwk_->name(cur_inst));
    ReportInstanceIpsOps(depth, fp, cur_inst);
    fprintf(fp, "\n");
    sta::InstancePinIterator* ip = target_nwk_->pinIterator(cur_inst);
    while (ip->hasNext()) {
      const sta::Pin* ip_pin = ip->next();
      if (target_nwk_->direction(ip_pin)->isInput()) {
        visited.insert(ip_pin);
        sta::Net* cur_net = target_nwk_->net(ip_pin);
        sta::PinSet* drivers = target_nwk_->drivers(cur_net);
        if (drivers) {
          sta::PinSet::Iterator drvr_iter(drivers);
          sta::Pin* driving_pin = const_cast<sta::Pin*>(drvr_iter.next());
          if (end_points.find(driving_pin) != end_points.end()) {
            Tabs(fp, depth),
                fprintf(fp, "LEAF %s\n", target_nwk_->name(driving_pin));
          } else if (target_nwk_->direction(driving_pin)->isOutput()) {
            WriteCutInNwkR(fp, depth + 1, driving_pin, end_points, visited);
          }
        }
      }
    }
  }
}

int Ntk2Cut::GetPOIx(abc::Abc_Ntk_t_* nwk, Abc_Obj_t* obj)
{
  int ret = 0;
  for (int i = 0; i < Abc_NtkPoNum(nwk); i++) {
    if (obj == Abc_NtkPo(nwk, i))
      return i;
  }
  return -1;
}

abc::Abc_Obj_t_* Ntk2Cut::DrivesAbcPO(abc::Abc_Obj_t_* abc_node)
{
  int i;
  Abc_Obj_t* pFanout;
  Abc_ObjForEachFanout(abc_node, pFanout, i)
  {
    if (pFanout->Type == ABC_OBJ_PO) {
      return pFanout;
    }
  }
  return nullptr;
}

void Ntk2Cut::BuildNwkElements()
{
  std::map<Abc_Obj_t*, sta::Net*> abc2net;
  std::map<sta::Net*, Abc_Obj_t_*> net2abc;
  std::map<Abc_Obj_t*, sta::Instance*> abc2inst;
  std::map<sta::Instance*, Abc_Obj_t*> inst2abc;
  std::map<const sta::Pin*, sta::Net*> cut_ip_nets;

  char cut_environment[1000];
  sprintf(cut_environment, "cut_%d_env.txt", cut_->id_);
  FILE* fp = fopen(cut_environment, "w+");

#ifdef DEBUG_RESTRUCT
  cut_->Print(fp, target_nwk_);
  WriteCutEnvironment("before building from abc", fp);
#endif

  Abc_Obj_t* pObj = nullptr;
  int i;
  static int net_id = 0;
  static int inst_id = 0;

  sta::Instance* parent = target_nwk_->topInstance();

  // Create a net for each primary input
  for (i = 0; i < Abc_NtkPiNum(mapped_netlist_); i++) {
    pObj = Abc_NtkPi(mapped_netlist_, i);
    sta::Pin* cut_ip_pin = const_cast<sta::Pin*>(cut_->leaves_[i]);

    sta::Net* ip_net = target_nwk_->net(cut_ip_pin);
    if (!ip_net) {
      ip_net = target_nwk_->findNet(target_nwk_->pathName(cut_ip_pin));
    }
    cut_ip_nets[cut_ip_pin] = ip_net;
    abc2net[pObj] = ip_net;
    net2abc[ip_net] = pObj;
    net_id++;
  }

  // Create the instances and make a net for each instance output (single output
  // objects only)
  i = 0;
  Abc_NtkForEachNode(mapped_netlist_, pObj, i)
  {
    inst_id++;
    if (pObj->Type == ABC_OBJ_PI || pObj->Type == ABC_OBJ_PO) {
      continue;
    } else {
      Mio_Gate_t* mapped_gate = (Mio_Gate_t*) (pObj->pData);
      std::string op_port_name = mapped_gate->pOutName;
      char* lib_gate_name = Mio_GateReadName(mapped_gate);

      std::string inst_name
          = std::string(lib_gate_name) + "_" + std::to_string(inst_id);
      while (target_nwk_->findInstance(inst_name.c_str())) {
        inst_id++;
        inst_name = std::string(lib_gate_name) + "_" + std::to_string(inst_id);
      }
      sta::LibertyCell* liberty_cell
          = target_nwk_->findLibertyCell(lib_gate_name);

      if (!liberty_cell) {
        // check for constant
        if (!strcmp(lib_gate_name, "_const0_")) {
          liberty_cell = target_nwk_->findLibertyCell(locell_.c_str());
          op_port_name = loport_;
        } else if (!strcmp(lib_gate_name, "_const1_")) {
          liberty_cell = target_nwk_->findLibertyCell(hicell_.c_str());
          op_port_name = hiport_;
        }
      }
      if (!liberty_cell) {
        logger_->error(
            RMP, 147, "Unable to find constant cell for {}.", lib_gate_name);
      }

      sta::Instance* cur_inst
          = ((sta::ConcreteNetwork*) target_nwk_)
                ->makeInstance(liberty_cell, inst_name.c_str(), parent);
      ((sta::ConcreteNetwork*) target_nwk_)->makePins(cur_inst);
      sta::LibertyPort* liberty_op_port
          = liberty_cell->findLibertyPort(op_port_name.c_str());

      inst2abc[cur_inst] = pObj;
      abc2inst[pObj] = cur_inst;

      // check does object drive po,
      // if so use cut output net and don't make a new net
      Abc_Obj_t* abc_po;
      abc_po = DrivesAbcPO(pObj);
      if (abc_po) {
        int ix = GetPOIx(mapped_netlist_, abc_po);
        sta::Net* op_net = target_nwk_->net(cut_->roots_[ix]);
        ((sta::NetworkEdit*) target_nwk_)
            ->disconnectPin(const_cast<sta::Pin*>(cut_->roots_[ix]));
        cut_->roots_[ix] = target_nwk_->findPin(cur_inst, liberty_op_port);
        abc2net[pObj] = op_net;
        net2abc[op_net] = pObj;
        ((sta::ConcreteNetwork*) target_nwk_)
            ->connect(cur_inst, liberty_op_port, op_net);
#ifdef DEBUG_RESTRUCT
        printf("Recycling root net\n");
#endif
      } else {
        std::string op_net_name = inst_name + "_" + std::to_string(net_id);
        net_id++;
        sta::Net* op_net = ((sta::ConcreteNetwork*) target_nwk_)
                               ->makeNet(op_net_name.c_str(), parent);
        abc2net[pObj] = op_net;
        net2abc[op_net] = pObj;
        ((sta::ConcreteNetwork*) target_nwk_)
            ->connect(cur_inst, liberty_op_port, op_net);
      }
    }
  }

  // Wire up the inputs of the instances
  i = 0;
  Abc_NtkForEachNode(mapped_netlist_, pObj, i)
  {
    if (pObj->Type == ABC_OBJ_PI || pObj->Type == ABC_OBJ_PO) {
      continue;
    } else {
      Mio_Gate_t* mapped_gate = (Mio_Gate_t*) (pObj->pData);
      char* lib_gate_name = Mio_GateReadName(mapped_gate);
      sta::LibertyCell* liberty_cell = ((sta::ConcreteNetwork*) target_nwk_)
                                           ->findLibertyCell(lib_gate_name);
      sta::Instance* cur_inst = abc2inst[pObj];
      // get the inputs
      int fanin_count = Abc_ObjFaninNum(pObj);
      for (int ip_ix = 0; ip_ix < fanin_count; ip_ix++) {
        Abc_Obj_t* driver = Abc_ObjFanin(pObj, ip_ix);
        sta::Net* driving_net = abc2net[driver];

        if (!driving_net) {
          printf(
              "Cannot find input %d driver for abc obj %d of type %d which "
              "isdriven by abc obj %d\n",
              ip_ix,
              pObj->Id,
              pObj->Type,
              driver->Id);
          continue;
        }
        assert(driving_net);
        char* dest_name = Mio_GateReadPinName(mapped_gate, ip_ix);
        sta::Pin* dest_pin = ((sta::ConcreteNetwork*) target_nwk_)
                                 ->findPin(cur_inst, dest_name);
        assert(dest_pin);
        // connect driving pin to this pin
        if (!((sta::ConcreteNetwork*) target_nwk_)
                 ->isConnected(driving_net, dest_pin))
          ((sta::ConcreteNetwork*) target_nwk_)
              ->connectPin(dest_pin, driving_net);
        else
          printf("Weird -- pin already connected to destination\n");
      }
    }
  }

  // STA will clean up the connections as we delete the instances
  for (auto cur_inst : cut_->volume_) {
    ((sta::ConcreteNetwork*) target_nwk_)
        ->deleteInstance(const_cast<sta::Instance*>(cur_inst));
  }

#ifdef DEBUG_RESTRUCT
  WriteCutInNwk(fp);
  WriteCutEnvironment("after", fp);
#endif
  fclose(fp);
}

void Ntk2Cut::FuseNets(int root_ix,
                       sta::Pin* new_driving_pin,
                       sta::Net* new_driving_net,
                       sta::Pin* orig_driving_pin,
                       sta::Net* orig_driving_net)
{
  sta::Port* new_driver_port = target_nwk_->port(new_driving_pin);
  sta::Instance* new_driving_instance = target_nwk_->instance(new_driving_pin);

  sta::NetPinIterator* npi = target_nwk_->pinIterator(orig_driving_net);
  while (npi->hasNext()) {
    sta::Pin* sink_pin = const_cast<sta::Pin*>(npi->next());
    if (sink_pin != orig_driving_pin) {
      sta::Instance* sink_instance = target_nwk_->instance(sink_pin);
      sta::Port* sink_port = target_nwk_->port(sink_pin);
      ((sta::NetworkEdit*) target_nwk_)
          ->disconnectPin(const_cast<sta::Pin*>(sink_pin));
      ((sta::NetworkEdit*) target_nwk_)
          ->connect(sink_instance, sink_port, new_driving_net);
    }
  }
  ((sta::NetworkEdit*) target_nwk_)->disconnectPin(orig_driving_pin);
  cut_->roots_[root_ix] = new_driving_pin;
}

void Restructure::annotateCutTiming(
    Cut* cut,
    std::vector<std::pair<const sta::Pin*, TimingRecord*>>& timing_requirements)
{
  sta::Sta* sta = sta::Sta::sta();
  sta::NetworkReader* network = sta->networkReader();
  sta::Arrival max_arrival_rise = 0.0;
  sta::Arrival max_arrival_fall = 0.0;
  sta::Required min_required_rise;
  sta::Required min_required_fall;
  sta::Slack s;

  // Get the arrival and required times for the exceptions for each vertex.
  sta::Sdc* sdc_nwk = sta->sdc();
  sta::ClockSeq* clock_seq = sdc_nwk->clocks();
  sta::Graph* graph = sta->graph();

  int ip_ix = 0;
  TimingRecord* tr = nullptr;

  //
  // cut inputs: allow for delayed arrival time
  //
  for (auto leaf_pin : cut->leaves_) {
    sta::VertexId v_id = network->vertexId(graph->id(), leaf_pin);
    sta::Vertex* v = graph->vertex(v_id);
    tr = nullptr;
    if (v) {
      sta::MinMax* min_max_arrival;
      sta::MinMax* min_max = sta::MinMax::find("max");
      sta::RiseFall* rf_arrival_rise = sta::RiseFall::find("rise");
      sta::RiseFall* rf_arrival_fall = sta::RiseFall::find("fall");
      max_arrival_rise = sta->vertexArrival(v, rf_arrival_rise, min_max);
      max_arrival_fall = sta->vertexArrival(v, rf_arrival_fall, min_max);

      sta::Pin* cur_pin = v->pin();
      sta::Slack max_s = sta->pinSlack(cur_pin, sta::MinMax::max());
      sta::Slack min_s = sta->pinSlack(cur_pin, sta::MinMax::min());
      s = (max_s == sta::INF || max_s == -sta::INF) ? min_s : max_s;

      for (auto clk : *clock_seq) {
        sta::ClockEdge* clk_edge_fall = clk->edge(rf_arrival_fall);
        sta::ClockEdge* clk_edge_rise = clk->edge(rf_arrival_rise);
        for (auto path_ap : sta->corners()->pathAnalysisPts()) {
          sta::Arrival v_arrival_rise = sta->vertexArrival(
              v, rf_arrival_rise, clk_edge_rise, path_ap, min_max);
          sta::Arrival v_arrival_fall = sta->vertexArrival(
              v, rf_arrival_fall, clk_edge_fall, path_ap, min_max);
          if (v_arrival_rise > max_arrival_rise)
            max_arrival_rise = v_arrival_rise;
          if (v_arrival_fall > max_arrival_fall)
            max_arrival_fall = v_arrival_fall;
        }
      }
    }

    TimingRecord* tr = nullptr;
    if (graph && v) {
      tr = new TimingRecord();
      tr->arrival_rise = atof(delayAsString(max_arrival_rise, sta));
      tr->arrival_fall = atof(delayAsString(max_arrival_fall, sta));
      tr->arrival_fall = atof(delayAsString(max_arrival_fall, sta));
      tr->slack = atof(delayAsString(s, sta));
      //      printf("Leaf %s\n", network->pathName(v -> pin()));
      /*      printf(" Arrival %s (%f) Fall %s (%f) Slack %s (%f)\n",

             delayAsString(max_arrival_rise, sta),
             max_arrival_rise,

             delayAsString(max_arrival_fall, sta),
             max_arrival_rise ,

             delayAsString(s, sta),
             s);
      */
    }
    timing_requirements.push_back(
        std::pair<const sta::Pin*, TimingRecord*>(leaf_pin, tr));
    ip_ix++;

#ifdef DEBUG_RESTRUCT
    printf("Pin %s Setting arrival rise %s (%f)\n",
           network->pathName(leaf_pin),
           delayAsString(tr->arrival_rise, sta),
           1e9 * tr->arrival_rise);
    printf("Pin %s Setting arrival fall %s (%f)\n",
           network->pathName(leaf_pin),
           delayAsString(tr->arrival_fall, sta),
           1e9 * tr->arrival_fall);
#endif
  }

  // cut outputs: set required time
  for (auto root_pin : cut->roots_) {
    sta::VertexId v_id = network->vertexId(graph->id(), root_pin);
    sta::Vertex* v = graph->vertex(v_id);
    bool got_required = false;

    if (v) {
      sta::MinMax* min_max_arrival;
      sta::RiseFall* rf_required_rise = sta::RiseFall::find("rise");
      sta::RiseFall* rf_required_fall = sta::RiseFall::find("fall");
      sta::MinMax* min_max = sta::MinMax::find("max");

      sta::Required v_required_rise = sta->vertexRequired(v, min_max);
      sta::Required v_required_fall = sta->vertexRequired(v, min_max);
      min_required_rise = v_required_rise;
      min_required_fall = v_required_fall;
      got_required = true;
    }

    TimingRecord* tr = nullptr;
    if (graph && v && got_required) {
      tr = new TimingRecord();
      tr->required_rise = atof(delayAsString(min_required_rise, sta));
      tr->required_fall = atof(delayAsString(min_required_fall, sta));
    }
    timing_requirements.push_back(
        std::pair<const sta::Pin*, TimingRecord*>(root_pin, tr));

#ifdef DEBUG_RESTRUCT
    printf("Pin %s required rise %s (%f)\n",
           network->pathName(root_pin),
           delayAsString(tr->required_rise, sta),
           tr->required_rise);
    printf("Pin %s required fall %s (%f) \n",
           network->pathName(root_pin),
           delayAsString(tr->required_fall, sta),
           tr->required_fall);
#endif
  }
}

}  // namespace rmp
