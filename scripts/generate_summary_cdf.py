import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import argparse
import os
import json
import numpy as np
from parse_vec_files import *

parser = argparse.ArgumentParser('CDF')
parser.add_argument('--vec_files',
        type=str,
        required=True,
        nargs='+',
        help='Each vector file should be generated from a run using the omnet simulator')
parser.add_argument('--labels',
        type=str,
        required=True,
        nargs='+')
parser.add_argument('--save',
        type=str,
        required=True,
        help='The file to which to write the figure')

args = parser.parse_args()

fmts = ['r--o', 'b-^']



def compute_avg_path_completion_rates(filename):
    completion_fractions = []
    all_timeseries, vec_id_to_info_map = parse_vec_file(filename, "completion_rate_cdfs")

    # dictionary of src dest pairs to a map denoting the attempting rate or completion rate on each of
    # MAX_K paths each identified by path id
    avg_rate_attempted = dict()
    avg_rate_completed = dict()

    # aggregate information on a per src_Dest pair and per path level
    for vec_id, timeseries in all_timeseries.items():
        vector_details = vec_id_to_info_map[vec_id]
        src_dest_pair = (vector_details[0], vector_details[3])

        signal_name = vector_details[2]
        path_id = int(signal_name.split("_")[1])
        signal_values = [t[1] for t in timeseries]

        
        if "rateAttempted" in signal_name:
            cur_info = avg_rate_attempted.get(src_dest_pair, dict())
            cur_info[path_id] = np.average(np.array(signal_values))
            avg_rate_attempted[src_dest_pair] = cur_info
        elif "rateCompleted" in signal_name:
            cur_info = avg_rate_completed.get(src_dest_pair, dict())
            cur_info[path_id] = np.average(np.array(signal_values))
            avg_rate_completed[src_dest_pair] = cur_info
        else: 
            print "UNINTERESTING TIME SERIES OF VECTOR of type", signal_name, "FOUND"


    # aggregate all the information across all source destination pairs and paths onto 
    # a single list with the completion fractions for all of them
    for key in avg_rate_attempted.keys():
        rates_attempted_across_paths = avg_rate_attempted[key]
        rates_completed_across_paths = avg_rate_completed[key]

        for path_id, attempt_rate in rates_attempted_across_paths.items():
            if attempt_rate > 0: 
                completion_fractions.append(rates_completed_across_paths[path_id]/attempt_rate)

    return completion_fractions


# plot cdf of completion fractions across all paths used between different sources and destination pairs
# for the given set of algorithms
def plot_completion_rate_cdf(args):
    def bar_x_coord(bw, ix):
        return bw - (len(args.summaries)/2.0 - ix)*args.bar_width

    color_opts = ['#fa9e9e', '#a4e0f9', '#57a882', '#ad62aa']
    dims = plt.rcParams["figure.figsize"]
    plt.rcParams["figure.figsize"] = dims
    for i in range(len(args.vec_files)):
        scale = 1
        # returns all the data points for the completion rates for individual paths to every source dest pair
        res = compute_avg_path_completion_rates(args.vec_files[i])
        keys = sorted(res)
        ys = np.linspace(0.1, 1.0, 100)
        
        # if you want error bars
        #xerr_low = [k - data[k][0] for k in keys]
        #xerr_high = [data[k][1] - k for k in keys]
        #plt.errorbar(keys, ys, xerr=[keys, keys], fmt=fmts[i], label=args.labels[i], linewidth=3, markersize=15)
        plt.hist(keys, bins = 100, normed='True', cumulative='True', label=args.labels[i], \
                linewidth=3, histtype='step')

    plt.title('Completion rate cdfs for waterfilling') # TODO: put topology file here
    plt.xlabel('Achieved completion rates')
    plt.ylabel('CDF')
    plt.ylim(0,1.1)
    plt.legend(loc="upper left")
    plt.tight_layout()
    plt.savefig(args.save)


def main():
    matplotlib.rcParams['figure.figsize'] = [15, 10]
    plt.rc('font', size=40)          # controls default text sizes
    plt.rc('axes', titlesize=42)     # fontsize of the axes title
    plt.rc('axes', labelsize=40)    # fontsize of the x and y labels
    plt.rc('xtick', labelsize=32)    # fontsize of the tick labels
    plt.rc('ytick', labelsize=32)    # fontsize of the tick labels
    plt.rc('legend', fontsize=34)    # legend fontsize
    plot_completion_rate_cdf(args)

main()