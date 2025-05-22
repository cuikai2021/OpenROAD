%{
#include "ord/OpenRoad.hh"
#include "banyan/Banyan.h"

using std::string;
using std::vector;
%}

%include "../../Exception.i"

%typemap(in) std::vector<std::string> {
  std::vector<std::string> args;
  int var_size;
  Tcl_Obj** vars_objs;
  if (Tcl_ListObjGetElements(interp, $input, &var_size, &vars_objs) == TCL_OK) {
    for (int i = 0; i < var_size; i++) {
      std::string var = Tcl_GetString(vars_objs[i]);
      args.push_back(var);
    }
  }
  $1 = args;
}

%typemap(in) std::string {
  char *cstr = Tcl_GetStringFromObj($input, 0);
  std::string str = cstr;
  $1 = str;
}

%inline %{

namespace banyan {

void banyan(std::vector<std::string> args, bool updateDB)
{
    ord::OpenRoad::openRoad()->getBanyan()->main(args, updateDB);
}

void clearWires(bool excludeClock, bool excludeSignal)
{
    ord::OpenRoad::openRoad()->getBanyan()->clearWires(excludeClock, excludeSignal);
}

void loadDef(std::string deffile)
{
    ord::OpenRoad::openRoad()->getBanyan()->loaddef(deffile);
}

void regression_RC_cmd(const char* spef,
                       const char* setRC_tcl,
                       const char* plot_file_prefix,
                       const char* corner_name)
{
    ord::OpenRoad::openRoad()->getBanyan()->regression_RC(
        spef, setRC_tcl, plot_file_prefix, corner_name);
}

void optimizeCongestion(std::string max_layer)
{
    ord::OpenRoad::openRoad()->getBanyan()->optimizeCongestion(max_layer);
}

void add_placement_blockage(std::vector<std::string> args)
{
    ord::OpenRoad::openRoad()->getBanyan()->add_placement_blockage(args);
}

void loadRoutingCongestion(std::string path)
{
   ord::OpenRoad::openRoad()->getBanyan()->loadRoutingCongestion(path);
}

}

void remove_placement_blockage()
{
    ord::OpenRoad::openRoad()->getBanyan()->remove_placement_blockage();
}

void newIncrementalRoute(std::string max_layer, std::string work_dir, std::string init_db)
{
    ord::OpenRoad::openRoad()->getBanyan()->newIncrementalRoute(max_layer, work_dir, init_db);
}

int incrementalRoute()
{
    return ord::OpenRoad::openRoad()->getBanyan()->incrementalRoute();
}

void deleteIncrementalRoute()
{
    ord::OpenRoad::openRoad()->getBanyan()->deleteIncrementalRoute();
}

int repair_antennas(odb::dbMTerm* diode_mterm,std::string history_db,std::string min_layer,std::string  max_layer,const int max_iter ) 
{
   return ord::OpenRoad::openRoad()->getBanyan()->repair_antennas(diode_mterm,history_db,min_layer,max_layer,max_iter);
}
%} // inline
