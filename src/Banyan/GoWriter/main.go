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

//export banyan
func banyan(
	nets []C.struct_net,
	pins []C.struct_pin,
	boxes []C.struct_box,
	layers []C.struct_layer,
	tracks []C.struct_track,
	ret *C.struct_entry_ret) int {

	fmt.Printf("Hello Banyan!\n")

	fmt.Printf("layers %d, tracks %d\n", len(layers), len(tracks))

	ret.rboxes_size = C.ulong(len(boxes))
	ret.rboxes = boxAlloc(ret.rboxes_size)

	for i := 0; i < len(nets); i++ {
		fmt.Printf("net %d: %s, %+v\n", i, C.GoString(nets[i].name), nets[i])
	}

	for i := 0; i < len(pins); i++ {
		fmt.Printf("pin %d: %s, %+v\n", i, C.GoString(pins[i].name), pins[i])
	}

	nets[0].rbox_begin = 0
	nets[0].rbox_end = C.longlong(len(boxes))

	for i := 0; i < len(boxes); i++ {
		fmt.Printf("box %d: %+v\n", i, boxes[i])
		*boxAt(ret.rboxes, i) = boxes[i]
	}

	return 12345678987654321
}
