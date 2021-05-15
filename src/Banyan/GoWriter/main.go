package main

/*
#include "../src/banyan_go_types.h"
*/
import "C"

import "fmt"

func main() {
}

//export banyan
func banyan(boxes []C.struct_box) int {
	fmt.Printf("Hello Banyan!\n")
	for i := 0; i < len(boxes); i++ {
		fmt.Printf("%d: %+v\n", i, boxes[i])
	}
	return 12345678987654321
}
