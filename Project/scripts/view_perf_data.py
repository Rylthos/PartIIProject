import argparse

from plots import *
from helper import parse_file

def parse_args():
    parser = argparse.ArgumentParser(
            prog="View perf data",
            description="Graphs perf data recorderd from renderer"
            )

    parser.add_argument("perf_file")

    parser.add_argument("-b", "--bar", action="store_true")
    parser.add_argument("-v", "--violin", action="store_true")
    parser.add_argument("-r", "--ratio", action="store_true")

    return parser.parse_args()

if __name__=="__main__":
    args = parse_args()

    data = parse_file(args.perf_file)

    if args.bar:
        barpoints(data)
    if args.violin:
        violin(data)
    if args.ratio:
        size_ratio(data)
