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

parser = argparse.ArgumentParser('Completion Time Plots')
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
parser.add_argument('--lnd-retry-data',
        action='store_true',
        help='whether to parse overall lnd retry information')

# collect all arguments
args = parser.parse_args()
topo = args.topo
credit = args.credit
demand = args.demand
path_type = args.path_type
num_paths = args.path_num
scheme_list = args.scheme_list

succ_retries, fail_retries = [],[]
output_file = open(GGPLOT_DATA_DIR + args.save, "w+")
output_file.write("Topo,CreditType,Scheme,Credit,SizeStart,SizeEnd,Point,AvgCompTime,TailCompTime90,TailCompTime99,Demand\n")

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

# parse and print the retry data for LND
def collect_retry_stats(succ_retry_file, fail_retry_file):
    for i, file_name in enumerate([succ_retry_file, fail_retry_file]):
        retries = fail_retries if i else succ_retries
        try:
            with open(RESULT_DIR + file_name) as f:
                for line in f:
                    parts = line.split()
                    for t in parts:
                        retries.append(float(t))
        except IOError:
            print("error with", file_name)
            continue



# go through all relevant files and aggregate probability by size
for scheme in scheme_list:
    size_to_comp_times = {} 
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

        if scheme == 'lndBaseline' and args.lnd_retry_data:
            succ_retry_file = file_name + "_LIFO_succRetries.txt"
            fail_retry_file = file_name + "_LIFO_failRetries.txt"
            collect_retry_stats(succ_retry_file, fail_retry_file)
            
        file_name += "_LIFO_tailCompBySize.txt"
        try:
            with open(RESULT_DIR + file_name) as f:
                for line in f:
                    parts = line.split()
                    size = float(parts[0][:-1])
                    bucket = buckets[np.searchsorted(buckets, size)]
                    comp_times = size_to_comp_times.get(bucket, [])
                    for t in parts[1:]:
                        comp_times.append(float(t))
                    size_to_comp_times[bucket] = comp_times
        except IOError:
            print("error with", file_name)
            continue
     

    sorted_sizes = [5]
    sorted_sizes.extend(sorted(size_to_comp_times.keys()))
    print(sorted_sizes)
    if scheme == 'lndBaseline' and args.lnd_retry_data:
        print("Successful transaction LND retries Average:", np.average(np.array(succ_retries)), \
                "99%:" , np.percentile(np.array(succ_retries), 99))
        print("Failed transaction retries LND Average:", np.average(np.array(fail_retries)), \
                "99%:" , np.percentile(np.array(fail_retries), 99))
    
    for i, size in enumerate(sorted_sizes[1:]):
        comp_times = np.array(size_to_comp_times[size])
        output_file.write(topo_type + "," + credit_type + "," + \
                str(SCHEME_CODE[scheme]) +  "," + str(credit) +  "," + \
            "%f,%f,%f,%f,%f,%f,%f\n" % (sorted_sizes[i], size, \
                    math.sqrt(size * sorted_sizes[i]), \
                    np.average(comp_times), np.percentile(comp_times, 90), np.percentile(comp_times, 99),
                     demand))

output_file.close()
