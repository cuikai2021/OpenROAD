#!/usr/bin/python3

import json
import logging
import subprocess as sp
import glob
from pathlib import Path

in_dir = Path('/opt/in')
out_dir = Path('/opt/out')
design_json = out_dir.joinpath('design-inspector.metrics.json')

def design_inspector(design_inspector_cmd, lef_dir, def_dir):
    lef_path = sorted(glob.glob(f'{lef_dir}/*.lef'))
    def_path = sorted(glob.glob(f'{def_dir}/*.def'))
    assert len(def_path) == 1
    cmd = [design_inspector_cmd, '--json-output'] + def_path + ['--lef'] + lef_path
    try:
        with open(design_json, 'w') as file:
            sp.check_call(cmd, stdout=file)
        logging.info('succeeded to run design_inspector commands.')
    except:
        logging.error('failed to run design_inspector commands')
        raise

def extract_metrics():
    placement_metrics = out_dir.joinpath('placement_metrics.json')
    design = {}
    with open(design_json, 'r') as file:
        design.update(json.load(file))
    data = {}
    data['cell_count'] = design['chip']['cells']['total']
    data['cell_area'] = int(design['chip']['cells']['area']['total']['value']) / pow(10, 12)
    used_sites = int(design['chip']['sites']['used'])
    total_sites = int(design['chip']['sites']['total'])
    data['utilization'] = used_sites / total_sites
    with open(placement_metrics, 'w') as file:
        json.dump(data, file, indent=4)

if __name__ == '__main__':
    design_inspector('/benchmark/bin/design-inspector', in_dir.joinpath('lef'), in_dir.joinpath('def'))
    extract_metrics()
    if design_json.exists():
        design_json.unlink()
