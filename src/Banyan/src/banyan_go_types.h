#pragma once

// POD types that can be consumed by go code

// a box located in a specific layer
struct box
{
    // the layer the box belongs to
    // 0, 1, 2, ...: routing layers from the lowest
    // -1, -2, ...: cut layers between 0/1, 1/2, etc.
    int la;

	int x, y, w, h;
}; // struct box

#ifndef __cplusplus
typedef struct box box;
#endif

// a pin that can be accessed from a few boxes
// Boxes are assumed to be connected even if they are not within all know layers.
struct pin
{
    // for diagnostics only
    const char *name;

    // index into the box array
    long long box_begin;
    long long box_end;
}; // struct pin

// a net of pins that need to be connected
// Physical pins that need to be connected logically, e.g. GND,
// can appear multiple times, one for each component that is already connected.
struct net
{
    // for diagnostics only
    const char *name;

    // index into the pin array
    long long pin_begin;
    long long pin_end;

    // returned boxes
    long long rbox_begin;
    long long rbox_end;
}; // struct net

struct layer
{
    int width;
    int pitch;

    // index into the track array
    long long track_begin;
    long long track_end;
}; // struct layer

struct track
{
    int la;

    int line, tmin, tmax;
}; // struct track

// returned
struct entry_ret
{
    box *rboxes; // need free
    size_t rboxes_size;
}; // struct entry_ret
