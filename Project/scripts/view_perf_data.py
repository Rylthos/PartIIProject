import json
import sys

import matplotlib.pyplot as plt

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
errorbar()
memory_use()
memory_per_voxel()
