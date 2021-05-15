%{

#include "openroad/OpenRoad.hh"
#include "banyan/Banyan.h"
 
%}

%include "../../Exception.i"

%inline %{

void banyan_cmd()
{
  auto* router = ord::OpenRoad::openRoad()->getBanyan();
  router->main();
}

%} // inline
