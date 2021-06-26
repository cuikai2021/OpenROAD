package main

/*
#include "../src/banyan_go_types.h"
*/
import "C"

import (
	"encoding/json"
)

type jsonDesign C.struct_design
type jsonNets []C.struct_net
type jsonPins []C.struct_pin
type jsonBoxes []C.struct_box
type jsonVias []C.struct_via
type jsonLayers []C.struct_layer
type jsonViadefs []C.struct_viadef
type jsonTracks []C.struct_track

type jsonChip struct {
	Design  jsonDesign  `json:"design"`
	NN      int         `json:"numNets"`
	Nets    jsonNets    `json:"nets"`
	NP      int         `json:"numPins"`
	Pins    jsonPins    `json:"pins"`
	NB      int         `json:"numBoxes"`
	Boxes   jsonBoxes   `json:"boxes"`
	NV      int         `json:"numVias"`
	Vias    jsonVias    `json:"vias"`
	NL      int         `json:"numLayers"`
	Layers  jsonLayers  `json:"layers"`
	NVD     int         `json:"numViadefs"`
	Viadefs jsonViadefs `json:"viadefs"`
	NT      int         `json:"numTracks"`
	Tracks  jsonTracks  `json:"tracks"`
}

func newJsonChip(
	nets []C.struct_net,
	pins []C.struct_pin,
	boxes []C.struct_box,
	vias []C.struct_via,
	layers []C.struct_layer,
	viadefs []C.struct_viadef,
	tracks []C.struct_track,
	design C.struct_design) jsonChip {
	return jsonChip{jsonDesign(design),
		len(nets), jsonNets(nets),
		len(pins), jsonPins(pins),
		len(boxes), jsonBoxes(boxes),
		len(vias), jsonVias(vias),
		len(layers), jsonLayers(layers),
		len(viadefs), jsonViadefs(viadefs),
		len(tracks), jsonTracks(tracks)}
}

func (design jsonDesign) MarshalJSON() ([]byte, error) {
	return json.Marshal(&struct {
		Name string  `json:"name"`
		Rect []C.int `json:"rect"`
	}{C.GoString(design.name),
		[]C.int{design.xmin, design.ymin, design.xmax, design.ymax}})
}

func (design *jsonDesign) UnmarshalJSON(data []byte) error {
	var v map[string]interface{}
	err := json.Unmarshal(data, &v)
	if err != nil {
		return err
	}

	design.name = C.CString(v["name"].(string))
	rect := v["rect"].([]interface{})
	design.xmin = C.int(rect[0].(float64))
	design.ymin = C.int(rect[1].(float64))
	design.xmax = C.int(rect[2].(float64))
	design.ymax = C.int(rect[3].(float64))

	return nil
}

func (nets jsonNets) MarshalJSON() ([]byte, error) {
	arr := make([]interface{}, 0, len(nets))
	for i, net := range nets {
		arr = append(arr, &struct {
			I int       `json:"nid"`
			N string    `json:"name"`
			U C.int     `json:"use"`
			R []C.ulong `json:"r"`
		}{i, C.GoString(net.name), net.use,
			[]C.ulong{net.pin_begin, net.pin_end}})
	}
	return json.Marshal(arr)
}

func (net *C.struct_net) UnmarshalJSON(data []byte) error {
	var v map[string]interface{}
	err := json.Unmarshal(data, &v)
	if err != nil {
		return err
	}

	net.name = C.CString(v["name"].(string))
	net.use = C.int(v["use"].(float64))
	r := v["r"].([]interface{})
	net.pin_begin = C.ulong(r[0].(float64))
	net.pin_end = C.ulong(r[1].(float64))

	return nil
}

func (pins jsonPins) MarshalJSON() ([]byte, error) {
	arr := make([]interface{}, 0, len(pins))
	for i, pin := range pins {
		arr = append(arr, &struct {
			I int       `json:"pid"`
			N string    `json:"name"`
			R []C.ulong `json:"r"`
		}{i, C.GoString(pin.name), []C.ulong{
			pin.box_begin, pin.box_end,
			pin.via_begin, pin.via_end}})
	}
	return json.Marshal(arr)
}

func (pin *C.struct_pin) UnmarshalJSON(data []byte) error {
	var m map[string]interface{}
	err := json.Unmarshal(data, &m)
	if err != nil {
		return err
	}

	pin.name = C.CString(m["name"].(string))
	r := m["r"].([]interface{})
	pin.box_begin = C.ulong(r[0].(float64))
	pin.box_end = C.ulong(r[1].(float64))
	pin.via_begin = C.ulong(r[2].(float64))
	pin.via_end = C.ulong(r[3].(float64))

	return nil
}

func (boxes jsonBoxes) MarshalJSON() ([]byte, error) {
	arr := make([]interface{}, 0, len(boxes))
	for i, box := range boxes {
		arr = append(arr, &struct {
			I int     `json:"bid"`
			B []C.int `json:"b"`
		}{i, []C.int{box.la, box.x, box.y, box.w, box.h}})
	}
	return json.Marshal(arr)
}

