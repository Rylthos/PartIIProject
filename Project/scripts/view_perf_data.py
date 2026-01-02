import json
import sys

import matplotlib.pyplot as plt
import numpy as np

if (len(sys.argv) != 2):
    print("Wrong number of arguments")
    print(f"Usage: {sys.argv[0]} filepath")
    exit(-1)

lines = ""
with open(sys.argv[1], 'r') as f:
    lines = f.read()

data = json.loads(lines)["values"]

def parse_frames(data):
    frames = data["frametimes"]

    return [(i, x) for i, x in enumerate(frames)]

def stat_frames(data):
    frames = data["frametimes"]

    frames_ms = [x for x in frames]


    frames_ms = sorted(frames_ms)
    median = (len(frames_ms) + 1) // 2

    q1 = median // 2
    q3 = median + q1

    frames_ms = frames_ms[q1:q3]

    total = sum(frames_ms)

    mean = total / len(frames_ms)
    frame_min = min(frames_ms)
    frame_max = max(frames_ms)

    return mean, frame_max - mean,  mean - frame_min

def get_frametimes(data):
    frames = data["frametimes"];

    frames = sorted(frames)
    median = (len(frames) + 1) // 2
    q1 = median // 2
    q3 = median + q1

    return frames, frames[median], q1, q3

def bytes_per_voxel(data):
    stats = data["stats"]

    memory_bytes = stats["memory"];
    voxels = stats["voxels"];

    return memory_bytes / voxels

def scatter():
    fig = plt.figure()
    ax = fig.add_subplot(111)

    ax.set_title("Frame times")
    ax.set_xlabel("Frame")
    ax.set_ylabel("Frame time (ms)")
    for i in range(len(data)):
        frame = data[i]

        frametimes = parse_frames(data[i])
        x, y = zip(*frametimes)
        name = frame["structure"]
        ax.scatter(x, y, label=f"{name}")

    plt.legend()
    plt.show()

def errorbar():
    fig = plt.figure()
    ax = fig.add_subplot(111)

    ax.set_title("Frame times")
    ax.set_ylabel("Frame time (ms)")

    x = [f["structure"] for f in data]
    y = [stat_frames(f) for f in data]

    mean, err_high, err_low = zip(*y)

    err = [err_low, err_high]

    ax.bar(x, mean)
    ax.errorbar(x, mean, yerr = err, fmt='o', color="r")

    plt.show()

def barpoints():
    fig = plt.figure()
    ax = fig.add_subplot(111)

    ax.set_title("Frame times")
    ax.set_ylabel("Frame time (ms)")

    ids = set([f["id"] for f in data])
    structures = set([f["structure"] for f in data])

    ids = dict([(x, i) for i, x in enumerate(ids)])
    structures = dict([(x, i) for i, x in enumerate(structures)])

    w = 1. / (len(ids) + 1)
    x = np.arange(len(structures))

    plt.xticks(x, [i for i in structures])

    for id in ids:
        data_id = [f for f in data if f["id"] == id]

        left_side = -(len(ids) * w / 2)
        offset = left_side + (ids[id] * w) + w / 2

        frametimes = [get_frametimes(f) for f in data_id]
        frametimes, medians, q1, q3 = zip(*frametimes)

        ax.bar(x + offset, medians, w, label=f"{id}")

        for i, frames in enumerate(frametimes):
            cut_frames = frames[q1[i]:q3[i]]
            ax.scatter([x[i] + offset] * len(cut_frames), cut_frames, alpha=0.1, color="red")

    plt.legend()
    plt.show()

def memory_use():
    fig = plt.figure()
    ax = fig.add_subplot(111)

    ax.set_title("Memory usage")
    ax.set_ylabel("Memory usage (bytes)")

    x = [f["structure"] for f in data]
    y = [f["stats"]["memory"] for f in data]

    ax.bar(x, y)

    plt.show()

def memory_per_voxel():
    fig = plt.figure()
    ax = fig.add_subplot(111)

    ax.set_title("Memory usage")
    ax.set_ylabel("Bytes per voxels")

    x = [f["structure"] for f in data]
    y = [bytes_per_voxel(f) for f in data]

    ax.bar(x, y)

    plt.show()

# scatter()
# errorbar()
barpoints()
# memory_use()
# memory_per_voxel()
