%{
#include "linden/gp.hpp"
#include "./cgr/ffi.hpp"
#include "./dp/ffi.hpp"
#include "./add_tie_cell/ffi.hpp"
#include "./gp/ffi.hpp"
#include "./initial_place/ffi.hpp"
#include "./itdp/ffi.hpp"
#include "./mp/ffi.hpp"
#include "./remove_fillers/ffi.hpp"
#include "./timing_data/ffi.hpp"
#include "../include/linden/eco_placer.hpp"
#include "odb/dbTypes.h"
#include "odb/geom.h"
#include <memory>
#include <deque>
#include <string>
#include <vector>
#include "envflag/envflag.hpp"
IMPORT_STR_FLAG_WITH_DEFAULT(LINDEN_TMP_FILE_ROOT_DIR);
%}

%include "typemaps.i"
%include "../include/linden/eco_placer.hpp"

%inline %{

class StrDeque {
public:
    explicit StrDeque() {}
    void push_back(const char* elem) {
        elems_.emplace_back(elem);
    }

    std::deque<std::string> elems_;
};

%}

%inline %{ // lindengp

void set_density(float density)
{
    linden::gp::Config::get_default()->mut_density() = density;
}

void set_to_uniform_density()
{
    linden::gp::Config::get_default()->set_to_uniform();
}

void set_overflow(float overflow)
{
    linden::gp::Config::get_default()->overflow = overflow;
}

void enable_timing_driven_mode()
{
    linden::gp::Config::get_default()->timing_driven = true;
}

void set_initial_place_policy(const char* policy)
{
    using linden::gp::Config;
    Config::get_default()->initial_place_policy =
        Config::InitialPlacePolicy::from_str(policy);
}

void enable_routability_driven_mode() {
    linden::gp::Config::get_default()->routability_driven = true;
}

void set_overflow_to_estimate_cmap(float overflow) {
    linden::gp::Config::get_default()->overflow_to_estimate_cmap = overflow;
}

void set_highest_layer_to_route(const char* highest_layer) {
    linden::gp::Config::get_default()->mut_highest_layer_to_route() = highest_layer;
}

void lindengp_run_cmd(StrDeque& args)
{
    linden::gp::gogogo(std::move(args.elems_));
}

void linden_initial_place()
{
    auto cfg = linden::gp::Config::get_default();
    linden::ip::gogogo(*cfg);
}

void set_continuation(const char* file) {
    using linden::gp::Config;
    Config::get_default()->continuation = file;
}

void lindengp_random_run_cmd() {
    linden::gp::random_place();
}


%} // inline lindengp

%inline %{ // linden others
void linden_remove_fillers_cmd() {
    linden::remove_filler::gogogo();
}
%} // linden others

%inline %{ // lindenmp

void lindenmp_run_cmd(StrDeque& args)
{
    linden::mp::mp_gogogo(std::move(args.elems_));
}

%} // inline lindenmp

%inline %{ // linden_lpbased_postdrv_itdp

void linden_lpbased_postdrv_itdp(const char* pb_path)
{
    linden::itdp::itdp_gogogo(pb_path);
}

%} // inline linden_lpbased_postdrv_itdp

%inline %{

int lindendp_gogogo(StrDeque& args) {
    return linden::basic_dp_gogogo(std::move(args.elems_));
}

int linden_incr_legalize(StrDeque& args) {
    return linden::incr_dp_gogogo(std::move(args.elems_));
}

int routability_driven_legalize(StrDeque& args) {
    return linden::rddp_gogogo(std::move(args.elems_));
}

int lindenopt_gogogo(StrDeque& args) {
    return linden::opt_gogogo(std::move(args.elems_));
}

%} // lindendp


%inline %{

void write_tdp_data_cmd(const char* filename) {
    linden::write_tdp_data(filename);
}

void read_tdp_data_cmd(const char* filename) {
    linden::read_tdp_data(filename);
}

%} // timing data

