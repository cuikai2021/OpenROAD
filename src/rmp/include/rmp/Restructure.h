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

#pragma once

#include <functional>
#include <string>
#include <vector>

#include "db_sta/dbSta.hh"
#include "rsz/Resizer.hh"

namespace abc {
struct Abc_Obj_t_;
struct Abc_Ntk_t_;
struct Amap_Lib_t_;
struct Amap_Par_t_;
extern Abc_Ntk_t_* Abc_NtkDarAmap(Abc_Ntk_t_*, Amap_Par_t_*);
}  // namespace abc

namespace utl {
class Logger;
}

namespace odb {
class dbDatabase;
class dbBlock;
class dbInst;
class dbNet;
class dbITerm;
}  // namespace odb

namespace sta {
class dbSta;
class Pin;
class Network;
class Vertex;
class LibertyBuilder;
}  // namespace sta

namespace rmp {

//
// Helper Objects and structures for timing driven remapping
//

// Timing record for constraining abc.
struct TimingRecord
{
  float required_rise;
  float required_fall;
  float arrival_rise;
  float arrival_fall;
  float slack;
  float load;
};

// Cut: Region of network to optimize
struct Cut
{
  std::vector<const sta::Pin*> roots_;
  std::vector<const sta::Pin*> leaves_;
  std::vector<sta::Instance*> volume_;
  std::map<const sta::Pin*, sta::LibertyPort*> internal_pins_;
  int id_;

  void Print(FILE* fp, sta::Network* nwk);
  void PrintVerilog(FILE* fp, sta::Network* nwk);
  // todo print dot
  bool Check(sta::Network* nwk);

  void BuildFuncExpr(sta::Network* nwk,
                     sta::LibertyCell* cut_cell,
                     sta::LibertyBuilder& liberty_builder,
                     std::vector<sta::FuncExpr*>& root_fns);

  bool LeafPin(const sta::Pin* cur_pin);
  bool RootPin(const sta::Pin* cur_pin);

  sta::FuncExpr* ExpandFunctionExprR(
      sta::Network* nwk,
      const sta::Pin* op_pin,
      sta::FuncExpr* op_pin_fe,
      std::map<const sta::Pin*, sta::LibertyPort*>& leaf_pin_to_port,
      std::map<const sta::Pin*, sta::FuncExpr*>& visited,
      sta::LibertyCell* cut_cell,
      sta::LibertyBuilder& lib_builder);
  sta::FuncExpr* BuildFuncExprR(
      sta::Network* nwk,
      sta::Instance* cur_inst,
      const sta::Pin* op_pin,
      sta::FuncExpr* op_pin_fe,
      std::map<const sta::Pin*, sta::LibertyPort*>& leaf_pin2port,
      std::map<const sta::Pin*, sta::FuncExpr*>& visited_pins,
      sta::LibertyCell* cut_cell,
      sta::LibertyBuilder& lib_builder);
};

class CutGen
{
 public:
  using expansionSet = std::vector<
      std::pair<std::vector<const sta::Pin*>, std::vector<const sta::Pin*>>>;
  CutGen(sta::Network* nwk, sta::LibertyLibrary* liberty_lib)
      : network_(nwk), liberty_lib_(liberty_lib)
  {
  }

  void AmendCutForEscapeRoots(Cut* cur_cut);

  void GenerateInstanceWaveFrontCutSet(sta::Instance*, std::vector<Cut*>&);

  bool EnumerateExpansions(std::vector<const sta::Pin*>& candidate_wavefront,
                           expansionSet& expansion_set);
  Cut* BuildCutFromExpansionElement(
      sta::Instance* cur_inst,
      const sta::Pin* root_pin,
      std::vector<const sta::Pin*>& keep,  // set of pins to keep
      std::vector<const sta::Pin*>& expand);

  void ExpandRegularPin(sta::Pin* driving_pin,
                        std::set<const sta::Pin*>& leaf_driver_pins,
                        std::set<sta::Instance*>& volume_insts,
                        std::set<const sta::Pin*>& visited_driver_pins,
                        Cut* cut);

