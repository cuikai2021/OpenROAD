#include "RCTable.h"
#include <utility>
#include "FastRoute.h"
#include "Grid.h"



namespace grt {

namespace RCTable {

// Returns {R(Ohm), Cap(F)} pair
std::pair<double, double>
GCellRC::getSegRC(GSegment seg,
                  sta::Corner* corner,
                  int wire_length_dbu,
                  int wire_width_dbu,
                  bool dbg) {
  if (seg.isVia()) {
    return table_[corner->index()]->getViaUnitRC();
  }
  auto layer_id = seg.init_layer;
  auto layer = tech_->findRoutingLayer(layer_id);
  if (wire_width_dbu == 0) {
    wire_width_dbu = layer->getWidth();
  }
  auto wire_length = block_->dbuToMicrons(wire_length_dbu);
  auto wire_width = block_->dbuToMicrons(wire_width_dbu);
  const double micronsToNM = 1e3;
  double res = 0.0;
  double cap = 0.0;
  const auto& fast_route = grouter_->fastroute();

  int x1 = (seg.init_x - grid_->getXMin()) / grid_->getTileSize();
  int y1 = (seg.init_y - grid_->getYMin()) / grid_->getTileSize();
  
  int x2 = (seg.final_x - grid_->getXMin()) / grid_->getTileSize();
  int y2 = (seg.final_y - grid_->getYMin()) / grid_->getTileSize();
  bool is_hor = (seg.init_y == seg.final_y);
  bool layer_is_hor =
    tech_->findRoutingLayer(seg.init_layer)->getDirection().getValue() == odb::dbTechLayerDir::HORIZONTAL;

  // The last gcell is oversized and includes space that the above
  // calculation doesn't represent so correct it:
  // x2 = std::min(x2, grid_->getXGrids() - 1);
  // y2 = std::min(y2, grid_->getYGrids() - 1);

  int layer_idx = layer_id - 1;
  auto&& edges_3D = layer_is_hor ? fast_route->getHorizontalEdges3D() : fast_route->getVerticalEdges3D();
  std::vector<RCValue> edges_rc;
  
  for (int idx = (is_hor ? x1 : y1), end = (is_hor ? x2 : y2); idx <= end; idx++) {
    int yy = y2, xx = x2;
    is_hor ? (xx = idx) : (yy = idx);
    xx = std::min(xx, grid_->getXGrids() - 1);
    yy = std::min(yy, grid_->getYGrids() - 1);
    auto g_usage = edges_3D[layer_idx][yy][xx].usage;
    auto g_cap = edges_3D[layer_idx][yy][xx].cap;
    auto g_red = edges_3D[layer_idx][yy][xx].red;
    auto pitch = block_->dbuToMicrons(layer->getPitch());
    auto width = block_->dbuToMicrons(layer->getWidth());
    double pitch_count = 0.0;
    if (g_cap == 0) {
      pitch_count = 1.0 / ((1.0 * g_usage + g_red) / (g_red));
    } else {
      if (g_usage != 0) {
        pitch_count = 1.0 / (1.0 * g_usage / g_cap);
      } else {
        // g_usage not apply to this wire, then assume this usage be 1 
        pitch_count = 1.0 / (1.0 * 1 / g_cap);
      }
    }

    std::vector<double> densities(tech_->getRoutingLayerCount());
    auto s = layer_idx % 2;
    for (int layer_idx = s; layer_idx < grouter_->getMaxRoutingLayer(); layer_idx += 2) {
      densities[layer_idx] = 1.0 * (edges_3D[layer_idx][yy][xx].usage + edges_3D[layer_idx][yy][xx].red)
                              / (edges_3D[layer_idx][yy][xx].cap + edges_3D[layer_idx][yy][xx].red);
      if (edges_3D[layer_idx][yy][xx].cap + edges_3D[layer_idx][yy][xx].red != grouter_->getGCellTracks()
        && !(yy == grid_->getYGrids() - 1 || xx == grid_->getXGrids() - 1)) {
        logger_->info(utl::GRT, 1054, "bad edges {} {} {} {}", edges_3D[layer_idx][yy][xx].cap + edges_3D[layer_idx][yy][xx].red, edges_3D[layer_idx][yy][xx].red, edges_3D[layer_idx][yy][xx].usage, edges_3D[layer_idx][yy][xx].cap);
        logger_->info(utl::GRT, 1055, "bad edges {} {} {} {}", xx, yy, grid_->getXGrids(), grid_->getYGrids());
      }
    }
    s = (layer_idx + 1) % 2;
    for (int layer_idx = s; layer_idx < grouter_->getMaxRoutingLayer(); layer_idx += 2) {
      auto&& edges_3D = !layer_is_hor ? fast_route->getHorizontalEdges3D() : fast_route->getVerticalEdges3D();
      
      densities[layer_idx] = 1.0 * (edges_3D[layer_idx][yy][xx].usage + edges_3D[layer_idx][yy][xx].red)
                              / (edges_3D[layer_idx][yy][xx].cap + edges_3D[layer_idx][yy][xx].red);
      if (edges_3D[layer_idx][yy][xx].cap + edges_3D[layer_idx][yy][xx].red != grouter_->getGCellTracks()
        && !(yy == grid_->getYGrids() - 1 || xx == grid_->getXGrids() - 1)) {
        logger_->info(utl::GRT, 1056, "bad edges {} {} {} {}", edges_3D[layer_idx][yy][xx].cap + edges_3D[layer_idx][yy][xx].red, edges_3D[layer_idx][yy][xx].red, edges_3D[layer_idx][yy][xx].usage, edges_3D[layer_idx][yy][xx].cap);
        logger_->info(utl::GRT, 1057, "bad edges {} {} {} {}", xx, yy, grid_->getXGrids(), grid_->getYGrids());
      }
    }
    // TODO : check power density
    for (int layer_idx = grouter_->getMaxRoutingLayer() + 2; layer_idx < tech_->getRoutingLayerCount(); layer_idx++) {
      densities[layer_idx] = 1.0;
    }
    if (dbg) {
      logger_->report("gcell pos {} {} usage {} cap {} red {} pitch {} wid {} pit siz {}", xx, yy, g_usage, g_cap, g_red, pitch, width, pitch_count);
    }
    edges_rc.push_back(table_[corner->index()]->getWireUnitRC(layer_idx, micronsToNM * ((pitch * pitch_count) - width), micronsToNM * wire_width, densities, dbg));
  }
  double length_sum = 0.0;
  for (int idx = (is_hor ? x1 : y1), end = (is_hor ? x2 : y2), rc_idx = 0; idx <= end; idx++, rc_idx++) {
    int yy = y2, xx = x2;
    is_hor ? (xx = idx) : (yy = idx);
    auto getLengthOnGcell = [&]() {
      int bloat_multi_x = 1, bloat_multi_y = 1;
      if (is_hor) {
        bloat_multi_x = idx == end ? 2 : 1;
        bloat_multi_y = 2;
      } else {
        bloat_multi_y = idx == end ? 2 : 1;
        bloat_multi_x = 2;
      }
      odb::Rect seg_rect(seg.init_x, seg.init_y, seg.final_x, seg.final_y),
                gcell_rect(xx * grid_->getTileSize() + grid_->getXMin(),
                           yy * grid_->getTileSize() + grid_->getYMin(),
                           (xx + bloat_multi_x) * grid_->getTileSize() + grid_->getXMin(),
                           (yy + bloat_multi_y) * grid_->getTileSize() + grid_->getYMin());
      odb::Rect inter_segs = seg_rect.intersect(gcell_rect);
      return is_hor ? inter_segs.dx() : inter_segs.dy();
      // TODO : fix me
      // return inter_segs.maxDXDY();
    };

    auto unit_rc = edges_rc[rc_idx]; 
    auto length = block_->dbuToMicrons(getLengthOnGcell());
    if (rc_idx > 0) {
      unit_rc.merge(edges_rc[rc_idx-1]);
    }
    auto r = micronsToNM * length * unit_rc.unitRes();
    auto c = micronsToNM * length * unit_rc.unitCap();

    length_sum += length;
    if (dbg) {
      logger_->report("seg info length {} um unit_rc {} {} rc {} {}", length, unit_rc.unitRes(), unit_rc.unitCap(), r, c);
    }
    res += r;
    cap += c;
  }
  if (dbg) {
    logger_->report("wire info {} {} {} {} la {}->{} length {} um rc {} {}", seg.init_x, seg.init_y, seg.final_x, seg.final_y, seg.init_layer, seg.final_layer, wire_length, res, cap);
    logger_->report("grid max {} {}\n", grid_->getXGrids(), grid_->getYGrids());
  }
  if (fabs(length_sum - wire_length) >= 1e-6) {
    logger_->report("bad length cal %lf %lf {} {} {} {}", length_sum, wire_length, seg.init_x, seg.init_y, seg.final_x, seg.final_y);
  }

  // FF to F
  cap *= 1e-15;
  return {res, cap};
}

void 
RCTable::readRCFromRCXModel(const char* file_name) {
  std::ifstream file(file_name);
  if (file) {
    std::string line;
    std::vector<std::string> words;

    bool parse_head = true;
    bool parse_cap = false;

    int layer_id = 0;

    while (std::getline(file, line)) {
      std::stringstream ss(line);
      std::string s;
      while(ss >> s) {
        words.push_back(s);
      }

      if (parse_head) {
        switch (words.size()) {
          case 3:
            if (words[0] == "Metal") {
              if (words[2] == "OVER" || words[2] == "UNDER" || words[2] == "OVERUNDER") {
                parse_cap = true;
              } else {
                parse_cap = false;
              }
            }
            break;
          case 4:
            if (words[0] == "Metal") {
              layer_id = std::stoi(words[1]);
              if (words[2] == "RESOVER" && words[3] == "0") {
                parseRes(file, layer_id);
              }
              // use over lower metal as golden
              if (parse_cap
                  && (words[2] == "OVER" || words[2] == "UNDER")) {
                auto over = std::stoi(words[3]);
                auto under = std::stoi(words[3]);
                if (words[2] == "OVER") {
                  under = tech_->getRoutingLayerCount() + 1;
                } else {
                  over = 0;
                }
                parseCap(file, layer_id, over, under);
              }
            }
            break;
          case 6:
            if (words[0] == "Metal") {
              if (parse_cap
                  && words[2] == "OVER" && words[4] == "UNDER") {
                auto over = std::stoi(words[3]),
                  under = std::stoi(words[5]);
                parseCap(file, layer_id, over, under);
              }
            }
            break;
        }
      }
      words.resize(0);
    }
  } else {
    logger_->error(utl::GRT, 1050, "read_rcx_model can't open file {}", file_name);
  }
}

bool getWordsFromFile(std::ifstream& file, std::vector<std::string>& res) {
  res.resize(0);
  std::string line;
  bool ok = !std::getline(file, line).eof();
  std::stringstream ss(line);
  std::string s;
  while(ss >> s) {
    res.push_back(s);
  }
  return ok;
}

void
RCTable::parseRes(std::ifstream& file, LayerId layer_id) {
  std::vector<std::string> words;
  while (getWordsFromFile(file, words)) {
    if (words.size() > 0) {
      if (words[0] == "END") {
        return;
      }
      // do nothing, use parseCap res
    }
  }
}

void
RCTable::parseCap(std::ifstream& file, LayerId layer_id, LayerId over_id, LayerId under_id) {
  std::vector<std::string> words;
  Width width = -1;
  while (getWordsFromFile(file, words)) {
    if (words.size() > 0) {
      if (words[0] == "END") {
        return;
      }
      if (words[0] == "DIST") {
        width = std::atof(words[4].c_str()) * 1e3;
      } else {
        Spacing sp = std::atof(words[0].c_str()) * 1e3;
        RCValue rc;
        rc.coupling_cap = std::atof(words[1].c_str());
        rc.gnd_cap = std::atof(words[2].c_str());
        rc.res = std::atof(words[3].c_str());
        table_[LaOU{layer_id, over_id, under_id}][width][sp] = rc;
      }
    }
  }
}

void
RCTable::checkLoaded(const char* file_name) {
  auto layer_count = tech_->getRoutingLayerCount();
  for (size_t layer_id = 1; layer_id <= layer_count; layer_id++) {
    Width width = tech_->findRoutingLayer(layer_id)->getWidth();
    for (size_t over_id = 0; over_id < layer_id; over_id++) {
      for (size_t under_id = layer_id + 1; under_id <= layer_count + 1; under_id++) {
        if (table_[LaOU{layer_id, over_id, under_id}][width].size() == 0) {
          logger_->error(utl::GRT, 1051, 
            "model file {} missing 'Metal {} OVER {} UNDER {}' in width {}, corner {}", file_name, layer_id, over_id, under_id, width, corner_->name());
        }
      }
    }
  }
  logger_->info(utl::GRT, 1052, "model file {} loaded", file_name);
  return;
}

RCValue 
RCTable::getWireUnitRC(LayerId layer_id, Spacing sp, Width width, LayerId over, LayerId under) {
  const SpRCTable& sp_table = table_[LaOU{layer_id, over, under}][width];
  auto rc_it = sp_table.lower_bound(sp);
  if (rc_it == sp_table.end()) {
    rc_it = std::prev(sp_table.end());
  }
  if (rc_it->first > sp && rc_it != sp_table.begin()) {
    const auto& prev_rc_it = std::prev(rc_it);  
    double ratio = 1.0 * (sp - prev_rc_it->first) / (rc_it->first - prev_rc_it->first);
    auto rc = prev_rc_it->second;
    rc.merge(rc_it->second, ratio);
    return rc;
  }
  if (rc_it->first > sp && rc_it == sp_table.begin()) {
    double ratio = std::min(rc_max_scale_for_overflow, rc_it->first / (sp + (rc_it->first - sp) / 2.0));
    auto rc = rc_it->second;
    rc.coupling_cap *= ratio;
    rc.gnd_cap *= ratio;
    rc.res *= ratio;
    return rc;
  }
  return rc_it->second;
}

RCValue 
RCTable::getWireUnitRC(LayerId idx_of_layer, Spacing sp, Width width, std::vector<double> densities, bool dbg) {
  for (auto& v : densities) {
    v = std::min(1.0, v);
  }
  auto layer_id = idx_of_layer + 1;
  auto layer_count = tech_->getRoutingLayerCount();

  // at least over 0 udner $layer_count+1
  densities.insert(densities.begin(), 1.0);
  densities.push_back(1.0);

  std::vector<double> p_layer(layer_count + 2);
  double p_empty = 1.0;
  for (int over = layer_id - 1; over >= 0; over--) {
    p_layer[over] = p_empty * densities[over];
    p_empty *= (1 - densities[over]);
  }
  p_empty = 1.0;
  for (int under = layer_id + 1; under <= layer_count + 1; under++) {
    p_layer[under] = p_empty * densities[under];
    p_empty *= (1 - densities[under]);
  }
  
  assert(fEqual(1.0, std::accumulate(p_layer.begin(), p_layer.begin() + layer_id, 0.0)));
  assert(fEqual(1.0, std::accumulate(p_layer.begin() + layer_id + 1, p_layer.end(), 0.0)));

  RCValue rc;
  for (int over = layer_id - 1; over >= 0; over--) {
    for (int under = layer_id + 1; under <= layer_count + 1; under++) {
      auto p_over_under = p_layer[over] * p_layer[under];
      if (fEqual(p_over_under, 0.0)) {
        continue;
      }
      if (dbg) {
        auto dbg_rc = getWireUnitRC(layer_id, sp, width, over, under) * p_over_under;
        logger_->report("metal {} over {} under {} p_over_under {} rc {} {}", layer_id, over, under, p_over_under, dbg_rc.unitRes(), dbg_rc.unitCap());
      }
      rc += getWireUnitRC(layer_id, sp, width, over, under) * p_over_under;
    }
  }
  if (dbg) {
    logger_->report("unit_rc result {} {} wid {} sp {}", rc.unitRes(), rc.unitCap(), width, sp);
  }
  return rc;
}


}


}  // namespace grt