%inline %{

double run()
{
// cgr::CongestionRepair* cgr = getCongestionRepair();
// return cgr->run();
    return 0;
}

void done() {
// cgr::CongestionRepair* cgr = getCongestionRepair();
// return cgr->done();
}

void set_cgr_params_file_cmd(const char* cgr_params_file) {
// cgr::CongestionRepair* cgr = getCongestionRepair();
// cgr->setCGRParamsFile(cgr_params_file);
}

void set_routing_max_layer_cmd(const char* max_layer) {
// cgr::CongestionRepair* cgr = getCongestionRepair();
// cgr->setRoutingMaxLayer(max_layer);
}

int analyze_banyan_cmap_file_cmd(const char* cmap_file, const char* report_file) {
    return linden::cgr::analyze_file_gogogo(cmap_file, report_file);
}

%} // cgr

%inline %{

const char* env_tmp_file_root_dir() {
    return LINDEN_TMP_FILE_ROOT_DIR().data();
}

%} // exposing envflags

// Typemap for odb::Point &
%typemap(in) odb::Point & {
    const char *inputString = Tcl_GetString($input);
    const char **splitElements;
    int elementCount;

    if (Tcl_SplitList(interp, inputString, &elementCount, &splitElements) != TCL_OK || elementCount != 2) {
        SWIG_fail;
    }

    int x, y;
    if (Tcl_GetInt(interp, splitElements[0], &x) != TCL_OK ||
        Tcl_GetInt(interp, splitElements[1], &y) != TCL_OK) {
        Tcl_Free((char *)splitElements);
        SWIG_fail;
    }

    Tcl_Free((char *)splitElements);

    $1 = new odb::Point(x, y);
}
%typemap(out) odb::Point & {
    delete $1;  
}

// Typemap for linden::PointPlace &
%typemap(in) linden::PointPlace & {
    const char *inputString = Tcl_GetString($input);
    const char **splitElements;
    int elementCount;

    if (Tcl_SplitList(interp, inputString, &elementCount, &splitElements) != TCL_OK || elementCount != 3) {
        SWIG_fail;
    }

    int x, y;
    if (Tcl_GetInt(interp, splitElements[0], &x) != TCL_OK ||
        Tcl_GetInt(interp, splitElements[1], &y) != TCL_OK) {
        Tcl_Free((char *)splitElements);
        SWIG_fail;
    }

    odb::dbOrientType orient;
    if (strcmp(splitElements[2], "R0") == 0) orient = odb::dbOrientType::R0;
    else if (strcmp(splitElements[2], "R180") == 0) orient = odb::dbOrientType::R180;
    else if (strcmp(splitElements[2], "R90") == 0) orient = odb::dbOrientType::R90;
    else if (strcmp(splitElements[2], "R270") == 0) orient = odb::dbOrientType::R270;
    else if (strcmp(splitElements[2], "MY") == 0) orient = odb::dbOrientType::MY;
    else if (strcmp(splitElements[2], "MX") == 0) orient = odb::dbOrientType::MX;
    else if (strcmp(splitElements[2], "MXR90") == 0) orient = odb::dbOrientType::MXR90;
    else if (strcmp(splitElements[2], "MYR90") == 0) orient = odb::dbOrientType::MYR90;
    else {
        Tcl_Free((char *)splitElements);
        SWIG_fail;
    }

    Tcl_Free((char *)splitElements);

    $1 = new linden::PointPlace(odb::Point(x, y), orient);
}
%typemap(out) linden::PointPlace & {
    delete $1; 
}

// Typemap for std::string &
%typemap(in) std::string & {
    const char* tempStr = Tcl_GetString($input);
    $1 = new std::string(tempStr);
}
%typemap(out) std::string & {
    $result = Tcl_NewStringObj($1->c_str(), $1->length());
}