  bool Boundary(const sta::Pin* cur_pin);
  bool IsBufferInverterInstance(sta::Instance* inst);

  bool AccumulateInverterBuffers(sta::Pin* bufferinverter_op_pin,
                                 std::set<const sta::Pin*>& leaf_driver_pins,
                                 std::set<sta::Instance*>& volume_insts,
                                 std::set<const sta::Pin*>& visited_driver_pins,
                                 Cut* cut,
                                 sta::Pin*& driving_pin_out);

  bool SelfFeedBack(const sta::Instance*);
  bool InstanceOpConnectedToCutBoundary(const sta::Instance*, Cut* cur_cut);

  unsigned OpPinCount(const sta::Instance*);

 private:
  sta::Network* network_;
  sta::LibertyLibrary* liberty_lib_;
};

// Library reader.
class LibRead
{
 public:
  LibRead(utl::Logger* logger, char* lib_file_name, bool verbose);

 private:
  utl::Logger* logger_;
  char* lib_file_name_;
  bool verbose_;
  abc::Amap_Lib_t_* lib_;
  friend class PhysRemap;
};

// Build an abc network at source code level from a cut on the sta
class Cut2Ntk
{
 public:
  Cut2Ntk(utl::Logger* logger,
          sta::Network* nwk,
          Cut* cut,
          std::string cut_name)
      : logger_(logger), sta_nwk_(nwk), cut_(cut), cut_name_(cut_name)
  {
  }
  abc::Abc_Ntk_t_* BuildSopLogicNetlist();
  abc::Abc_Obj_t_* CreateGate(const sta::Instance* cur_inst);
  void CleanUpNetNets();

 private:
  utl::Logger* logger_;
  sta::Network* sta_nwk_;
  Cut* cut_;
  std::string cut_name_;
  abc::Abc_Ntk_t_* abc_nwk_;
  // cross reference dictionaries for ease of constraining.
  std::map<const sta::Pin*, abc::Abc_Obj_t_*> pi_table_;
  std::map<const sta::Pin*, abc::Abc_Obj_t_*> po_table_;
  std::map<const sta::Pin*, abc::Abc_Obj_t_*> net_table_sta2abc_;  // sta -> abc
  std::map<abc::Abc_Obj_t_*, const sta::Pin*> net_table_abc2sta_;  // abc ->
                                                                   // sta.
  friend class Restructure;
};

// Build an sta network from a cut and its abc network
class Ntk2Cut
{
 public:
  Ntk2Cut(utl::Logger* logger,
          abc::Abc_Ntk_t_*,
          Cut*,
          sta::Network*,
          odb::dbBlock* block,
          std::string& locell,
          std::string& loport,
          std::string& hicell,
          std::string& hiport);

  void BuildNwkElements();
  void FuseNets(int root_ix,
                sta::Pin* new_driving_pin,
                sta::Net* new_driving_net,
                sta::Pin* orig_driving_pin,
                sta::Net* orig_driving_net);

  abc::Abc_Obj_t_* DrivesAbcPO(abc::Abc_Obj_t_* obj);
  int GetPOIx(abc::Abc_Ntk_t_* nwk, abc::Abc_Obj_t_* obj);
  // Routines to debug cut creation in network.
  void ReportInstanceIpsOps(int depth, FILE* fp, sta::Instance* cur_inst);
  void WriteCutEnvironment(char* str, FILE* fp);
  void WritePinEnvironment(FILE* fp, const sta::Pin*, const char* str);
  void Tabs(FILE* fp, int depth);
  void WriteCutInNwk(FILE* fp);
  void WriteCutInNwkR(FILE* fp,
                      int depth,
                      const sta::Pin* op_pin,
                      std::set<const sta::Pin*>& end_points,
                      std::set<const sta::Pin*>& visited);

