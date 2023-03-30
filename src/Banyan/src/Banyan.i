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

%inline %{
namespace banyan {

void banyan(std::vector<std::string> args)
{
    int status = ord::OpenRoad::openRoad()->getBanyan()->main(args, true);
    if (status == 0) {
      ord::OpenRoad::openRoad()->getBanyan()->mergeWireSegment();
      ord::OpenRoad::openRoad()->getBanyan()->drcPatch();
    }
}

void mergeWireSegment()
{
    ord::OpenRoad::openRoad()->getBanyan()->mergeWireSegment();
}

void m1GridlessMaze()
{
    ord::OpenRoad::openRoad()->getBanyan()->m1GridlessMaze();
}

void instMovement(std::vector<std::string> args)
{
    ord::OpenRoad::openRoad()->getBanyan()->instanceMovement(args);
}

void drcPatch()
{
    ord::OpenRoad::openRoad()->getBanyan()->drcPatch();
}

}

%} // inline




