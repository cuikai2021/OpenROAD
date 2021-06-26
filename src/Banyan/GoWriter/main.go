package main

/*
#include "../src/banyan_go_types.h"
*/
import "C"

import (
	"encoding/json"
	"fmt"
	"os"
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
	design C.struct_design,
	ret *C.struct_entry_ret) int {
	/*
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
	*/

	ret.rboxes_size = C.ulong(0)
	ret.rvias_size = C.ulong(0)

	chip1 := newJsonChip(
		nets, pins, boxes, vias, layers, viadefs, tracks, design)
	fmt.Printf(
		"saving %s: nets %d, pins %d, boxes %d, "+
			"vias %d, layers %d, viadefs %d, tracks %d\n",
		C.GoString(chip1.Design.name), chip1.NN, chip1.NP, chip1.NB,
		chip1.NV, chip1.NL, chip1.NVD, chip1.NT)

	jsonfile, _ := os.Create("banyan.json")
	defer jsonfile.Close()
	enc := json.NewEncoder(jsonfile)
	enc.SetIndent("", "  ")
	err := enc.Encode(chip1)
	if err != nil {
		panic(err)
	}

	jsonfile2, _ := os.Open("banyan.json")
	defer jsonfile2.Close()
	dec := json.NewDecoder(jsonfile2)
	chip2 := &jsonChip{}
	err = dec.Decode(chip2)
	if err != nil {
		panic(err)
	}

	fmt.Printf(
		"loaded %s: nets %d, pins %d, boxes %d, "+
			"vias %d, layers %d, viadefs %d, tracks %d\n",
		C.GoString(chip2.Design.name), chip2.NN, chip2.NP, chip2.NB,
		chip2.NV, chip2.NL, chip2.NVD, chip2.NT)

	jsonfile3, _ := os.Create("banyan2.json")
	defer jsonfile3.Close()
	enc = json.NewEncoder(jsonfile3)
	enc.SetIndent("", "  ")
	err = enc.Encode(chip2)
	if err != nil {
		panic(err)
	}

	return 0
}
