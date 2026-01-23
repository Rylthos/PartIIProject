import json
import hashlib

import matplotlib.pyplot as plt
import numpy as np

def parse_file(filename):
    lines = ""
    with open(filename, 'r') as f:
        lines = f.read()

    return json.loads(lines)["values"]

def name_to_colour(name, cmap):
    h = hashlib.md5(name.encode("utf-8")).hexdigest()

    idx = int(h, 16)

    plt_cmap = plt.get_cmap(cmap)
    return plt_cmap(idx % plt_cmap.N)

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
