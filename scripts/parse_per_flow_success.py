import sys
import argparse
import statistics as stat
from config import *
import shlex
import math

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
parser.add_argument('--measurement-interval',
        type=int,
        help='Number of seconds over which success was measured', default="200")


# collect all arguments
args = parser.parse_args()
topo = args.topo
credit = args.credit
demand = args.demand
path_type = args.path_type
num_paths = args.path_num
scheme_list = args.scheme_list


# determine topology and credit type
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




output_file = open(GGPLOT_DATA_DIR + args.save, "w+")
output_file.write("Topo,CreditType,Scheme,Credit,SuccVol,Demand\n")
print("scheme objective numberOfZeroes")


# go through all relevant files and aggregate probability by size
for i, scheme in enumerate(scheme_list):
    flow_succ_list, flow_arrival_list = [], []
    for run_num in range(0, args.num_max + 1):
        if credit_type != "uniform" and (scheme == "waterfilling" or scheme == "DCTCPQ" or scheme == "priceSchemeWindow"):
            path_type = "widest"
        else:
            path_type = "shortest"

        file_name = topo + "_" + args.payment_graph_type + "_net_" + str(credit) + "_" + scheme + "_" + \
                args.payment_graph_type + str(run_num) + \
            "_demand" + str(demand/10) + "_" + path_type
        if scheme != "shortestPath":
            file_name += "_" + str(num_paths)
        file_name += "-#0.sca"
        
        # num_completed will always be populated first and the next
        # entry will be arrived for the same flow
        # based on file structure
        with open(RESULT_DIR + file_name) as f:
            for line in f:
                if "->" in line:
                    if "amtCompleted" in line:
                        parts = shlex.split(line)
                        num_completed = float(parts[-1])
                    if "amtArrived" in line:
                        parts = shlex.split(line)
                        num_arrived = float(parts[-1])
                        flow_arrival_list.append(num_arrived / args.measurement_interval)
                        #flow_succ_list.append(num_completed * 100 /num_arrived)
                        flow_succ_list.append(num_completed/ args.measurement_interval)

    sum_fairness, count_zero = 0.0, 0.0
    for entry in sorted(flow_succ_list):
        if entry == 0:
            count_zero += 1
        else:
            sum_fairness += math.log(entry, 2)
        output_file.write(topo_type + "," + credit_type + "," + \
                str(SCHEME_CODE[scheme]) +  "," + str(credit) +  "," + \
            "%f,%f\n" % (entry, demand))
    print(scheme, sum_fairness/(args.num_max + 1), count_zero)

    if i == 0:
        for entry in sorted(flow_arrival_list):
            output_file.write(topo_type + "," + credit_type + \
                    ",arrival," + str(credit) +  "," + \
                "%f,%f\n" % (entry, demand))

output_file.close()
