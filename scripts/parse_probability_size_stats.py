import sys
import argparse
import statistics as stat
from config import *
import shlex
import numpy as np
import math

# figure out what the size buckets should be for a given number of buckets
# say you want 20 buckets, you want to make them equally sized in the number
# of transactions in a bucket (based on the skew of transaction sizes), so the
# larger transactions span a wider range but at the smaller end, the buckets
# are narrower
def compute_buckets(num_buckets, dist_filename):
    amt_dist = np.load(dist_filename)
    num_amts = amt_dist.item().get('p').size
    pdf = amt_dist.item().get('p')
    cdf = np.cumsum(pdf)

    gap = 1.0 / num_buckets
    break_point = gap
    buckets = []

    # return all the bucket end markers
    for i, c in enumerate(cdf):
        if c >= break_point:
            print(break_point, i, c)
            buckets.append(int(round(amt_dist.item().get('bins')[i], 1)))
            break_point += gap
    # buckets.append(int(round(amt_dist.item().get('bins')[-1], 1)))
    print(buckets, len(buckets))
    return buckets

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
parser.add_argument('--num-buckets',
        type=int,
        help='Single number denoting the maximum number of buckets to group txn sizes into', default="20")

# collect all arguments
args = parser.parse_args()
topo = args.topo
credit = args.credit
demand = args.demand
path_type = args.path_type
num_paths = args.path_num
scheme_list = args.scheme_list


output_file = open(GGPLOT_DATA_DIR + args.save, "w+")
output_file.write("Topo,CreditType,Scheme,Credit,SizeStart,SizeEnd,Point,Prob,Demand\n")

buckets = compute_buckets(args.num_buckets, KAGGLE_AMT_DIST_FILENAME)

if "sw" in args.topo or "sf" in args.topo:
    topo_type = args.save[:2]
else:
    topo_type = args.save[:3]

if "lnd_uniform" in args.topo: 
    credit_type = "uniform"
elif "lnd_july15" in args.topo or "lndCap" in args.topo:
    credit_type = "lnd"
else:
    credit_type = "uniform"


# go through all relevant files and aggregate probability by size
for scheme in scheme_list:
    size_to_arrival = {} 
    size_to_completion = {}
    for run_num in range(0, args.num_max + 1):
        if credit_type != "uniform" and (scheme == "waterfilling" or scheme == "DCTCPQ"):
            path_type = "widest"
        else:
            path_type = "shortest"

        file_name = topo + "_" + args.payment_graph_type + "_net_" + str(credit) + "_" + scheme + "_" + \
                args.payment_graph_type + str(run_num) + \
            "_demand" + str(demand/10) + "_" + path_type
        if scheme != "shortestPath":
            file_name += "_" + str(num_paths)
        file_name += "-#0.sca"
        
        try:
            with open(RESULT_DIR + file_name) as f:
                for line in f:
                    if "size" in line:
                        parts = shlex.split(line)
                        num_completed = float(parts[-1])
                        sub_parts = parts[-2].split()
                        size = int(sub_parts[1][:-1])
                        num_arrived = float(sub_parts[3][1:-1]) + 1
                        bucket = buckets[np.searchsorted(buckets, size)]
                        if num_arrived > 0:
                            if num_arrived < num_completed:
                                print("problem with ", scheme, " on run ", run_num)
                                print("Num arrived", num_arrived, "num completed", num_completed, "for size", size)
                                num_arrived = num_completed
                            size_to_arrival[bucket] = size_to_arrival.get(bucket, 0) + num_arrived
                            size_to_completion[bucket] = size_to_completion.get(bucket, 0) + num_completed
        except IOError:
            print("error with", file_name)
            continue
     

    sorted_sizes = [5]
    sorted_sizes.extend(sorted(size_to_completion.keys()))
    print(sorted_sizes)
    for i, size in enumerate(sorted_sizes[1:]):
        output_file.write(topo_type + "," + credit_type + "," + \
                str(SCHEME_CODE[scheme]) +  "," + str(credit) +  "," + \
            "%f,%f,%f,%f,%f\n" % (sorted_sizes[i], size, \
                    math.sqrt(size * sorted_sizes[i]), \
                    size_to_completion[size]/size_to_arrival[size], demand))
output_file.close()
