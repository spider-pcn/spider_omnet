import sys
import argparse
import statistics as stat
from config import *

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
parser.add_argument('--path-type-list',
        nargs="*",
        help='types of paths to collect data for', default=["shortest"])
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
credit_list = args.credit_list
demand = args.demand
path_type_list = args.path_type_list
scheme_list = args.scheme_list





output_file = open(PLOT_DIR + args.save, "w+")
output_file.write("Scheme,Size,Prob\n")


# go through all relevant files and aggregate info
for credit in credit_list:
    for scheme in scheme_list:
        for path_type in path_type_list:
            size_to_arrival = {} 
            size_to_completion = {}
            for run_num in range(1, args.num_max + 1):
                file_name = topo + "_" + args.payment_graph_type + "_" + str(credit) + "_" + scheme + "_" + \
                        args.payment_graph_type + str(run_num) + \
                    "_demand" + str(demand/10) + "_" + path_type
                if scheme != "shortestPath":
                    filename += "_" + str(num_paths)
                filename += "-#0.sca"
                
                with open(SUMMARY_DIR + file_name) as f:
                    for line in f:
                        if line.startswith("Success ratio"):
                            succ_ratio = float(line.split(" ")[4])
                        elif line.startswith("Success volume"):
                            succ_volume = float(line.split(" ")[5])
                        elif line.startswith("Avg completion time"):
                            comp_time = float(line.split(" ")[3][:-1])
                    succ_ratios.append(succ_ratio)
                    succ_vols.append(succ_volume)
                    comp_times.append(comp_time)

            output_file.write(str(SCHEME_CODE[scheme]) + "," + str(run_num) + "," + str(credit) +  "," + \
                    path_type + "," + "%f,%f,%f,%f,%f,%f,%f,%f,%f\n" % (stat.mean(succ_ratios), min(succ_ratios), \
                    max(succ_ratios), stat.mean(succ_vols), min(succ_vols),  max(succ_vols), \
                    stat.mean(comp_times), min(comp_times), max(comp_times)))
output_file.close()
