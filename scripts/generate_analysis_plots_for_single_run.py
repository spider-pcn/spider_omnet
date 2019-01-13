import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import argparse
import os
import json
import numpy as np
from parse_vec_files import *
from matplotlib.backends.backend_pdf import PdfPages
from cycler import cycler
from config import *

parser = argparse.ArgumentParser('Analysis Plots')
parser.add_argument('--vec_file',
        type=str,
        required=True,
        help='Single vector file for a particular run using the omnet simulator')
parser.add_argument('--balance',
        action='store_true',
        help='Plot balance information for all routers')
parser.add_argument('--inflight',
        action='store_true',
        help='Plot inflight funds information for all routers (need to be used with --balance to work)')
parser.add_argument('--num_sent_per_channel',
        action='store_true',
        help='Plot number sent per channel for all routers')
parser.add_argument('--queue_info',
        action='store_true',
        help='Plot queue information for all routers')
parser.add_argument('--timeouts',
        action='store_true',
        help='Plot timeout information for all source destination pairs')
parser.add_argument('--frac_completed',
        action='store_true',
        help='Plot fraction completed for all source destination pairs')

parser.add_argument('--save',
        type=str,
        required=True,
        help='The pdf file prefix to which to write the figures')

args = parser.parse_args()

#fmts = ['r--', 'b-', 'g-.']

# returns a dictionary of the necessary stats where key is a router node
# and value is another dictionary where the key is the partner node 
# and value is the time series of the signal_type recorded for this pair of nodes
def aggregate_info_per_node(filename, signal_type, is_router):
    node_signal_info = dict()
    all_timeseries, vec_id_to_info_map = parse_vec_file(filename, signal_type)

    # aggregate information on a per router node basis
    # and then on a per channel basis for that router node
    for vec_id, timeseries in all_timeseries.items():
        vector_details = vec_id_to_info_map[vec_id]
        src_node = vector_details[0]
        src_node_type = vector_details[1]
        dest_node_type = vector_details[4]

        if signal_type is "balance":
            if (src_node_type == "router" and dest_node_type == "host") or \
                    (src_node_type == "host" and dest_node_type == "router"):
                        for t in timeseries:
                            if t[1] == 0:
                                print "End host " + str(src_node) + " hitting zero at time " + str(t[0])


        if is_router and (src_node_type != "router" or dest_node_type != "router"):
            continue

        if not is_router and (src_node_type != "host" or dest_node_type != "host"):
            continue

        signal_name = vector_details[2]
        if signal_type not in signal_name:
            continue

        signal_values =  timeseries
        dest_node = vector_details[3]

        cur_info = node_signal_info.get(src_node, dict())
        cur_info[dest_node] = signal_values
        node_signal_info[src_node] = cur_info
        
    return node_signal_info

# use the balance information to get amount inlfight on every channel
def aggregate_inflight_info(bal_timeseries):
    node_signal_info = dict()

    for router, channel_info in bal_timeseries.items():
        inflight_info = dict()
        for partner, router_partner_TS in channel_info.items():
            if router < partner:
                inflight_TS = []
                partner_router_TS = bal_timeseries[partner][router]

                for i, (time, forward_bal) in enumerate(router_partner_TS):
                    backward_bal = partner_router_TS[i][1]
                    inflight_TS.append((time, ROUTER_CAPACITY - forward_bal - backward_bal))
                
                inflight_info[partner] = inflight_TS
        node_signal_info[router] = inflight_info
    return node_signal_info


# use the successful and attempted information to get fraction of successful txns in every interval
def aggregate_frac_successful_info(success, attempted):
    node_signal_info = dict()

    for router, channel_info in success.items():
        frac_successful = dict()
        for partner, success_TS in channel_info.items():
            frac_succ_TS = []
            attempt_TS = attempted[router][partner]

            for i, (time, num_succeeded) in enumerate(success_TS):
                num_attempted = attempt_TS[i][1]
                if num_attempted == 0:
                    frac_succ_TS.append((time, 0))
                else:
                    frac_succ_TS.append((time, num_succeeded/float(num_attempted)))
                frac_successful[partner] = frac_succ_TS
        node_signal_info[router] = frac_successful
    
    return node_signal_info



