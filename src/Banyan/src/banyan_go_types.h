#pragma once

// interface version
enum {
    interface_version_0 = 0xaa,
    interface_version_1 = 0xaa,
    interface_version_2 = 0x00,
    interface_version_3 = 0x00,
    interface_version_4 = 0x20,
    interface_version_5 = 0x21,
    interface_version_6 = 0x08,
    interface_version_7 = 0x12,
};

// POD types that can be consumed by go code

typedef unsigned long long GoUint64;

// option as key/value string pair
struct option
{
    const char *key;
    const char *value;
};

// a box located in a specific layer
struct box
{
    // the layer the box belongs to
    // 0, 1, 2, ...: routing layers from the lowest
    int la;

    int x, y, w, h;
}; // struct box

// a via connecting two layers
struct via
{
    // index into the viadef array
    int viadef_index;

    int x, y;
}; // struct via

enum net_use {
    USE_GROUND = 0,
    USE_POWER,
    USE_CLOCK,
    USE_SIGNAL,
};

enum layer_dir {
    DIR_NONE = 0,
    DIR_HORIZONTAL,
    DIR_VERTICAL,
};

enum pin_use {
    PIN_SOURCE = 0,
    PIN_SINK,
    PIN_OBS,
};

#ifndef __cplusplus
typedef struct box box;
typedef struct via via;
typedef enum net_use net_use;
typedef enum layer_dir layer_dir;
#endif

// a pin that can be accessed from a few boxes
// Boxes are assumed to be connected even if they are not within all know layers.
struct pin
{
    // for diagnostics only
    const char *name;
    
    int use;

    // index into the box array
    GoUint64 box_begin;
    GoUint64 box_end;

    // index into the via array
    GoUint64 via_begin;
    GoUint64 via_end;    
}; // struct pin

// a net of pins that need to be connected
// Physical pins that need to be connected logically, e.g. GND,
// can appear multiple times, one for each component that is already connected.
struct net
{
    // for diagnostics only
    const char *name;

    int ndr_index;

    int use;

    // index into the pin array
    GoUint64 pin_begin;
    GoUint64 pin_end;

    // returned boxes
    GoUint64 rbox_begin;
    GoUint64 rbox_end;

    // returned vias
    GoUint64 rvia_begin;
    GoUint64 rvia_end;
}; // struct net

struct viadef
{
    // for diagnostics only
    const char *name;

    box top, bottom;
}; // struct viadef

struct layer
{
    // for diagnostics only
    const char *name;

    int dir;

    int width;
    int pitch;

    GoUint64 min_steps;
    /*
    Specifies that up to maxEdges consecutive edges that are less
    than minStepLength in length are allowed, but more than
    maxEdges in a row is a violation. Typically, most tools only
    allow a maxEdges value of 0, 1, or 2. A maxEdges value of 0
    means that no edge can be less than minStepLength.
    */
    GoUint64 max_edges;
    /*
    Specifies the maximum total length of consecutive short edges
    (edges that are less than minStepLength) that OPC can
    correct without causing new DRC violations.
    If the total length of the edges is greater than maxLength, a
    violation occurs. No violation occurs if the total length is less
    than or equal to maxLength.
    */
    GoUint64 max_length;

    // index into the track array
    GoUint64 track_begin;
    GoUint64 track_end;

    // index into the viadef array
    // viadef connect this and one layer below
    GoUint64 viadef_begin;
    GoUint64 viadef_end;

    // index into the axioms array
    GoUint64 via_track_begin, via_track_end;
    GoUint64 trunk_min_len_begin, trunk_min_len_end;
    GoUint64 trunk_dist_same_begin, trunk_dist_same_end;
    GoUint64 trunk_dist_diff_begin, trunk_dist_diff_end;
    GoUint64 trunk_via_begin, trunk_via_end;
    GoUint64 via_track_diff_begin, via_track_diff_end;
    GoUint64 via_track_same_begin, via_track_same_end;
}; // struct layer

struct track
{
    int la;

    int line, tmin, tmax;
}; // struct track

struct ndr_layer
{
    int la;
    int width;
    int spacing;
    unsigned int wire_ext;
};

struct ndr
{
    // for diagnostics only
    const char* name;

    int hard_spacing;

    GoUint64 ndr_via_begin;
    GoUint64 ndr_via_end;

    GoUint64 ndr_layer_begin;
    GoUint64 ndr_layer_end;
};

struct design
{
    const char *name;

	int xmin, ymin, xmax, ymax;
}; // struct design

// returned
struct route_ret
{
    box *rboxes; // need free
    GoUint64 rboxes_size;

    via *rvias; // need free
    GoUint64 rvias_size;
}; // struct entry_ret

struct axioms_triple
{
    // horizontal: Align the left side of via and wire
    // vertical: Align the bottom side of via and wire
    int via_x;
    // horizontal: Align the right side of via and wire
    // vertical: Align the top side of via and wire
    int via_y;
    GoUint64 len;
};
