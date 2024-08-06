#pragma once

#include "banyan_db.pb.h"
#include "banyan_instance_movement.pb.h"
#include "utl/Logger.h"


namespace banyan {
//use this class to order seg in net
//ensure a unique answer
class WireSorter final{
 public:
  WireSorter(utl::Logger* logger):logger_(logger){
  }
  static void wireOrdering(banyan::Net &net);
  static void designWireOrdering(Design &design);
  static void assertOrdered(google::protobuf::RepeatedPtrField<::banyan::Wire>& wires);
 private:
  utl::Logger* logger_;
  void printSeg(const WireSegment& seg)const;
  static int getSegType(const WireSegment& seg_a);
  static void algoSort(google::protobuf::RepeatedPtrField<::banyan::Wire>& wires);
  void printSegType(const WireSegment& seg_a)const;
  static bool equalPoint(const Point& point_a,const Point& point_b);
  static bool cmpPoint(const Point& point_a,const Point& point_b);
  static bool cmpSegment(const WireSegment& seg_a,const WireSegment& seg_b);
  static bool cmpVia(const WireSegment& seg_a,const WireSegment& seg_b);
  static bool cmpRect(const WireSegment& seg_a,const WireSegment& seg_b);
  static bool cmpLine(const WireSegment& seg_a,const WireSegment& seg_b);
};

}//end banyan