# plots every router's signal_type info in a new pdf page
# and add a router wealth plot separately
def plot_relevant_stats(data, pdf, signal_type, compute_router_wealth=False):
    color_opts = ['#fa9e9e', '#a4e0f9', '#57a882', '#ad62aa']
    router_wealth_info =[]
    
    for router, channel_info in data.items():
        channel_bal_timeseries = []
        plt.figure()
        plt.rc('axes', prop_cycle = (cycler('color', ['r', 'g', 'b', 'y', 'c', 'm', 'y', 'k']) +
            cycler('linestyle', ['-', '--', ':', '-.', '-', '--', ':', '-.'])))
        
        i = 0
        for channel, info in channel_info.items():
            time = [t[0] for t in info]
            values = [t[1] for t in info]
            label_name = str(router) + "->" + str(channel)
            plt.plot(time, values, label=label_name)
            if compute_router_wealth:
                channel_bal_timeseries.append((time, values))
            i += 1

        if compute_router_wealth:
            router_wealth = []
            for i, time in enumerate(channel_bal_timeseries[0][0]):
                wealth = 0
                for j in channel_bal_timeseries:
                    wealth += j[1][i]
                router_wealth.append(wealth)
            router_wealth_info.append((router,channel_bal_timeseries[0][0], router_wealth))
        
        plt.title(signal_type + " for Router " + str(router))
        plt.xlabel("Time")
        plt.ylabel(signal_type)
        plt.legend()
        pdf.savefig()  # saves the current figure into a pdf page
        plt.close()

    if compute_router_wealth:
        for (r, time, wealth) in router_wealth_info:
            plt.plot(time, wealth, label=str(r))
        plt.title("Router Wealth Timeseries")
        plt.xlabel("Time")
        plt.ylabel("Router Wealth")
        plt.legend()
        pdf.savefig()  # saves the current figure into a pdf page
        plt.close()



# plot per router channel information on a per router basis depending on the type of signal wanted
def plot_per_payment_channel_stats(args):
    color_opts = ['#fa9e9e', '#a4e0f9', '#57a882', '#ad62aa']
    dims = plt.rcParams["figure.figsize"]
    plt.rcParams["figure.figsize"] = dims
    data_to_plot = dict()

    with PdfPages(args.save + "_per_channel_info.pdf") as pdf:
        if args.balance: 
            data_to_plot = aggregate_info_per_node(args.vec_file, "balance", True)
            plot_relevant_stats(data_to_plot, pdf, "Balance", compute_router_wealth=True)

            if args.inflight:
                inflight_data_to_plot = aggregate_inflight_info(data_to_plot)
                plot_relevant_stats(inflight_data_to_plot, pdf, "Inflight Funds")

            
        if args.queue_info:
            data_to_plot = aggregate_info_per_node(args.vec_file, "numInQueue", True)
            plot_relevant_stats(data_to_plot, pdf, "Number in Queue")

        if args.num_sent_per_channel:
            data_to_plot = aggregate_info_per_node(args.vec_file, "numSent", True)
            plot_relevant_stats(data_to_plot, pdf, "Number Sent")
    print "http://" + EC2_INSTANCE_ADDRESS + ":" + str(PORT_NUMBER) + "/" + args.save + "_per_channel_info.pdf"
    


# plot per router channel information on a per router basis depending on the type of signal wanted
def plot_per_src_dest_stats(args):
    color_opts = ['#fa9e9e', '#a4e0f9', '#57a882', '#ad62aa']
    dims = plt.rcParams["figure.figsize"]
    plt.rcParams["figure.figsize"] = dims
    data_to_plot = dict()

    with PdfPages(args.save + "_per_src_dest_stats.pdf") as pdf:
        if args.timeouts: 
            data_to_plot = aggregate_info_per_node(args.vec_file, "numTimedOut", False)
            plot_relevant_stats(data_to_plot, pdf, "Number of Transactions Timed Out")
        
        if args.frac_completed: 
            successful = aggregate_info_per_node(args.vec_file, "rateCompleted", False)
            attempted = aggregate_info_per_node(args.vec_file, "rateAttempted", False)
            data_to_plot = aggregate_frac_successful_info(successful, attempted)
            plot_relevant_stats(data_to_plot, pdf, "Fraction of successful txns in each window")
 
    print "http://" + EC2_INSTANCE_ADDRESS + ":" + str(PORT_NUMBER) + "/" + \
            args.save + "_per_src_dest_stats.pdf"



def main():
    '''matplotlib.rcParams['figure.figsize'] = [15, 10]
    plt.rc('font', size=40)          # controls default text sizes
    plt.rc('axes', titlesize=42)     # fontsize of the axes title
    plt.rc('axes', labelsize=40)    # fontsize of the x and y labels
    plt.rc('xtick', labelsize=32)    # fontsize of the tick labels
    plt.rc('ytick', labelsize=32)    # fontsize of the tick labels
    plt.rc('legend', fontsize=34)    # legend fontsize'''
    plot_per_payment_channel_stats(args)
    plot_per_src_dest_stats(args)
main()