 private:
  utl::Logger* logger_;
  abc::Abc_Ntk_t_* mapped_netlist_;
  Cut* cut_;
  sta::Network* target_nwk_;
  odb::dbBlock* block_;
  std::string locell_;
  std::string loport_;
  std::string hicell_;
  std::string hiport_;
};

// Remap a cut
class PhysRemap
{
 public:
  PhysRemap(utl::Logger* logger,
            sta::Network* nwk,
            Cut* cut_to_remap,
            std::vector<std::pair<const sta::Pin*, TimingRecord*>>&
                timing_requirements,
            char* lib_name,
            LibRead& lib_file,
            bool script,
            std::string& script_to_apply,
            bool accept,
            odb::dbBlock* block,
            std::string locell,
            std::string loport,
            std::string hicell,
            std::string hiport,
            bool verify = false)
      : logger_(logger),
        nwk_(nwk),
        cut_(cut_to_remap),
        timing_requirements_(timing_requirements),
        lib_name_(lib_name),
        liberty_library_(lib_file.lib_),
        script_(script),
        script_to_apply_(script_to_apply),
        accept_(accept),
        block_(block),
        locell_(locell),
        loport_(loport),
        hicell_(hicell),
        hiport_(hiport),
        verify_(verify)
  {
    cut_name_ = "cut_" + std::to_string(cut_to_remap->id_);
    global_optimize_ = false;
    verbose_ = true;
  }
  void Remap();
  void AnnotateTimingRequirementsOntoABCNwk(abc::Abc_Ntk_t_* sop_logic_nwk);
  bool getAbcResult(abc::Abc_Ntk_t_*&);
  void BuildCutRealization(abc::Abc_Ntk_t_*);
  float TimingGain(FILE* fp = nullptr);

 private:
  utl::Logger* logger_;
  sta::Network* nwk_;

  Cut* cut_;
  std::vector<std::pair<const sta::Pin*, TimingRecord*>>& timing_requirements_;
  char* lib_name_;
  std::string cut_name_;
  abc::Amap_Lib_t_* liberty_library_;

  bool global_optimize_;
  bool verbose_;
  bool script_;
  std::string script_to_apply_;
  bool accept_;
  abc::Abc_Ntk_t_* intermediate_result_;

  odb::dbBlock* block_;
  std::string locell_;
  std::string loport_;
  std::string hicell_;
  std::string hiport_;
  bool verify_;

  friend class Restructure;
};

using utl::Logger;

enum class Mode
{
  AREA_1 = 0,
  AREA_2,
  AREA_3,
  DELAY_1,
  DELAY_2,
  DELAY_3,
  DELAY_4
};

class Restructure
{
 public:
  Restructure() = default;
  ~Restructure();

  void init(utl::Logger* logger,
            sta::dbSta* open_sta,
            odb::dbDatabase* db,
            rsz::Resizer* resizer);

  void reset();

  void cutresynthrun(char* liberty_file_name,
                     char* script,
                     sta::Pin* head_pin,
                     bool single_op,
                     bool unconstrained,
                     bool oracle_remap,
                     bool verbose,
                     bool verify = false);

  void run(
      char* liberty_file_name,
      float slack_threshold,
      unsigned max_depth,
      char* workdir_name,
      char* abc_logfile,
      bool cut_restructure,    // if true use cut restructuring, else old flow
      char* head_pin_ptr_str,  // the start pin to generate the cut
      bool unconstrained,  // unconstrained -> walk back to invariant points for
                           // resynth
      bool single_op_cut,  // generate single or multiple output cuts
      bool oracle,         // undocumented
      char* script,
      bool verify);

  void setMode(const char* mode_name);
  void setTieLoPort(sta::LibertyPort* loport);
  void setTieHiPort(sta::LibertyPort* hiport);

  static bool isRegInput(sta::Network* nwk, sta::Vertex* vertex);
  static bool isRegOutput(sta::Network* nwk, sta::Vertex* vertex);
  static bool isPrimary(sta::Network* nwk, sta::Vertex* vertex);

  void ResetCutTemporaries()
  {
    leaves_.clear();
    roots_.clear();
    cut_volume_.clear();
    ordered_leaves_.clear();
    ordered_roots_.clear();
    pin_visited_.clear();
  }
  void AmendCutForEscapeLeaves(sta::Network* nwk);

