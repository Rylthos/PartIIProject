from helper import *

import numpy as np
import matplotlib.pyplot as plt

from pathlib import Path

def barpoints(data, path):
    ax = plt.subplot()

    ax.set_title("Frame times")
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

        ax.bar(x + offsets, medians, w, label=f"{id}", color=name_to_colour(id, "tab20b"))
        for j in range(len(frames)):
            frame = frames[j]
            ax.scatter([x[j] + offsets[j]] * len(frame), frame, alpha=0.1, color="red")

        if realtime_line:
            ax.hlines(16, minX, maxX, linestyles="--")

    plt.legend()

    parent_path = Path(path).parent
    file_name = Path(path).stem

    plt.savefig(f"{parent_path}/{file_name}.png")

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

def size_ratio(data):
    ids = list(set([f["id"] for f in data]))
    structures = list(set([f["structure"] for f in data]))

    ids.sort()
    structures.sort()

    labels = [s for s in structures]

    for id in ids:
        datapoints = []
        for structure in structures:
            frame = find(data, id, structure)

            total = total_voxels(frame)
            actual = frame["stats"]["voxels"]

            ratio = actual / total

            datapoints.append(ratio)

        plt.bar(labels, datapoints, label=f"{id}")
        break

    plt.show()
