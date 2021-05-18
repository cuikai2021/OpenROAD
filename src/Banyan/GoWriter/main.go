package main

/*
#include "../src/banyan_go_types.h"
*/
import "C"

import (
	"fmt"
	"unsafe"
)

func main() {
}

func boxAlloc(n C.ulong) *C.struct_box {
	return (*C.struct_box)(unsafe.Pointer(
		C.malloc(C.sizeof_struct_box * n)))
}

func boxAt(arr *C.struct_box, i int) *C.struct_box {
	return (*C.struct_box)(unsafe.Pointer(
		uintptr(unsafe.Pointer(arr)) + uintptr(i*C.sizeof_struct_box)))
}

func viaAlloc(n C.ulong) *C.struct_via {
	return (*C.struct_via)(unsafe.Pointer(
		C.malloc(C.sizeof_struct_via * n)))
}

func viaAt(arr *C.struct_via, i int) *C.struct_via {
	return (*C.struct_via)(unsafe.Pointer(
		uintptr(unsafe.Pointer(arr)) + uintptr(i*C.sizeof_struct_via)))
}

//export banyan
func banyan(
	nets []C.struct_net,
	pins []C.struct_pin,
	boxes []C.struct_box,
	vias []C.struct_via,
	layers []C.struct_layer,
	viadefs []C.struct_viadef,
	tracks []C.struct_track,
	ret *C.struct_entry_ret) int {

	fmt.Printf("Hello Banyan!\n")

	fmt.Printf("layers %d, viadefs %d, tracks %d\n",
		len(layers), len(viadefs), len(tracks))

	for i := 0; i < len(nets); i++ {
		fmt.Printf("net %d: %s, %+v\n", i, C.GoString(nets[i].name), nets[i])
	}

	for i := 0; i < len(pins); i++ {
		fmt.Printf("pin %d: %s, %+v\n", i, C.GoString(pins[i].name), pins[i])
	}

	for i := 0; i < len(viadefs); i++ {
		fmt.Printf("viadef %d: %s, t %+v, b %+v\n",
			i, C.GoString(viadefs[i].name), viadefs[i].top, viadefs[i].bottom)
	}

	ret.rboxes_size = C.ulong(len(boxes))
	ret.rboxes = boxAlloc(ret.rboxes_size)
	ret.rvias_size = C.ulong(len(vias))
	ret.rvias = viaAlloc(ret.rvias_size)

	nets[0].rbox_begin = 0
	nets[0].rbox_end = C.ulong(len(boxes))
	nets[0].rvia_begin = 0
	nets[0].rvia_end = C.ulong(len(vias))

	for i := 0; i < len(boxes); i++ {
		fmt.Printf("box %d: %+v\n", i, boxes[i])
		*boxAt(ret.rboxes, i) = boxes[i]
	}

	for i := 0; i < len(vias); i++ {
		fmt.Printf("via %d: %+v\n", i, vias[i])
		*viaAt(ret.rvias, i) = vias[i]
	}

	return 12345678987654321
}