  // cut generation methods:
  // Wave front (restricts search for cuts to region)
  // Single output cuts. Walk back through wavefront. Given start point
  void generateWaveFrontSingleOpCutSet(sta::Network* nwk,
                                       sta::Instance*,  // start from instance
                                       std::vector<Cut*>& local_cut_set);
  void generateWaveFrontSingleOpCutSet(sta::Network* nwk,
                                       sta::Pin*,  // start from pin
                                       std::vector<Cut*>& local_cut_set);

  // Unconstained cuts, can walk right back to invariant points. Given start pin
  //(start pin selected by caller eg after sensitivity analysis).
  void generateUnconstrainedCut(sta::Pin* head_pin,
                                bool single_op,
                                std::vector<Cut*>& cut_set);

  //
  // Generate cuts using timing information. Start from outputs of logic (eg
  // dinputs). Kind of like blob generation algorithm 1: generate a single
  // output cut (useful for oracle mapping). single_op=true algorithm 2:
  // generate a multiple input/multiple output cut (for big restructuring).
  void generateUnconstrainedCuts(std::vector<Cut*>& cut_set, bool single_op);

  void annotateCutTiming(Cut*,
                         std::vector<std::pair<const sta::Pin*, TimingRecord*>>&
                             timing_requirements);

  odb::dbBlock* block() { return block_; }
  std::string& locell() { return locell_; }
  std::string& loport() { return loport_; }
  std::string& hicell() { return hicell_; }
  std::string& hiport() { return hiport_; }

 private:
  // temporaries for cut building
  std::set<sta::Vertex*> end_points_;  // full set of end points
  std::map<sta::Pin*, int> leaves_;
  std::map<sta::Pin*, int> roots_;
  std::set<sta::Instance*> cut_volume_;
  std::set<sta::Pin*> pin_visited_;
  std::vector<sta::Pin*> ordered_leaves_;
  std::vector<sta::Pin*> ordered_roots_;

  void deleteComponents();
  void getBlob(unsigned max_depth);
  void runABC();
  void postABC(float worst_slack);
  bool writeAbcScript(std::string file_name);
  void writeOptCommands(std::ofstream& script);
  void initDB();
  void getEndPoints(sta::PinSet& ends, bool area_mode, unsigned max_depth);
  int countConsts(odb::dbBlock* top_block);
  void removeConstCells();
  void removeConstCell(odb::dbInst* inst);
  bool readAbcLog(std::string abc_file_name, int& level_gain, float& delay_val);

  //
  // cut based optimization. Directly uses abc with timing constraints.
  //

  Cut* extractCut(int cut_id);
  void updateVertexList(sta::Vertex* root,
                        std::multimap<float, sta::Vertex*>& float2vertex,
                        std::multimap<sta::Vertex*, float>& vertex2float);
  void walkBackwardsToTimingEndPointsR(sta::Graph* graph,
                                       sta::Network* nwk,
                                       sta::Pin* start_pin,
                                       int depth);
  void walkForwardsToTimingEndPointsR(sta::Graph* graph,
                                      sta::Network* nwk,
                                      sta::Pin* start_pin,
                                      int depth);

  void annotateTimingConstraintsOnCut(Cut*) {}
  void remapCut(Cut*){};
  bool acceptImprovement() { return false; };

  Logger* logger_;
  std::string logfile_;
  std::string locell_;
  std::string loport_;
  std::string hicell_;
  std::string hiport_;
  std::string work_dir_name_;

  // db vars
  sta::dbSta* open_sta_;
  odb::dbDatabase* db_;
  rsz::Resizer* resizer_;
  odb::dbBlock* block_ = nullptr;

  std::string input_blif_file_name_;
  std::string output_blif_file_name_;
  std::vector<std::string> lib_file_names_;
  std::set<odb::dbInst*> path_insts_;

  Mode opt_mode_;
  bool is_area_mode_;
};

}  // namespace rmp
