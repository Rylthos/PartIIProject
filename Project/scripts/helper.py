import json
import hashlib
from itertools import cycle

import matplotlib.pyplot as plt
import numpy as np

def get_title(filename):
    return " ".join([x.capitalize() for x in filename.split("_")])

def parse_file(filename):
    lines = ""
    with open(filename, 'r') as f:
        lines = f.read()

    return json.loads(lines)["values"]

def name_to_colour(all_ids, id, cmap):
    plt_cmap = plt.get_cmap(cmap)
    colourcyle = cycle(plt_cmap.colors)

    colours = {}
    for i in all_ids:
        colours[i] = next(colourcyle)

    return colours[id]

def parse_frames(data):
    frames = data["frametimes"]

    return [(i, x) for i, x in enumerate(frames)]

def get_frametimes(data):
    frames = data["frametimes"];

    frames = sorted(frames)
    median = (len(frames) + 1) // 2
    q1 = median // 2
    q3 = median + q1

    return frames, frames[median], q1, q3

def total_voxels(frame):
    dims = frame["dimensions"]
    return dims[0] * dims[1] * dims[2]

def calculate_bar_sizes(data):
    ids = list(set([f["id"] for f in data]))
    structures = list(set([f["structure"] for f in data]))

    ids.sort()
    structures.sort()

    w = 1. / (len(ids) + 1)
    x = np.arange(len(structures))

    return ids, structures, w, x

def find(data, id, structure):
    return list(filter(lambda f: f["id"] == id and f["structure"] == structure, data))[0]
