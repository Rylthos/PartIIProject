import argparse

from plots import *
from helper import parse_file

def parse_args():
    parser = argparse.ArgumentParser(
            prog="View perf data",
            description="Graphs perf data recorderd from renderer"
            )

    parser.add_argument("perf_file")

    parser.add_argument("-f", "--frame-times", action="store_true")
    parser.add_argument("-v", "--violin", action="store_true")
    parser.add_argument("-r", "--ratio", action="store_true")
    parser.add_argument("-m", "--memory", action="store_true")
    parser.add_argument("-p", "--per-voxel", action="store_true")

    return parser.parse_args()

if __name__=="__main__":
    args = parse_args()

    data = parse_file(args.perf_file)

    if args.frame_times:
        frame_times(data, args.perf_file)
    if args.violin:
        violin(data)
    if args.ratio:
        voxel_ratio(data, args.perf_file)
    if args.memory:
        total_memory(data, args.perf_file)
    if args.per_voxel:
        per_voxel(data, args.perf_file)
