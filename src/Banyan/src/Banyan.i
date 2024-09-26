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

void banyanClearWires(bool excludeClock, bool excludeSignal)
{
    ord::OpenRoad::openRoad()->getBanyan()->clearWires(excludeClock, excludeSignal);
}

void regression_RC_cmd(const char* spef,
                       const char* setRC_tcl,
                       const char* plot_file_prefix,
                       const char* corner_name)
{
    ord::OpenRoad::openRoad()->getBanyan()->regression_RC(
        spef, setRC_tcl, plot_file_prefix, corner_name);
}

void loadDB(std::string dbfile)
{
    ord::OpenRoad::openRoad()->getBanyan()->loaddb(dbfile);
}

void loadDef(std::string deffile)
{
    ord::OpenRoad::openRoad()->getBanyan()->loaddef(deffile);
}
void diffDefs(std::string deffileA, std::string deffileB, bool exceptWires)
{
    ord::OpenRoad::openRoad()->getBanyan()->diffdefs(deffileA, deffileB, exceptWires);
}

}

%} // inline
