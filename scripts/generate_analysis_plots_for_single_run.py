import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import argparse
import os
import json
import numpy as np
from parse_vec_files import *
from matplotlib.backends.backend_pdf import PdfPages

parser = argparse.ArgumentParser('Analysis Plots')
parser.add_argument('--vec_file',
        type=str,
        required=True,
        help='Single vector file for a particular run using the omnet simulator')
parser.add_argument('--balance',
        action='store_true',
        help='Plot balance information for all routers')
parser.add_argument('--num_sent_per_channel',
        action='store_true',
        help='Plot number sent per channel for all routers')
parser.add_argument('--queue_info',
        action='store_true',
        help='Plot queue information for all routers')

parser.add_argument('--save',
        type=str,
        required=True,
        help='The pdf file prefix to which to write the figures')

args = parser.parse_args()

fmts = ['r--', 'b-', 'g-.']

'''
per src dest
number of timeouts
sumber attempted - separate plot per sender
number completed'''

# returns a dictionary of the necessary stats where key is a router node
# and value is another dictionary where the key is the partner node 
# and value is the time series of the signal_type recorded for this pair of nodes
def aggregate_info_on_all_channels(filename, signal_type):
    router_channel_signal_info = dict()
    all_timeseries, vec_id_to_info_map = parse_vec_file(filename, signal_type)

    # aggregate information on a per router node basis
    # and then on a per channel basis for that router node
    for vec_id, timeseries in all_timeseries.items():
        vector_details = vec_id_to_info_map[vec_id]
        src_node = vector_details[0]
        src_node_type = vector_details[1]
        if src_node_type != "router":
            continue

        signal_name = vector_details[2]
        if signal_type not in signal_name:
            continue

        signal_values =  timeseries
        dest_node = vector_details[3]

        cur_info = router_channel_signal_info.get(src_node, dict())
        cur_info[dest_node] = signal_values
        router_channel_signal_info[src_node] = cur_info
        
    return router_channel_signal_info

# plots every router's signal_type info in a new pdf page
def plot_relevant_stats(data, pdf, signal_type):
    color_opts = ['#fa9e9e', '#a4e0f9', '#57a882', '#ad62aa']
    
    for router, channel_info in data.items():
        plt.figure(figsize=(3, 3))
        i = 0
        for channel, info in channel_info.items():
            time = [t[0] for t in info]
            values = [t[1] for t in info]
            label_name = str(router) + "->" + str(channel)
            plt.plot(time, values, fmts[i%len(fmts)], label=label_name)
            i += 1
        
        plt.title(signal_type + " for Router " + str(router))
        plt.xlabel("Time")
        plt.ylabel(signal_type)
        pdf.savefig()  # saves the current figure into a pdf page
        plt.close()


# plot per channel information on a per router basis depending on the type of signal wanted
def plot_per_payment_channel_stats(args):
    color_opts = ['#fa9e9e', '#a4e0f9', '#57a882', '#ad62aa']
    dims = plt.rcParams["figure.figsize"]
    plt.rcParams["figure.figsize"] = dims
    data_to_plot = dict()

    with PdfPages(args.save + "per_channel_info.pdf") as pdf:
        if args.balance: 
            data_to_plot = aggregate_info_on_all_channels(args.vec_file, "balance")
            plot_relevant_stats(data_to_plot, pdf, "Balance")
            
        if args.queue_info:
            data_to_plot = aggregate_info_on_all_channels(args.vec_file, "queue_info")
            plot_relevant_stats(data_to_plot, pdf), "Number in Queue"

        if args.num_sent_per_channel:
            data_to_plot = aggregate_info_on_all_channels(args.vec_file, "num_sent_per_channel")
            plot_relevant_stats(data_to_plot, pdf, "Number Sent")


def main():
    '''matplotlib.rcParams['figure.figsize'] = [15, 10]
    plt.rc('font', size=40)          # controls default text sizes
    plt.rc('axes', titlesize=42)     # fontsize of the axes title
    plt.rc('axes', labelsize=40)    # fontsize of the x and y labels
    plt.rc('xtick', labelsize=32)    # fontsize of the tick labels
    plt.rc('ytick', labelsize=32)    # fontsize of the tick labels
    plt.rc('legend', fontsize=34)    # legend fontsize'''
    plot_per_payment_channel_stats(args)

main()
