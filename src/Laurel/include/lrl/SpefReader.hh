#pragma once

#include "parasitics/SpefReader.hh"
#include "parasitics/SpefReaderPvt.hh"
#include "sta/Sta.hh"

// Global namespace.
// redirect YY input macro
#undef YY_INPUT
#define YY_INPUT(buf, result, max_size) \
  sta::lrl_spef_reader->getChars(buf, result, max_size)

int LrlSpefParse_error(const char* msg);

namespace sta {
class SpefReaderFct
{
 public:
  SpefReaderFct(const char* filename,
                gzFile stream,
                Instance* instance,
                ParasiticAnalysisPt* ap,
                bool increment,
                bool pin_cap_included,
                bool keep_coupling_caps,
                float coupling_cap_factor,
                ReducedParasiticType reduce_to,
                bool delete_after_reduce,
                const OperatingConditions* op_cond,
                const Corner* corner,
                const MinMax* cnst_min_max,
                bool quiet,
                Report* report,
                Network* network,
                Parasitics* parasitics,
                float scale_factor);
  virtual ~SpefReaderFct();
  char divider() const { return divider_; }
  void setDivider(char divider);
  char delimiter() const { return delimiter_; }
  void setDelimiter(char delimiter);
  void incrLine();
  int line() const { return line_; }
  const char* filename() const { return filename_; }
  // flex YY_INPUT yy_n_chars arg changed definition from int to size_t,
  // so provide both forms.
  void getChars(char* buf, int& result, size_t max_size);
  void getChars(char* buf, size_t& result, size_t max_size);
  // Translate from spf/spef namespace to sta namespace.
  char* translated(const char* token);
  void warn(int id, const char* fmt, ...) __attribute__((format(printf, 3, 4)));
  void setBusBrackets(char left, char right);
  void setTimeScale(float scale, const char* units);
  void setCapScale(float scale, const char* units);
  void setResScale(float scale, const char* units);
  void setInductScale(float scale, const char* units);
  void makeNameMapEntry(char* index, char* name);
  char* nameMapLookup(char* index);
  void setDesignFlow(StringSeq* flow_keys);
  Pin* findPin(char* name);
  Net* findNet(char* name);
  void rspfBegin(Net* net, SpefTriple* total_cap);
  void rspfFinish();
  void rspfDrvrBegin(Pin* drvr_pin, SpefRspfPi* pi);
  void rspfLoad(Pin* load_pin, SpefTriple* rc);
  void rspfDrvrFinish();
  void dspfBegin(Net* net, SpefTriple* total_cap);
  void dspfFinish();
  void makeCapacitor(int id, char* node_name, SpefTriple* cap);
  void makeCapacitor(int id,
                     char* node_name1,
                     char* node_name2,
                     SpefTriple* cap);
  void makeResistor(int id,
                    char* node_name1,
                    char* node_name2,
                    SpefTriple* res);
  PortDirection* portDirection(char* spef_dir);

 protected:
  Pin* findPinRelative(const char* name);
  Pin* findPortPinRelative(const char* name);
  Net* findNetRelative(const char* name);
  Instance* findInstanceRelative(const char* name);
  void makeCouplingCap(int id, char* node_name1, char* node_name2, float cap);
  ParasiticNode* findParasiticNode(char* name);
  void findParasiticNode(char* name,
                         ParasiticNode*& node,
                         Net*& ext_net,
                         int& ext_node_id,
                         Pin*& ext_pin);

  const char* filename_;
  Instance* instance_;
  const ParasiticAnalysisPt* ap_;
  bool increment_;
  bool pin_cap_included_;
  bool keep_coupling_caps_;
  ReducedParasiticType reduce_to_;
  bool delete_after_reduce_;
  const OperatingConditions* op_cond_;
  const Corner* corner_;
  const MinMax* cnst_min_max_;
  // Normally no need to keep device names.
  bool keep_device_names_;
  bool quiet_;
  gzFile stream_;
  int line_;
  char divider_;
  char delimiter_;
  char bus_brkt_left_;
  char bus_brkt_right_;
  Net* net_;
  Report* report_;
  Network* network_;
  Parasitics* parasitics_;

  int triple_index_;
  float time_scale_;
  float cap_scale_;
  float res_scale_;
  float induct_scale_;
  SpefNameMap name_map_;
  StringSeq* design_flow_;
  Parasitic* parasitic_;

  float scale_factor_;  // additional
};

bool readSpefFileFct(const char* filename,
                     Instance* instance,
                     ParasiticAnalysisPt* ap,
                     bool increment,
                     bool pin_cap_included,
                     bool keep_coupling_caps,
                     float coupling_cap_factor,
                     ReducedParasiticType reduce_to,
                     bool delete_after_reduce,
                     const OperatingConditions* op_cond,
                     const Corner* corner,
                     const MinMax* cnst_min_max,
                     bool quiet,
                     Report* report,
                     Network* network,
                     Parasitics* parasitics,
                     float factor);

bool readSpefFct(const char* filename,
                 Instance* instance,
                 const Corner* corner,
                 const MinMaxAll* min_max,
                 bool increment,
                 bool pin_cap_included,
                 bool keep_coupling_caps,
                 float coupling_cap_factor,
                 ReducedParasiticType reduce_to,
                 bool delete_after_reduce,
                 bool quiet,
                 float factor,
                 Sta* sta);

extern SpefReaderFct* lrl_spef_reader;

}  // namespace sta