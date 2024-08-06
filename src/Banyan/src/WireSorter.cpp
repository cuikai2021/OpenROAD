#include "WireSorter.hpp"
#include <cassert>
#include <cstdlib>
#include <string>
#include <algorithm>
#include <map>
#include "banyan_db.pb.h"
using std::map;
namespace banyan {

void WireSorter::printSegType(const WireSegment& seg)const{
  if(seg.has_rect()){
    logger_->info(utl::BY, 1, "type: {}", "rect");
  }
  if(seg.has_line()){
    logger_->info(utl::BY, 1, "type: {}", "line");
  }
  if(seg.has_via()){
    logger_->info(utl::BY, 1, "type: {}", "via");
  }
}

int WireSorter::getSegType(const WireSegment& seg){
  if(seg.has_rect()){
    return 1;
  }
  if(seg.has_line()){
    return 2;
  }
  if(seg.has_via()){
    return 3;
  }
  abort();
}

void WireSorter::printSeg(const WireSegment& seg)const{
  if(seg.has_rect()){
    logger_->info(utl::BY, 1, "type: Rect");
  }
  else if(seg.has_line()){
    logger_->info(utl::BY, 1, "type: Line {} {} {} {}",seg.line().start().point().x(),
    seg.line().start().point().y(),seg.line().end().point().x(),seg.line().end().point().y());
  }
  else if(seg.has_via()){
    logger_->info(utl::BY, 1, "type: Via {} {} {} ",seg.via().vianame(),seg.via().point().x(),seg.via().point().y());
  }
  else logger_->info(utl::BY, 1, "type: null");
}

bool WireSorter::cmpPoint(const Point& point_a,const Point& point_b){
  return point_a.x()<point_b.x() || 
    point_a.x() == point_b.x() && point_a.y()<point_b.y() ;
}

bool WireSorter::equalPoint(const Point& point_a,const Point& point_b){
  return point_a.x() == point_b.x() && point_a.y() == point_b.y();
}

bool WireSorter::cmpRect(const WireSegment& seg_a,const WireSegment& seg_b){
  if (!equalPoint(seg_a.rect().point(), seg_b.rect().point()))
    return cmpPoint(seg_a.rect().point(), seg_b.rect().point());
  if (!equalPoint(seg_a.rect().rect().lowleft(), seg_b.rect().rect().lowleft()))
    return cmpPoint(seg_a.rect().rect().lowleft(), seg_b.rect().rect().lowleft());
  return cmpPoint(seg_a.rect().rect().upright(), seg_b.rect().rect().upright());
}

bool WireSorter::cmpLine(const WireSegment& sega,const WireSegment& segb){
  if (!equalPoint(sega.line().start().point(), segb.line().start().point())) 
    return cmpPoint(sega.line().start().point(), segb.line().start().point());
  return cmpPoint(sega.line().end().point(), segb.line().end().point());
}

bool WireSorter::cmpVia(const WireSegment& sega,const WireSegment& segb){
  if (sega.via().vianame()!=segb.via().vianame()) return sega.via().vianame() < segb.via().vianame();
  return cmpPoint(sega.via().point(), segb.via().point());
}
bool WireSorter::cmpSegment(const WireSegment &sega,const WireSegment &segb){
  int typeA = getSegType(sega);
  int typeB = getSegType(segb);
  if (typeA != typeB){
    return (typeA < typeB);
  }
  if (typeA == 1) return cmpRect(sega, segb);
  if (typeA == 2) return cmpLine(sega, segb);
  if (typeA == 3) return cmpVia(sega, segb);
  abort();
}
void WireSorter::assertOrdered(google::protobuf::RepeatedPtrField<::banyan::Wire>& wires){
  for (auto& line: wires){
    auto& segs = line.segments();
    for (int i = 1; i<segs.size(); ++i){
      assert(!cmpSegment(segs[i],segs[i - 1]));
    }
  }
  for (int i = 1; i < wires.size(); i++){
    assert(wires[i - 1].layername() < wires[i].layername());
  }
}

void WireSorter::algoSort(google::protobuf::RepeatedPtrField<::banyan::Wire>& wires) {
  map<std::string,Wire*> mp;
  for (auto& line : wires){
    auto par = mp[line.layername()];
    if (par==nullptr){
      Wire* new_wire = new Wire(line);
      mp[line.layername()] = new_wire;
    }
    else{
      par->MergeFrom(line);
    }
  }

  wires.Clear();
  for (auto iter:mp){
    auto segs = iter.second->mutable_segments();
    std::sort(segs->begin(),segs->end(),cmpSegment);
    wires.AddAllocated(iter.second);
  }
}

void WireSorter::wireOrdering(banyan::Net &net){
  google::protobuf::RepeatedPtrField<::banyan::Wire>* wires = net.mutable_wires();
  google::protobuf::RepeatedPtrField<::banyan::Wire>* pinwires = net.mutable_pinaccesswires();
  if(!wires->empty()){
    algoSort(*wires);
    assertOrdered(*wires);
  }
  if(!pinwires->empty()){
    algoSort(*pinwires);
    assertOrdered(*pinwires);
  }
}
void WireSorter::designWireOrdering(Design &design){
  for (auto& net: *design.mutable_nets()){
    wireOrdering(net);
  }
}

}//banyan end