
///////////////////////////////////////////////////////////////////////////////
// BSD 3-Clause License
//
// Copyright (c) 2019, Nefelus Inc
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

#include "dbUtil.h"
#include "rcx/extMeasureRC.h"
#include "rcx/extRCap.h"
#include "rcx/extSegment.h"
#include "utl/Logger.h"

#ifdef HI_ACC_1
#define FRINGE_UP_DOWN
#endif
// #define CHECK_SAME_NET
// #define MIN_FOR_LOOPS

namespace rcx {

using utl::RCX;

using namespace odb;

extDistRC* extMeasureRC::computeUnderRC(uint len) 
{
  extDistRC* rcUnit = NULL;

  for (uint ii = 0; ii < _metRCTable.getCnt(); ii++) {
    extMetRCTable* rcModel = _metRCTable.get(ii);
/*
    rcUnit = getUnderRC(rcModel);
    
    double frw= 0;
  //  if (_diagResDist>0)
      frw= getUnderRC_weightedFringe(rcModel, _width, _met, _overMet, _dist, _diagResDist);
    if (_dist<0 || _diagResDist<0)
      addRC_new(_tmpRC, len, ii, _dist>0 || _diagResDist>0);
    else
      addRC(rcUnit, len, ii, frw);

*/
    if (_dist<0 && _diagResDist<0) // openEnded both sides
    {
      rcUnit = getUnderRC(rcModel);
      double inf_cc= rcUnit->_coupling;
      addRC(rcUnit, 2*len, ii, inf_cc);
    }
    else if (_dist<0 || _diagResDist<0) // openEnded
    { 
      float openFr= getOURC_Open(rcModel, _width, _met, _underMet, _overMet, _dist, _diagResDist);
      addRC_new(_tmpRC, len, ii, true);
    }
    else if ((_dist>0 && _diagResDist>0) && _diagResDist==200 && _dist!=_diagResDist) // model1
    {
      float FR= getOU_over1(rcModel, len, _diagResDist, _dist);
      addRC_new(_tmpRC, len, ii, true);
      _rc[ii]->_fringeW += _tmpRC->_fringeW * len;
    } 
    else {
      rcUnit = getUnderRC(rcModel);
      double frw= getUnderRC_weightedFringe(rcModel, _width, _met, _overMet, _dist, _diagResDist);
      addRC(rcUnit, len, ii, frw);
    }
  }
  return rcUnit;
}

extDistRC* extMeasureRC::addRC(extDistRC* rcUnit, uint len, uint jj, double frw) {
  if (rcUnit == NULL)
    return NULL;
  int dbUnit = _extMain->_block->getDbUnitsPerMicron();
  extDistRC* tmp= _rc[jj];
  if (IsDebugNet()) {
    // rcUnit->printDebugRC(_met, _overMet, _underMet, _width, _dist, dbUnit, logger_);
    printDebugRC(_debugFP, tmp, _met, _overMet, _underMet, _width, _dist, len, "PREV ");
    printDebugRC(stdout, tmp, _met, _overMet, _underMet, _width, _dist, len, "PREV ");
    printDebugRC(_debugFP, rcUnit, _met, _overMet, _underMet, _width, _dist, len, "Rule: ");
    printDebugRC(stdout, rcUnit, _met, _overMet, _underMet, _width, _dist, len, "Rule: ");
  }
  if (_sameNetFlag) {  // TO OPTIMIZE
    _rc[jj]->_fringe += 0.5 * rcUnit->_fringe * len;
    _rc[jj]->_fringeW += 0.5 * frw * len;
  } else {
    _rc[jj]->_fringe += rcUnit->_fringe * len;
    _rc[jj]->_fringeW += frw * len;

    if (_dist > 0)  // dist based
      _rc[jj]->_coupling += rcUnit->_coupling * len;
  }

  _rc[jj]->_res += rcUnit->_res * len;
  if (IsDebugNet()) {
    // _rc[jj]->printDebugRC_sum(len, dbUnit, logger_);
    printDebugRC(_debugFP, tmp, _met, _overMet, _underMet, _width, _dist, len, "Update ");
    printDebugRC(stdout, tmp, _met, _overMet, _underMet, _width, _dist, len, "Update ");
  }
  return rcUnit;
}

extDistRC* extMeasureRC::addRC_new(extDistRC* rcUnit, uint len, uint jj, bool addCC) {
  if (rcUnit == NULL)
    return NULL;
  int dbUnit = _extMain->_block->getDbUnitsPerMicron();

  extDistRC* tmp= _rc[jj];
  if (IsDebugNet()) {
    // rcUnit->printDebugRC(_met, _overMet, _underMet, _width, _dist, dbUnit, logger_);
    printDebugRC(_debugFP, tmp, _met, _overMet, _underMet, _width, _dist, len, "PREV ");
    printDebugRC(stdout, tmp, _met, _overMet, _underMet, _width, _dist, len, "PREV ");
    printDebugRC(_debugFP, rcUnit, _met, _overMet, _underMet, _width, _dist, len, "Rule: ");
    printDebugRC(stdout, rcUnit, _met, _overMet, _underMet, _width, _dist, len, "Rule: ");
  }

  if (_sameNetFlag) {  // TO OPTIMIZE
    _rc[jj]->_fringe += 0.5 * rcUnit->_fringe * len;
  } else {
    _rc[jj]->_fringe += rcUnit->_fringe * len;

    if (addCC)  // dist based
      _rc[jj]->_coupling += rcUnit->_coupling * len;
  }

  _rc[jj]->_res += rcUnit->_res * len;
  if (IsDebugNet()) {
    // _rc[jj]->printDebugRC_sum(len, dbUnit, logger_);
    printDebugRC(_debugFP, tmp, _met, _overMet, _underMet, _width, _dist, len, "Update ");
    printDebugRC(stdout, tmp, _met, _overMet, _underMet, _width, _dist, len, "Update ");
  }
  return rcUnit;
}

extDistRC* extMeasureRC::computeOverUnderRC(uint len) {
  extDistRC* rcUnit = NULL;

  for (uint ii = 0; ii < _metRCTable.getCnt(); ii++) {
    extMetRCTable* rcModel = _metRCTable.get(ii);

    if (_dist<0 && _diagResDist<0) // openEnded both sides
    {
      rcUnit = getOverUnderRC(rcModel);
      double inf_cc= rcUnit->_coupling;
      addRC(rcUnit, 2*len, ii, inf_cc);
    }
    else if (_dist<0 || _diagResDist<0) {
      float openFr= getOURC_Open(rcModel, _width, _met, _underMet, _overMet, _dist, _diagResDist);
      addRC_new(_tmpRC, len, ii, _dist>0 || _diagResDist>0);
    }
    else if ((_dist>0 && _diagResDist>0) && _diagResDist==200 && _dist!=_diagResDist) {
      float FR= getOU_over1(rcModel, len, _diagResDist, _dist);
      addRC_new(_tmpRC, len, ii, true);
      _rc[ii]->_fringeW += _tmpRC->_fringeW * len;
    } else {
      rcUnit = getOverUnderRC(rcModel);
      double frw= getOverUnderRC_weightedFringe(rcModel, _width, _met, _underMet, _overMet, _dist, _diagResDist);
      addRC(rcUnit, len, ii, frw);
    }
  }
  return rcUnit;
}
extDistRC* extMeasureRC::computeOverRC(uint len) 
{
  extDistRC* rcUnit = NULL;

  for (uint ii = 0; ii < _metRCTable.getCnt(); ii++) 
  {
    extMetRCTable* rcModel = _metRCTable.get(ii);

    if (_dist<0 && _diagResDist<0) // openEnded both sides
    {
      rcUnit = getOverRC(rcModel);
      double inf_cc= rcUnit->_coupling;
      addRC(rcUnit, 2*len, ii, inf_cc);
    }
    else if (_dist<0 || _diagResDist<0) // openEnded
    { 
      float openFr= getOverRC_Open(rcModel, _width, _met, _underMet, _dist, _diagResDist);
      addRC_new(_tmpRC, len, ii, _dist>0 || _diagResDist>0);
    }
    else if ((_dist>0 && _diagResDist>0) && _diagResDist==200 && _dist!=_diagResDist) // model1
    {
      // float FR= getOver_over1(rcModel, _width, _met, _underMet, _diagResDist, _dist, len);
      float FR= getOU_over1(rcModel, len, _diagResDist, _dist);
      addRC_new(_tmpRC, len, ii, true);
      _rc[ii]->_fringeW += _tmpRC->_fringeW * len;
    } 
    else {
      rcUnit = getOverRC(rcModel);
      double frw= getOverR_weightedFringe(rcModel, _width, _met, _underMet, _dist, _diagResDist);
      addRC(rcUnit, len, ii, frw);
    }
  }
  return rcUnit;
}



}  // namespace rcx
