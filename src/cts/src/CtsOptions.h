/////////////////////////////////////////////////////////////////////////////
//
// BSD 3-Clause License
//
// Copyright (c) 2019, The Regents of the University of California
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
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "Util.h"
#include "db.h"
#include "stt/SteinerTreeBuilder.h"
#include "utl/Logger.h"

namespace cts {

class CtsOptions
{
 public:
  CtsOptions(utl::Logger* logger, stt::SteinerTreeBuilder* sttBuildder)
      : _logger(logger), _sttBuilder(sttBuildder)
  {
  }

  void setBlockName(const std::string& blockName) { _blockName = blockName; }
  std::string getBlockName() const { return _blockName; }

  void setClockNets(const std::string& clockNets) { _clockNets = clockNets; }
  std::string getClockNets() const { return _clockNets; }
  void setRootBuffer(const std::string& buffer) { _rootBuffer = buffer; }
  std::string getRootBuffer() const { return _rootBuffer; }
  void setBufferList(std::vector<std::string> buffers)
  {
    _bufferList = buffers;
  }
  std::vector<std::string> getBufferList() const { return _bufferList; }
  void setDbUnits(int units) { _dbUnits = units; }
  int getDbUnits() const { return _dbUnits; }
  void setWireSegmentUnit(unsigned wireSegmentUnit)
  {
    _wireSegmentUnit = wireSegmentUnit;
  }
  unsigned getWireSegmentUnit() const { return _wireSegmentUnit; }
  void setPlotSolution(bool plot) { _plotSolution = plot; }
  bool getPlotSolution() const { return _plotSolution; }
  void setGuiDebug() { gui_debug_ = true; }
  bool getGuiDebug() const { return gui_debug_; }
  void setSimpleCts(bool enable) { _simpleCts = enable; }
  bool getSimpleCts() const { return _simpleCts; }
  void setSinkClustering(bool enable) { _sinkClusteringEnable = enable; }
  bool getSinkClustering() const { return _sinkClusteringEnable; }
  void setSinkClusteringUseMaxCap(bool useMaxCap)
  {
    _sinkClusteringUseMaxCap = useMaxCap;
  }
  bool getSinkClusteringUseMaxCap() const { return _sinkClusteringUseMaxCap; }
  void setNumMaxLeafSinks(unsigned numSinks) { _numMaxLeafSinks = numSinks; }
  unsigned getNumMaxLeafSinks() const { return _numMaxLeafSinks; }
  void setMaxSlew(unsigned slew) { _maxSlew = slew; }
  unsigned getMaxSlew() const { return _maxSlew; }
  void setMaxCharSlew(double slew) { _maxCharSlew = slew; }
  double getMaxCharSlew() const { return _maxCharSlew; }
  void setMaxCharCap(double cap) { _maxCharCap = cap; }
  double getMaxCharCap() const { return _maxCharCap; }
  void setCharLoadIterations(unsigned loadIterations)
  {
    _charLoadIterations = loadIterations;
  }
  unsigned getCharLoadIterations() const { return _charLoadIterations; }
  void setCharSlewIterations(unsigned slewIterations)
  {
    _charSlewIterations = slewIterations;
  }
  unsigned getCharSlewIterations() const { return _charSlewIterations; }
  void setCharWirelengthIterations(unsigned wirelengthIterations)
  {
    _charWirelengthIterations = wirelengthIterations;
  }
  unsigned getCharWirelengthIterations() const
  {
    return _charWirelengthIterations;
  }
  void setOutputPath(const std::string& path) { _outputPath = path; }
  std::string getOutputPath() const { return _outputPath; }
  void setCapInter(double cap) { _capInter = cap; }
  double getCapInter() const { return _capInter; }
  void setSlewInter(double slew) { _slewInter = slew; }
  double getSlewInter() const { return _slewInter; }
  void setClockTreeMaxDepth(unsigned depth) { _clockTreeMaxDepth = depth; }
  unsigned getClockTreeMaxDepth() const { return _clockTreeMaxDepth; }
  void setEnableFakeLutEntries(bool enable) { _enableFakeLutEntries = enable; }
  unsigned isFakeLutEntriesEnabled() const { return _enableFakeLutEntries; }
  void setForceBuffersOnLeafLevel(bool force)
  {
    _forceBuffersOnLeafLevel = force;
  }
  bool forceBuffersOnLeafLevel() const { return _forceBuffersOnLeafLevel; }
  void setWriteOnlyClockNets(bool writeOnlyClk)
  {
    _writeOnlyClockNets = writeOnlyClk;
  }
  bool writeOnlyClockNets() const { return _writeOnlyClockNets; }
  void setRunPostCtsOpt(bool run) { _runPostCtsOpt = run; }
  bool runPostCtsOpt() { return _runPostCtsOpt; }
  void setBufDistRatio(double ratio) { _bufDistRatio = ratio; }
  double getBufDistRatio() { return _bufDistRatio; }
  void setClockNetsObjs(std::vector<odb::dbNet*> nets)
  {
    _clockNetsObjs = nets;
  }
  std::vector<odb::dbNet*> getClockNetsObjs() const { return _clockNetsObjs; }
  void setMetricsFile(const std::string& metricFile)
  {
    _metricFile = metricFile;
  }
  std::string getMetricsFile() const { return _metricFile; }
  void setNumClockRoots(unsigned roots) { _clockRoots = roots; }
  long int getNumClockRoots() const { return _clockRoots; }
  void setNumClockSubnets(long int nets) { _clockSubnets = nets; }
  long int getNumClockSubnets() const { return _clockSubnets; }
  void setNumBuffersInserted(long int buffers) { _buffersInserted = buffers; }
  long int getNumBuffersInserted() const { return _buffersInserted; }
  void setNumSinks(long int sinks) { _sinks = sinks; }
  long int getNumSinks() const { return _sinks; }
  void setTreeBuffer(const std::string& buffer) { _treeBuffer = buffer; }
  std::string getTreeBuffer() const { return _treeBuffer; }
  unsigned getClusteringPower() const { return _clusteringPower; }
  void setClusteringPower(unsigned power) { _clusteringPower = power; }
  double getClusteringCapacity() const { return _clusteringCapacity; }
  void setClusteringCapacity(double capacity)
  {
    _clusteringCapacity = capacity;
  }
  double getBufferDistance() const { return _bufDistance; }
  void setBufferDistance(double distance) { _bufDistance = distance; }
  double getVertexBufferDistance() const { return _vertexBufDistance; }
  void setVertexBufferDistance(double distance)
  {
    _vertexBufDistance = distance;
  }
  bool isVertexBuffersEnabled() const { return _vertexBuffersEnable; }
  void setVertexBuffersEnabled(bool enable) { _vertexBuffersEnable = enable; }
  bool isSimpleSegmentEnabled() const { return _simpleSegmentsEnable; }
  void setSimpleSegmentsEnabled(bool enable) { _simpleSegmentsEnable = enable; }
  double getMaxDiameter() const { return _maxDiameter; }
  void setMaxDiameter(double distance)
  {
    _maxDiameter = distance;
    _sinkClusteringUseMaxCap = false;
  }
  unsigned getSizeSinkClustering() const { return _sinkClustersSize; }
  void setSizeSinkClustering(unsigned size)
  {
    _sinkClustersSize = size;
    _sinkClusteringUseMaxCap = false;
  }
  unsigned getSinkClusteringLevels() const { return _sinkClusteringLevels; }
  void setSinkClusteringLevels(unsigned levels)
  {
    _sinkClusteringLevels = levels;
  }
  unsigned getNumStaticLayers() const { return _numStaticLayers; }
  void setBalanceLevels(bool balance) { _balanceLevels = balance; }
  bool getBalanceLevels() const { return _balanceLevels; }
  void setNumStaticLayers(unsigned num) { _numStaticLayers = num; }
  void setSinkBuffer(const std::string& buffer) { _sinkBuffer = buffer; }
  void setSinkBufferMaxCap(double cap) { _sinkBufferMaxCap = cap; }
  double getSinkBufferMaxCap() const { return _sinkBufferMaxCap; }
  void setSinkBufferInputCap(double cap) { _sinkBufferInputCap = cap; }
  double getSinkBufferInputCap() const { return _sinkBufferInputCap; }
  std::string getSinkBuffer() const { return _sinkBuffer; }
  void setEnableNNC(bool enable) { _enableNNC = enable; }
  bool getEnableNNC() const { return _enableNNC; }
  utl::Logger* getLogger() { return _logger; }
  stt::SteinerTreeBuilder* getSttBuilder() { return _sttBuilder; }
  void setBufferTestInSlew(double in_slew) { buffer_in_slew = in_slew; }
  double getBufferTestInSlew() const { return buffer_in_slew; }
  void setSlewDegradationBound(double bound) { slew_degradation_bound = bound; }
  double getSlewDegradationBound() const { return slew_degradation_bound; }
  void setMergeDelayThreshold(double delay_threshold) { merge_delay_threshold = delay_threshold; }
  double getMergeDelayThreshold() const { return merge_delay_threshold; }
  void setBalanceDelayThreshold(double balance_threshold) { balance_delay_threshold = balance_threshold; }
  double getBalanceDelayThreshold() const { return balance_delay_threshold; }
  void setBufferMaxLoadCap(double load_cap) { buffer_max_load_cap = load_cap; }
  double getBufferMaxLoadCap() const { return buffer_max_load_cap; }
  unsigned getMaxFanout() const { return _max_fanout; }
  void setMaxFanout(unsigned num) { _max_fanout = num; }
  double getAvgDistance() const { return avg_distance; }
  void setAvgDistance(double distance) { avg_distance = distance; }
  void setMetricJsonFormat(bool json_format) { _metric_json_format = json_format; }
  bool getMetricJsonFormat() const { return _metric_json_format; }

