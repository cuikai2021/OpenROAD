%{

#include "ord/OpenRoad.hh"
#include "banyan/Banyan.h"
 
%}

%include "../../Exception.i"

%inline %{

void banyan_route(
  const char *log_level,
  const char* debug_output,
  bool debug_save_txt,
  const char* max_via_stack,
  const char* min_layer,
  const char* max_layer,
  bool cts)
{
  std::map<std::string, std::string> options;
  options["log_level"] = log_level;
  options["debug_output"] = debug_output;
  if (debug_save_txt) options["debug_save_txt"] = "";
  options["max_via_stack"] = max_via_stack;
  options["min_layer"] = min_layer;
  options["max_layer"] = max_layer;
  if (cts) options["cts"] = "";

  ord::OpenRoad::openRoad()->getBanyan()->main(options);
}

%} // inline
