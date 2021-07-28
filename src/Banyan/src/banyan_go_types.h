#pragma once

// interface version
enum {
    interface_version_0 = 0xaa,
    interface_version_1 = 0xaa,
    interface_version_2 = 0x00,
    interface_version_3 = 0x00,
    interface_version_4 = 0x20,
    interface_version_5 = 0x21,
    interface_version_6 = 0x07,
    interface_version_7 = 0x15,
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

    // index into the track array
    GoUint64 track_begin;
    GoUint64 track_end;

    // index into the viadef array
    // viadef connect this and one layer below
    GoUint64 viadef_begin;
    GoUint64 viadef_end;
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
    // the bottom left corner of box of via_x and wire are aligned
    int via_x;
    // the bottom right corner of box of via_y and wire are aligned
    int via_y;
    GoUint64 len;
};


struct trunk_min_len_table
{
    int la;
    GoUint64 axioms_triple_begin;
    GoUint64 axioms_triple_end;
};