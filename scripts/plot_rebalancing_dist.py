import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import sys
from parse_sca_files import *
import argparse
from config import *
import numpy

delay = 30
RESULT_PATH_PREFIX = "../benchmarks/dag"

parser = argparse.ArgumentParser('Analysis Plots')
parser.add_argument('--topo',
        type=str, 
        required=True,
        help='what topology to generate summary for')
parser.add_argument('--credit-list',
        nargs="+",
        required=True,
        help='Credits to collect stats for')
parser.add_argument('--demand',
        type=int,
        help='Single number denoting the demand to collect data for', default="30")
parser.add_argument('--scheduling-alg',
        type=str,
        help='scheduling algorithm to collect info for', default="LIFO")
parser.add_argument('--path-type',
        type=str,
        help='types of paths to collect info for', default="widest")
parser.add_argument('--queue-threshold',
        type=int,
        help='queue threshold to collect info for', default=None)
parser.add_argument('--dag-percent',
        type=int,
        help='dag percents to collect info for', default=[None])
parser.add_argument('--path-num',
        type=int,
        help='number of paths to collect data for', default=4)
parser.add_argument('--scheme-list',
        nargs="*",
        help='set of schemes to aggregate results for', default=["DCTCPQ"])
parser.add_argument('--save',
        type=str, 
        required=True,
        help='file name to save data in')
parser.add_argument('--num-max',
        type=int,
        help='Single number denoting the maximum number of runs to aggregate data over', default="5")
parser.add_argument('--rebalancing-rate-list',
        nargs="*",
        type=int,
        help='list of rebalancing frequencies', default=[None])

# collect all arguments
args = parser.parse_args()
topo = args.topo
credit_list = args.credit_list
demand = args.demand
path_type = args.path_type
scheme_list = args.scheme_list
num_paths = args.path_num
queue_threshold = args.queue_threshold
dag_percent = args.dag_percent
scheduling_alg = args.scheduling_alg
rebalancing_rate_list = args.rebalancing_rate_list

if "lnd_uniform" in args.topo: 
    credit_type = "uniform"
elif "lnd_july15" in args.topo or "lndCap" in args.topo:
    credit_type = "lnd"
else:
    credit_type = "uniform"

# compute how many nodes contribute to how much of the rebalancing
def compute_x_y(array):
    sorted_array = sorted(array)
    cum_sum = numpy.cumsum([i / sum(sorted_array) for i in sorted_array])
    new_x = [float(i) / len(array) for i, _ in enumerate(array)]
    return new_x, cum_sum


# plots histogram
def plot_rebalancing_dist(amt_list, num_list, alg):
    n_bins = 50
    fig, axs = plt.subplots(1, 2, tight_layout=True)

    amt_x_y = compute_x_y(amt_list)
    axs[0].plot(amt_x_y[0], amt_x_y[1])
    axs[0].set_xlabel("Fraction of nodes")
    axs[0].set_ylabel("Contribution to total amount rebalanced")
    
    num_x_y = compute_x_y(num_list)
    axs[1].plot(num_x_y[0], num_x_y[1])
    axs[1].set_xlabel("Fraction of nodes")
    axs[1].set_ylabel("Contribution to number of rebalancing events")
   
    st = plt.suptitle("Rebalancing Distribution for " + alg)
    # shift subplots down:
    st.set_y(0.98)
    fig.subplots_adjust(top=0.65)
    fig.savefig(args.save)

# go through all relevant files and aggregate probability by size
aggregate_rebalancing_amt_list = []
aggregate_rebalancing_num_list = []
for scheme in scheme_list:
    for run_num in range(0, args.num_max + 1):
        for credit in credit_list:
            for rebalancing_rate in rebalancing_rate_list:
                if credit_type != "uniform" and (scheme == "waterfilling" or scheme == "DCTCPQ"):
                    path_type = "widest"
                else:
                    path_type = "shortest"

                file_name = topo + "_dag" +  str(dag_percent) + "_net_" + str(credit) + "_" + scheme + "_" + \
                        "dag" + str(run_num) + \
                    "_demand" + str(demand/10) + "_" + path_type
                if scheme != "shortestPath":
                    file_name += "_" + str(num_paths)
                file_name += "_" + scheduling_alg + "_rebalancing1_rate" + str(rebalancing_rate) 
                if queue_threshold is not None and scheme == "DCTCPQ":
                    file_name += "_qd" + str(queue_threshold)
                file_name += "-#0.sca"

                # collect stats
                result_path = RESULT_PATH_PREFIX + str(dag_percent) + "/results/"
                (rebalancing_amt_list, rebalancing_num_list, stats_str) = parse_sca_files_overall(result_path + file_name)
                norm_rebalancing_amt = [x / sum(rebalancing_amt_list)  for x in rebalancing_amt_list]
                norm_rebalancing_num = [x / sum(rebalancing_num_list)  for x in rebalancing_num_list]
                aggregate_rebalancing_amt_list.extend(norm_rebalancing_amt)
                aggregate_rebalancing_num_list.extend(norm_rebalancing_num)
    
    # plot distribution
    alg = "Spider" if scheme == "DCTCPQ" else "LND"
    plot_rebalancing_dist(aggregate_rebalancing_amt_list, aggregate_rebalancing_num_list, alg)
