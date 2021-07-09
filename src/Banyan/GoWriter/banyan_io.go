package main

/*
#include "../src/banyan_go_types.h"
*/
import "C"

import (
	"bufio"
	"compress/bzip2"
	"encoding/json"
	"fmt"
	"io"
	"os"
	"strconv"
	"strings"

	"github.com/rs/zerolog/log"
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
			D C.ulong   `json:"ndr_index"`
			U C.int     `json:"use"`
			R []C.ulong `json:"r"`
		}{i, C.GoString(net.name), net.ndr_index, net.use,
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
	net.ndr_index = C.ulong(v["ndr_index"].(float64))
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

func (chip *jsonChip) saveJSON(filename string) {
	log.Info().Msgf("saving %s ...", filename)

	jsonfile, err := os.Create(filename)
	if err != nil {
		panic(err)
	}
	defer jsonfile.Close()
	enc := json.NewEncoder(jsonfile)
	enc.SetIndent("", "  ")
	err = enc.Encode(chip)
	if err != nil {
		panic(err)
	}

	log.Info().Msgf(
		"json saved: %s[%d,%d,%d,%d], nets %d, pins %d, boxes %d, "+
			"vias %d, layers %d, viadefs %d, tracks %d",
		C.GoString(chip.Design.name),
		chip.Design.xmin, chip.Design.ymin, chip.Design.xmax, chip.Design.ymax,
		chip.NN, chip.NP, chip.NB, chip.NV, chip.NL, chip.NVD, chip.NT)
}

func (chip *jsonChip) loadJSON(filename string) {
	log.Info().Msgf("loading %s ...", filename)

	jsonfile, err := os.Open(filename)
	if err != nil {
		panic(err)
	}
	defer jsonfile.Close()
	dec := json.NewDecoder(jsonfile)
	err = dec.Decode(chip)
	if err != nil {
		panic(err)
	}

	log.Info().Msgf(
		"json loaded: %s[%d,%d,%d,%d], nets %d, pins %d, boxes %d, "+
			"vias %d, layers %d, viadefs %d, tracks %d",
		C.GoString(chip.Design.name),
		chip.Design.xmin, chip.Design.ymin, chip.Design.xmax, chip.Design.ymax,
		chip.NN, chip.NP, chip.NB, chip.NV, chip.NL, chip.NVD, chip.NT)
}

func (chip *jsonChip) saveTXT(filename string) {
	log.Info().Msgf("saving %s ...", filename)

	txtfile, err := os.Create(filename)
	if err != nil {
		panic(err)
	}
	defer txtfile.Close()

	fmt.Fprintf(txtfile, "design,%s,%d,%d,%d,%d\n",
		C.GoString(chip.Design.name),
		chip.Design.xmin, chip.Design.ymin,
		chip.Design.xmax, chip.Design.ymax)

	fmt.Fprintf(txtfile, "nets,%d\n", len(chip.Nets))
	for i, net := range chip.Nets {
		fmt.Fprintf(txtfile, "net%d,%s,%d,%d,%d,%d\n",
			i, C.GoString(net.name), net.ndr_index, net.use,
			net.pin_begin, net.pin_end)
	}

	fmt.Fprintf(txtfile, "pins,%d\n", len(chip.Pins))
	for i, pin := range chip.Pins {
		fmt.Fprintf(txtfile, "pin%d,%s,%d,%d,%d,%d\n",
			i, C.GoString(pin.name),
			pin.box_begin, pin.box_end,
			pin.via_begin, pin.via_end)
	}

	fmt.Fprintf(txtfile, "boxes,%d\n", len(chip.Boxes))
	for i, box := range chip.Boxes {
		fmt.Fprintf(txtfile, "box%d,%d,%d,%d,%d,%d\n",
			i, box.la, box.x, box.y, box.w, box.h)
	}

	fmt.Fprintf(txtfile, "vias,%d\n", len(chip.Vias))
	for i, via := range chip.Vias {
		fmt.Fprintf(txtfile, "via%d,%d,%d,%d\n",
			i, via.viadef_index, via.x, via.y)
	}

	fmt.Fprintf(txtfile, "layers,%d\n", len(chip.Layers))
	for i, layer := range chip.Layers {
		fmt.Fprintf(txtfile, "layer%d,%s,%d,%d,%d,%d,%d,%d,%d\n",
			i, C.GoString(layer.name), layer.dir,
			layer.width, layer.pitch,
			layer.track_begin, layer.track_end,
			layer.viadef_begin, layer.viadef_end)
	}

	fmt.Fprintf(txtfile, "viadefs,%d\n", len(chip.Viadefs))
	for i, viadef := range chip.Viadefs {
		fmt.Fprintf(txtfile, "viadef%d,%s,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
			i, C.GoString(viadef.name),
			viadef.top.la, viadef.top.x, viadef.top.y, viadef.top.w, viadef.top.h,
			viadef.bottom.la, viadef.bottom.x, viadef.bottom.y, viadef.bottom.w, viadef.bottom.h)
	}

	fmt.Fprintf(txtfile, "tracks,%d\n", len(chip.Tracks))
	for i, track := range chip.Tracks {
		fmt.Fprintf(txtfile, "track%d,%d,%d,%d,%d\n",
			i, track.la, track.line, track.tmin, track.tmax)
	}

	log.Info().Msgf(
		"txt saved: %s[%d,%d,%d,%d], nets %d, pins %d, boxes %d, "+
			"vias %d, layers %d, viadefs %d, tracks %d",
		C.GoString(chip.Design.name),
		chip.Design.xmin, chip.Design.ymin, chip.Design.xmax, chip.Design.ymax,
		chip.NN, chip.NP, chip.NB, chip.NV, chip.NL, chip.NVD, chip.NT)
}

func (chip *jsonChip) readFields(s *bufio.Scanner, prefix string) []string {
	if !s.Scan() {
		panic(fmt.Sprintf("%s: scan error %s", prefix, s.Err()))
	}
	fields := strings.Split(s.Text(), ",")
	if fields[0] != prefix {
		panic(fmt.Sprintf("%s: found %v", prefix, fields))
	}
	return fields[1:]
}

func convInt(str string) int {
	i, err := strconv.Atoi(str)
	if err != nil {
		panic(err)
	}
	return i
}

func (chip *jsonChip) loadTXT(filename string) {
	log.Info().Msgf("loading %s ...", filename)

	var reader io.Reader
	txtfile, err := os.Open(filename)
	if err != nil {
		txtfile, err = os.Open(filename + ".bz2")
		if err != nil {
			panic(err)
		} else {
			reader = bzip2.NewReader(txtfile)
		}
	} else {
		reader = txtfile
	}
	defer txtfile.Close()

	var fields []string

	s := bufio.NewScanner(reader)

	fields = chip.readFields(s, "design")
	chip.Design.name = C.CString(fields[0])
	chip.Design.xmin = C.int(convInt(fields[1]))
	chip.Design.ymin = C.int(convInt(fields[2]))
	chip.Design.xmax = C.int(convInt(fields[3]))
	chip.Design.ymax = C.int(convInt(fields[4]))

	fields = chip.readFields(s, "nets")
	chip.NN = convInt(fields[0])
	chip.Nets = make([]C.struct_net, 0, chip.NN)
	for i := 0; i < chip.NN; i++ {
		fields = chip.readFields(s, "net"+strconv.Itoa(i))
		chip.Nets = append(chip.Nets, C.struct_net{
			name:      C.CString(fields[0]),
			ndr_index: C.ulong(convInt(fields[1])),
			use:       C.int(convInt(fields[2])),
			pin_begin: C.ulong(convInt(fields[3])),
			pin_end:   C.ulong(convInt(fields[4]))})
	}

	fields = chip.readFields(s, "pins")
	chip.NP = convInt(fields[0])
	chip.Pins = make([]C.struct_pin, 0, chip.NP)
	for i := 0; i < chip.NP; i++ {
		fields = chip.readFields(s, "pin"+strconv.Itoa(i))
		chip.Pins = append(chip.Pins, C.struct_pin{
			name:      C.CString(fields[0]),
			box_begin: C.ulong(convInt(fields[1])),
			box_end:   C.ulong(convInt(fields[2])),
			via_begin: C.ulong(convInt(fields[3])),
			via_end:   C.ulong(convInt(fields[4]))})
	}

	fields = chip.readFields(s, "boxes")
	chip.NB = convInt(fields[0])
	chip.Boxes = make([]C.struct_box, 0, chip.NB)
	for i := 0; i < chip.NB; i++ {
		fields = chip.readFields(s, "box"+strconv.Itoa(i))
		chip.Boxes = append(chip.Boxes, C.struct_box{
			la: C.int(convInt(fields[0])),
			x:  C.int(convInt(fields[1])),
			y:  C.int(convInt(fields[2])),
			w:  C.int(convInt(fields[3])),
			h:  C.int(convInt(fields[4]))})
	}

	fields = chip.readFields(s, "vias")
	chip.NV = convInt(fields[0])
	chip.Vias = make([]C.struct_via, 0, chip.NV)
	for i := 0; i < chip.NV; i++ {
		fields = chip.readFields(s, "via"+strconv.Itoa(i))
		chip.Vias = append(chip.Vias, C.struct_via{
			viadef_index: C.int(convInt(fields[0])),
			x:            C.int(convInt(fields[1])),
			y:            C.int(convInt(fields[2]))})
	}

	fields = chip.readFields(s, "layers")
	chip.NL = convInt(fields[0])
	chip.Layers = make([]C.struct_layer, 0, chip.NL)
	for i := 0; i < chip.NL; i++ {
		fields = chip.readFields(s, "layer"+strconv.Itoa(i))
		chip.Layers = append(chip.Layers, C.struct_layer{
			name:         C.CString(fields[0]),
			dir:          C.int(convInt(fields[1])),
			width:        C.int(convInt(fields[2])),
			pitch:        C.int(convInt(fields[3])),
			track_begin:  C.ulong(convInt(fields[4])),
			track_end:    C.ulong(convInt(fields[5])),
			viadef_begin: C.ulong(convInt(fields[6])),
			viadef_end:   C.ulong(convInt(fields[7]))})
	}

	fields = chip.readFields(s, "viadefs")
	chip.NVD = convInt(fields[0])
	chip.Viadefs = make([]C.struct_viadef, 0, chip.NVD)
	for i := 0; i < chip.NVD; i++ {
		fields = chip.readFields(s, "viadef"+strconv.Itoa(i))
		chip.Viadefs = append(chip.Viadefs, C.struct_viadef{
			name: C.CString(fields[0]),
			top: C.struct_box{
				la: C.int(convInt(fields[1])),
				x:  C.int(convInt(fields[2])),
				y:  C.int(convInt(fields[3])),
				w:  C.int(convInt(fields[4])),
				h:  C.int(convInt(fields[5]))},
			bottom: C.struct_box{
				la: C.int(convInt(fields[6])),
				x:  C.int(convInt(fields[7])),
				y:  C.int(convInt(fields[8])),
				w:  C.int(convInt(fields[9])),
				h:  C.int(convInt(fields[10]))}})
	}

	fields = chip.readFields(s, "tracks")
	chip.NT = convInt(fields[0])
	chip.Tracks = make([]C.struct_track, 0, chip.NT)
	for i := 0; i < chip.NT; i++ {
		fields = chip.readFields(s, "track"+strconv.Itoa(i))
		chip.Tracks = append(chip.Tracks, C.struct_track{
			la:   C.int(convInt(fields[0])),
			line: C.int(convInt(fields[1])),
			tmin: C.int(convInt(fields[2])),
			tmax: C.int(convInt(fields[3]))})
	}

	log.Info().Msgf(
		"txt loaded: %s[%d,%d,%d,%d], nets %d, pins %d, boxes %d, "+
			"vias %d, layers %d, viadefs %d, tracks %d",
		C.GoString(chip.Design.name),
		chip.Design.xmin, chip.Design.ymin, chip.Design.xmax, chip.Design.ymax,
		chip.NN, chip.NP, chip.NB, chip.NV, chip.NL, chip.NVD, chip.NT)
}
