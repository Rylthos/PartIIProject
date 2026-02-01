from helper import *

import numpy as np
import matplotlib.pyplot as plt

from pathlib import Path

def frame_times(data, path):
    parent_path = Path(path).parent
    file_name = Path(path).stem

    ax = plt.subplot()

    ax.set_title(f"{get_title(file_name)} Frame Times")
    ax.set_ylabel("Frame time (ms)")

    ids, structures, w, x = calculate_bar_sizes(data)

    plt.xticks(x, [s for s in structures])


    for i, id in enumerate(ids):
        datapoints = []
        offsets = []

        minX = 10000
        maxX = -10000

        realtime_line = False

        for structure in structures:
            frame = find(data, id, structure)

            left_side = -(len(ids) * w / 2)
            offset = left_side + (i * w) + w / 2

            frametime, median, q1, q3 = get_frametimes(frame)

            if any(np.array(frametime) > 16):
                realtime_line = True

            minX = min(minX, min(x + offset - w))
            maxX = max(maxX, max(x + offset + w))

            offsets.append(offset)
            datapoints.append((median, frametime[q1:q3]))

        medians = [d[0] for d in datapoints]
        frames = [d[1] for d in datapoints]

        ax.set_xbound(minX, maxX)

        ax.bar(x + offsets, medians, w, label=f"{id}", color=name_to_colour(ids, id, "tab10"))
        for j in range(len(frames)):
            frame = frames[j]
            ax.scatter([x[j] + offsets[j]] * len(frame), frame, alpha=0.1, color="red")

        if realtime_line:
            ax.hlines(16, minX, maxX, linestyles="--")

    plt.legend()


    plt.savefig(f"{parent_path}/{file_name}_frame_times.png")

    plt.show()

def violin(data):
    ax = plt.subplot();

    ax.set_title("Frame times")
    ax.set_ylabel("Frame time (ms)")

    ids, structures, w, x = calculate_bar_sizes(data)

    ax.set_xticks(x, [i for i in structures])

    for i, id in enumerate(ids):
        offsets = []
        datapoints = []
        for structure in structures:
            frame = find(data, id, structure)

            left_side = -(len(ids) * w / 2)
            offset = left_side + (i * w) + w / 2

            frametimes, median, q1, q3 = get_frametimes(frame)
            offsets.append(offset)
            datapoints.append(frametimes)

        vp = ax.violinplot(datapoints, positions=x + offsets, widths=w * 0.9, showmeans=True)

        vp["cmeans"].set_color("red")
        vp["cmeans"].set_linestyle("--")

        for body in vp['bodies']:
            body.set_facecolor(name_to_colour(id, "tab20b"))
            body.set_edgecolor("black")

    plt.legend()
    plt.show()

def voxel_ratio(data, path):
    parent_path = Path(path).parent
    file_name = Path(path).stem

    ax = plt.subplot()

    ids = list(set([f["id"] for f in data]))
    structures = list(set([f["structure"] for f in data]))

    ids.sort()
    structures.sort()

    labels = [s for s in structures]

    ax.set_title(f"{get_title(file_name)} Voxel Ratio")
    ax.set_ylabel("Voxel Ratio")

    for id in ids:
        datapoints = []
        for structure in structures:
            frame = find(data, id, structure)

            total = total_voxels(frame)
            actual = frame["stats"]["voxels"]

            # ratio = np.log10(total / actual)
            ratio = actual / total

            datapoints.append(ratio)

        plt.bar(labels, datapoints, label=f"{id}")
        break

    plt.savefig(f"{parent_path}/{file_name}_voxel_ratio.png")

    plt.show()

def total_memory(data, path):
    parent_path = Path(path).parent
    file_name = Path(path).stem

    ax = plt.subplot()

    ids = list(set([f["id"] for f in data]))
    structures = list(set([f["structure"] for f in data]))

    ids.sort()
    structures.sort()

    labels = [s for s in structures]

    ax.set_title(f"{get_title(file_name)} Memory Use")
    ax.set_ylabel("Memory use (MiB)")

    for id in ids:
        datapoints = []
        for structure in structures:
            frame = find(data, id, structure)

            memory = frame["stats"]["memory"] / (1024 ** 2)

            datapoints.append(memory)

        plt.bar(labels, datapoints, label=f"{id}")
        break

    plt.savefig(f"{parent_path}/{file_name}_total_memory.png")

    plt.show()

def per_voxel(data, path):
    parent_path = Path(path).parent
    file_name = Path(path).stem

    ax = plt.subplot()

    ids = list(set([f["id"] for f in data]))
    structures = list(set([f["structure"] for f in data]))

    ids.sort()
    structures.sort()

    labels = [s for s in structures]

    w = 0.4
    x = np.arange(len(labels))

    ax.set_title(f"{get_title(file_name)} Per Voxel Memory Use")
    ax.set_ylabel("Memory Per Voxel (Bytes)")

    ax.set_xticks(x)
    ax.set_xticklabels(labels)

    for id in ids:
        datapoints = []
        for structure in structures:
            frame = find(data, id, structure)

            memory = frame["stats"]["memory"]

            total = total_voxels(frame)
            actual = frame["stats"]["voxels"]

            datapoints.append((memory / total, memory / actual))


        plt.bar(x - w/4, [x[0] for x in datapoints], label=f"Per Total Voxels", alpha=0.7, width=w)
        plt.bar(x + w/4, [x[1] for x in datapoints], label=f"Per Visible Voxels", alpha=0.7, width=w)

        break

    plt.legend()
    plt.savefig(f"{parent_path}/{file_name}_per_voxel.png")

    plt.show()