func (box *C.struct_box) UnmarshalJSON(data []byte) error {
	var m map[string]interface{}
	err := json.Unmarshal(data, &m)
	if err != nil {
		return err
	}

	b := m["b"].([]interface{})
	box.la = C.int(b[0].(float64))
	box.x = C.int(b[1].(float64))
	box.y = C.int(b[2].(float64))
	box.w = C.int(b[3].(float64))
	box.h = C.int(b[4].(float64))

	return nil
}

func (vias jsonVias) MarshalJSON() ([]byte, error) {
	arr := make([]interface{}, 0, len(vias))
	for i, via := range vias {
		arr = append(arr, &struct {
			I int     `json:"vid"`
			V []C.int `json:"v"`
		}{i, []C.int{via.viadef_index, via.x, via.y}})
	}
	return json.Marshal(arr)
}

func (via *C.struct_via) UnmarshalJSON(data []byte) error {
	var m map[string]interface{}
	err := json.Unmarshal(data, &m)
	if err != nil {
		return err
	}

	v := m["v"].([]interface{})
	via.viadef_index = C.int(v[0].(float64))
	via.x = C.int(v[1].(float64))
	via.y = C.int(v[2].(float64))

	return nil
}

func (layers jsonLayers) MarshalJSON() ([]byte, error) {
	arr := make([]interface{}, 0, len(layers))
	for i, layer := range layers {
		arr = append(arr, &struct {
			I int       `json:"la"`
			N string    `json:"name"`
			D C.int     `json:"dir"`
			W C.int     `json:"w"`
			P C.int     `json:"p"`
			R []C.ulong `json:"r"`
		}{i, C.GoString(layer.name), layer.dir,
			layer.width, layer.pitch, []C.ulong{
				layer.track_begin, layer.track_end,
				layer.viadef_begin, layer.viadef_end}})
	}
	return json.Marshal(arr)
}

func (layer *C.struct_layer) UnmarshalJSON(data []byte) error {
	var m map[string]interface{}
	err := json.Unmarshal(data, &m)
	if err != nil {
		return err
	}

	layer.name = C.CString(m["name"].(string))
	layer.dir = C.int(m["dir"].(float64))
	layer.width = C.int(m["w"].(float64))
	layer.pitch = C.int(m["p"].(float64))
	r := m["r"].([]interface{})
	layer.track_begin = C.ulong(r[0].(float64))
	layer.track_end = C.ulong(r[1].(float64))
	layer.viadef_begin = C.ulong(r[2].(float64))
	layer.viadef_end = C.ulong(r[3].(float64))

	return nil
}

func (viadefs jsonViadefs) MarshalJSON() ([]byte, error) {
	arr := make([]interface{}, 0, len(viadefs))
	for i, viadef := range viadefs {
		arr = append(arr, &struct {
			I  int     `json:"vdid"`
			N  string  `json:"name"`
			TB []C.int `json:"tb"`
			BB []C.int `json:"bb"`
		}{i, C.GoString(viadef.name),
			[]C.int{viadef.top.la, viadef.top.x, viadef.top.y,
				viadef.top.w, viadef.top.h},
			[]C.int{viadef.bottom.la, viadef.bottom.x, viadef.bottom.y,
				viadef.bottom.w, viadef.bottom.h}})
	}
	return json.Marshal(arr)
}

func (viadef *C.struct_viadef) UnmarshalJSON(data []byte) error {
	var m map[string]interface{}
	err := json.Unmarshal(data, &m)
	if err != nil {
		return err
	}

	viadef.name = C.CString(m["name"].(string))

	tb := m["tb"].([]interface{})
	viadef.top.la = C.int(tb[0].(float64))
	viadef.top.x = C.int(tb[1].(float64))
	viadef.top.y = C.int(tb[2].(float64))
	viadef.top.w = C.int(tb[3].(float64))
	viadef.top.h = C.int(tb[4].(float64))

	bb := m["bb"].([]interface{})
	viadef.bottom.la = C.int(bb[0].(float64))
	viadef.bottom.x = C.int(bb[1].(float64))
	viadef.bottom.y = C.int(bb[2].(float64))
	viadef.bottom.w = C.int(bb[3].(float64))
	viadef.bottom.h = C.int(bb[4].(float64))

	return nil
}

func (tracks jsonTracks) MarshalJSON() ([]byte, error) {
	arr := make([]interface{}, 0, len(tracks))
	for i, track := range tracks {
		arr = append(arr, &struct {
			I int     `json:"tid"`
			T []C.int `json:"t"`
		}{i, []C.int{track.la, track.line, track.tmin, track.tmax}})
	}
	return json.Marshal(arr)
}

func (track *C.struct_track) UnmarshalJSON(data []byte) error {
	var m map[string]interface{}
	err := json.Unmarshal(data, &m)
	if err != nil {
		return err
	}

	t := m["t"].([]interface{})
	track.la = C.int(t[0].(float64))
	track.line = C.int(t[1].(float64))
	track.tmin = C.int(t[2].(float64))
	track.tmax = C.int(t[3].(float64))

	return nil
}
