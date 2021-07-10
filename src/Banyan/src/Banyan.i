%{

#include "openroad/OpenRoad.hh"
#include "banyan/Banyan.h"
 
%}

%include "../../Exception.i"

%inline %{


void banyan_route()
{
  auto* router = ord::OpenRoad::openRoad()->getBanyan();
  router->main(BY_CMD_ROUTE);
}

void banyan_save_txt()
{
  auto* router = ord::OpenRoad::openRoad()->getBanyan();
  router->main(BY_CMD_SAVE_TXT);
}

%} // inline
