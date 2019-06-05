import sys
import argparse
import statistics as stat
from config import *
import shlex

delay = 30

parser = argparse.ArgumentParser('Analysis Plots')
parser.add_argument('--topo',
        type=str, 
        required=True,
        help='what topology to generate size summary for')
parser.add_argument('--payment-graph-type',
        type=str, 
        help='what graph type topology to generate summary for', default="circ")
parser.add_argument('--credit',
        type=int,
        help='Credit to collect stats for', default=10)
parser.add_argument('--demand',
        type=int,
        help='Single number denoting the demand to collect data for', default="30")
parser.add_argument('--path-type',
        type=str,
        help='types of paths to collect data for', default="shortest")
parser.add_argument('--path-num',
        type=int,
        help='number of paths to collect data for', default=4)
parser.add_argument('--scheme-list',
        nargs="*",
        help='set of schemes to aggregate results for', default=["priceSchemeWindow"])
parser.add_argument('--save',
        type=str, 
        required=True,
        help='file name to save data in')
parser.add_argument('--num-max',
        type=int,
        help='Single number denoting the maximum number of runs to aggregate data over', default="5")

# collect all arguments
args = parser.parse_args()
topo = args.topo
credit = args.credit
demand = args.demand
path_type = args.path_type
num_paths = args.path_num
scheme_list = args.scheme_list





output_file = open(PLOT_DIR + args.save, "w+")
output_file.write("Scheme,Credit,Size,Prob,Demand\n")


# go through all relevant files and aggregate probability by size
for scheme in scheme_list:
    size_to_arrival = {} 
    size_to_completion = {}
    for run_num in range(0, args.num_max + 1):
        file_name = topo + "_" + args.payment_graph_type + "_net_" + str(credit) + "_" + scheme + "_" + \
                args.payment_graph_type + str(run_num) + \
            "_demand" + str(demand/10) + "_" + path_type
        if scheme != "shortestPath":
            file_name += "_" + str(num_paths)
        file_name += "-#0.sca"
        
        with open(RESULT_DIR + file_name) as f:
            for line in f:
                if "size" in line:
                    parts = shlex.split(line)
                    num_completed = float(parts[-1])
                    sub_parts = parts[-2].split()
                    size = int(sub_parts[1][:-1])
                    num_arrived = float(sub_parts[3][1:-1])
                    if num_arrived > 0:
                        size_to_arrival[size] = size_to_arrival.get(size, 0) + num_arrived
                        size_to_completion[size] = size_to_completion.get(size, 0) + num_completed

    for size in sorted(size_to_completion.keys()):
        output_file.write(str(SCHEME_CODE[scheme]) +  "," + str(credit) +  "," + \
            "%f,%f,%f\n" % (size, size_to_completion[size]/size_to_arrival[size], demand))
output_file.close()