%inline %{
// Defines a module independent of the OBD to handle operations related to eco_placer.
class EcoPlacerSwig {
public:
    EcoPlacerSwig() : _ep(nullptr) {
    }

    std::vector<std::string> _lefs;  // List of LEF file paths
    std::vector<std::string> _defs;  // List of DEF file paths
    linden::EcoPlacer* _ep;          // Pointer to an instance of EcoPlacer
};

// Creates an instance of EcoPlacerSwig.
EcoPlacerSwig* make_eco_placer() {
    return new EcoPlacerSwig();
}

// Adds a LEF file path to an EcoPlacerSwig instance.
void eco_placer_add_lef(EcoPlacerSwig* eps, const std::string& lef) {
    eps->_lefs.push_back(lef);
}   

// Adds a DEF file path to an EcoPlacerSwig instance.
void eco_placer_add_def(EcoPlacerSwig* eps, const std::string& def) {
    eps->_defs.push_back(def);
}   

// Initializes the EcoPlacer instance using the previously added LEF and DEF files.
void eco_placer_init(EcoPlacerSwig* eps) {
    eps->_ep = new linden::EcoPlacer(eps->_lefs, eps->_defs);
}

// Initializes the EcoPlacer instance from OpenRoad.
void eco_placer_init_from_odb(EcoPlacerSwig* eps) {
    eps->_ep = new linden::EcoPlacer();
}

// Finds vacant places within a specified range and returns a list of place descriptions.
std::vector<const char*> eco_placer_find_vacant_places_i(
    EcoPlacerSwig* eps, 
    const std::string& instance, 
    const odb::Point& p0, 
    const odb::Point& p1, 
    int x_delta, 
    int y_delta)
{
    std::vector<linden::PointPlace> places;
    eps->_ep->find_vacant_places(places, instance, p0, p1, x_delta, y_delta);
    
    std::vector<const char*> string_places;
    string_places.reserve(places.size());

    for (const auto& place : places) {
        std::string formatted_place;
        place.prettyPrint(formatted_place);
        char* heap_str = strdup(formatted_place.c_str());
        string_places.push_back(heap_str);
    }

    return string_places;
}

// Find the nearest place to given point and return a PlaceIterator
linden::PlaceIterator* eco_placer_find_nearest_place_i(
    EcoPlacerSwig* eps, 
    const std::string& instance, 
    const odb::Point& point, 
    const std::string& xmin, 
    const std::string& ymin, 
    const std::string& xmax, 
    const std::string& ymax) 
{
    std::optional<int> opt_xmin = (xmin == "null") ? std::nullopt : std::optional<int>(std::stoi(xmin));
    std::optional<int> opt_ymin = (ymin == "null") ? std::nullopt : std::optional<int>(std::stoi(ymin));
    std::optional<int> opt_xmax = (xmax == "null") ? std::nullopt : std::optional<int>(std::stoi(xmax));
    std::optional<int> opt_ymax = (ymax == "null") ? std::nullopt : std::optional<int>(std::stoi(ymax));
    linden::PlaceRange range(opt_xmin, opt_ymin, opt_xmax, opt_ymax);
    
    return new linden::PlaceIterator(*(eps->_ep), instance, point, range);
}

// Get the next place of the PlaceIterator
const char* eco_placer_next_place(linden::PlaceIterator* pi) {
    auto place = pi->next();
    std::string place_str;
    if (place.has_value()) {
        place->prettyPrint(place_str);
    } else {
        place_str = "null";
    }
    char* result = new char[place_str.size() + 1];
    std::strcpy(result, place_str.c_str());
    return result;
}
// Inserts a cell in place, but does not actually insert the cell into the design.
void eco_placer_insert_cell(
    EcoPlacerSwig* eps, 
    const std::string& master, 
    const std::string& instance, 
    const linden::PointPlace& place)
{
    eps->_ep->insert_cell(master, instance, place);
}

// Removes a unit, but does not actually remove the unit from the design.
void eco_placer_remove_cell(EcoPlacerSwig* eps, const std::string& master) {
    eps->_ep->remove_cell(master);
}

// Completes operations and cleans up the EcoPlacerSwig instance.
void eco_placer_finish(EcoPlacerSwig* eps) {
    delete eps->_ep;
    delete eps;
}

// Retrieves location information string by index.
const char* point_place_by_idx(std::vector<const char*> places, int idx) {
    return places[idx];
}

// Returns the size of the list of location information strings.
int vec_point_place_size(std::vector<const char*> places) {
    return places.size();
}

%} // eco_placer


%inline %{

int add_tie_cell_cmd(StrDeque& args) {
    return linden::add_tie_cell_gogogo(std::move(args.elems_));
}

%} // add_tie_cell
