#pragma once

// POD types that can be consumed by go code

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
    size_t box_begin;
    size_t box_end;

    // index into the via array
    size_t via_begin;
    size_t via_end;    
}; // struct pin

// a net of pins that need to be connected
// Physical pins that need to be connected logically, e.g. GND,
// can appear multiple times, one for each component that is already connected.
struct net
{
    // for diagnostics only
    const char *name;

    int use;

    // index into the pin array
    size_t pin_begin;
    size_t pin_end;

    // returned boxes
    size_t rbox_begin;
    size_t rbox_end;

    // returned vias
    size_t rvia_begin;
    size_t rvia_end;
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
    size_t track_begin;
    size_t track_end;

    // index into the viadef array
    // viadef connect this and one layer below
    size_t viadef_begin;
    size_t viadef_end;
}; // struct layer

struct track
{
    int la;

    int line, tmin, tmax;
}; // struct track

struct design
{
    const char *name;

	int xmin, ymin, xmax, ymax;
}; // struct design

// returned
struct entry_ret
{
    box *rboxes; // need free
    size_t rboxes_size;

    via *rvias; // need free
    size_t rvias_size;
}; // struct entry_ret