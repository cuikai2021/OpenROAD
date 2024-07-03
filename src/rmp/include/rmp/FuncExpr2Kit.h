#pragma once
#include <sta/FuncExpr.hh>
#include <sta/StringUtil.hh>
#include <sta/Liberty.hh>
#include <sta/Network.hh>


namespace rmp {

  class FuncExpr2Kit {
  public:    
    FuncExpr2Kit(sta::FuncExpr* fe, int& var_count, unsigned *&kit_tables,
                 std::map<int,std::string>& ix2var
                 );
    void RecursivelyEvaluate(sta::FuncExpr* fe, unsigned*);

  private:
    std::map<sta::LibertyPort*, unsigned*> kit_vars_; //pointer (not name) addressed.
    std::map<int, sta::LibertyPort*>  lib_ports_;
    int var_count_;
  };
  
}