 private:
  std::string _blockName = "";
  std::string _outputPath = "";
  std::string _clockNets = "";
  std::string _rootBuffer = "";
  std::string _sinkBuffer = "";
  std::string _treeBuffer = "";
  std::string _metricFile = "";
  int _dbUnits = -1;
  unsigned _wireSegmentUnit = 0;
  bool _plotSolution = false;
  bool _simpleCts = false;
  bool _sinkClusteringEnable = true;
  bool _sinkClusteringUseMaxCap = true;
  bool _simpleSegmentsEnable = false;
  bool _vertexBuffersEnable = false;
  bool gui_debug_ = false;
  double _vertexBufDistance = 240;
  double _bufDistance = 100;
  double _clusteringCapacity = 0.6;
  unsigned _clusteringPower = 4;
  unsigned _numMaxLeafSinks = 15;
  unsigned _maxSlew = 4;
  double _maxCharSlew = 0;
  double _maxCharCap = 0;
  double _sinkBufferMaxCap = 0;
  double _sinkBufferInputCap = 0;
  double _capInter = 0;
  double _slewInter = 0;
  unsigned _charWirelengthIterations = 4;
  unsigned _charLoadIterations = 34;
  unsigned _charSlewIterations = 12;
  unsigned _clockTreeMaxDepth = 100;
  bool _enableFakeLutEntries = true;
  bool _forceBuffersOnLeafLevel = true;
  bool _writeOnlyClockNets = false;
  bool _runPostCtsOpt = true;
  double _bufDistRatio = 0.1;
  long int _clockRoots = 0;
  long int _clockSubnets = 0;
  long int _buffersInserted = 0;
  long int _sinks = 0;
  double _maxDiameter = 50;
  unsigned _sinkClustersSize = 20;
  bool _balanceLevels = false;
  unsigned _sinkClusteringLevels = 0;
  unsigned _numStaticLayers = 0;
  std::vector<std::string> _bufferList;
  std::vector<odb::dbNet*> _clockNetsObjs;
  utl::Logger* _logger;
  stt::SteinerTreeBuilder* _sttBuilder;
  bool _enableNNC = false;
  float buffer_in_slew = -1;
  float slew_degradation_bound = -1;
  float merge_delay_threshold = -1;
  float balance_delay_threshold = -1;
  float buffer_max_load_cap = -1;
  //  float max_slew_limit = 5.0e-10;
  unsigned _max_fanout = 0;
  double avg_distance = 0;
  bool _metric_json_format = false;
};

}  // namespace cts
