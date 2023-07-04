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

void banyan(std::vector<std::string> args)
{
    int status = ord::OpenRoad::openRoad()->getBanyan()->main(args, true);
    if (status == 0) {
      ord::OpenRoad::openRoad()->getBanyan()->mergeWireSegment();
    }
}


void banyan_clear_wires()
{
    ord::OpenRoad::openRoad()->getBanyan()->clearWires();
}

void mergeWireSegment()
{
    ord::OpenRoad::openRoad()->getBanyan()->mergeWireSegment();
}

void m1GridlessMaze()
{
    ord::OpenRoad::openRoad()->getBanyan()->m1GridlessMaze();
}

void banyanRunGui()
{
    ord::OpenRoad::openRoad()->getBanyan()->runGui();
}
void drcPatch()
{
    ord::OpenRoad::openRoad()->getBanyan()->drcPatch();
}

}

%} // inline




