import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import argparse
import os
import json
import numpy as np
from parse_vec_files import *
from parse_sca_files import *
from matplotlib.backends.backend_pdf import PdfPages
from cycler import cycler
from config import *
from itertools import cycle

parser = argparse.ArgumentParser('Analysis Plots')
parser.add_argument('--vec_file',
        type=str,
        required=True,
        help='Single vector file for a particular run using the omnet simulator')
parser.add_argument('--sca_file',
        type=str,
        required=True,
        help='Single scalar file for a particular run using the omnet simulator')
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
parser.add_argument('--timeouts_sender',
        action='store_true',
        help='Plot number of timeouts at the sender for all source destination pairs')
parser.add_argument('--frac_completed',
        action='store_true',
        help='Plot fraction completed for all source destination pairs')
parser.add_argument('--frac_completed_window',
        action='store_true',
        help='Plot fraction completed for all source destination pairs in every window')
parser.add_argument('--path',
        action='store_true',
        help='Plot path index used for all source destination pairs')
parser.add_argument('--waiting',
        action='store_true',
        help='Plot number of waiting txns at a the sender at a point in time all source destination pairs')
parser.add_argument('--probabilities',
        action='store_true',
        help='Plot probabilities of picking each path at a given point in time all source destination pairs')
parser.add_argument('--bottlenecks',
        action='store_true',
        help='Plot bottlenecks on different paths at a given point in time all source destination pairs')
parser.add_argument('--lambda_val',
        action='store_true',
        help='Plot the per channel capacity related price when price based scheme is used')
parser.add_argument('--mu_local',
        action='store_true',
        help='Plot the per imbalance related price when price based scheme is used')
parser.add_argument('--mu_remote',
        action='store_true',
        help='Plot the imbalance related price at the remote end')
parser.add_argument('--x_local',
        action='store_true',
        help='Plot the per channel rate of sending related price when price based scheme is used')
parser.add_argument('--n_local',
        action='store_true',
        help='Plot the per channel number of txns when price based scheme is used')
parser.add_argument('--service_arrival_ratio',
        action='store_true',
        help='Plot the per channel  service rate when price based scheme is used')
parser.add_argument('--inflight_outgoing',
        action='store_true',
        help='Plot the per channel number of outgoing txns price when price based scheme is used')
parser.add_argument('--inflight_incoming',
        action='store_true',
        help='Plot the per channel number of incoming txns price when price based scheme is used')
parser.add_argument('--rate_to_send',
        action='store_true',
        help='Plot the per path rate to send when price based scheme is used')
parser.add_argument('--rate_sent',
        action='store_true',
        help='Plot the per path rate actually sent when price based scheme is used')
parser.add_argument('--amt_inflight_per_path',
        action='store_true',
        help='Plot the per path amt inflight when price based scheme is used')
parser.add_argument('--price',
        action='store_true',
        help='Plot the per channel price to send when price based scheme is used')
parser.add_argument('--demand',
        action='store_true',
        help='Plot the per dest estimated demand when price based scheme is used')


parser.add_argument('--save',
        type=str,
        required=True,
        help='The pdf file prefix to which to write the figures')

args = parser.parse_args()

#fmts = ['r--', 'b-', 'g-.']

# returns a dictionary of the necessary stats where key is a router node
# and value is another dictionary where the key is the partner node 
# and value is the time series of the signal_type recorded for this pair of nodes
def aggregate_info_per_node(all_timeseries, vec_id_to_info_map, signal_type, is_router, aggregate_per_path=False, is_both=False):
    node_signal_info = dict()
   #all_timeseries, vec_id_to_info_map = parse_vec_file(filename, signal_type)

    # aggregate information on a per router node basis
    # and then on a per channel basis for that router node
    for vec_id, timeseries in all_timeseries.items():
        vector_details = vec_id_to_info_map[vec_id]
        src_node = vector_details[0]
        src_node_type = vector_details[1]
        dest_node_type = vector_details[4]
        dest_node = vector_details[3]
        
        '''if signal_type is "balance":
            if (src_node_type == "router" and dest_node_type == "host") or \
                    (src_node_type == "host" and dest_node_type == "router"):
                        for t in timeseries:
                            if t[1] == "0":
                                print "End host " + str(src_node) + " hitting zero at time " + str(t[0])'''

            
        '''if is_both:
            if src_node_type == "host":
                src_node = 2 + src_node
            elif dest_node_type == "host":
                dest_node = 2 + dest_node'''
        #else:
        if is_router and (src_node_type != "router" or dest_node_type != "router"):
            continue

        if not is_router and (src_node_type != "host" or dest_node_type != "host"):
            continue
            '''elif not is_router:
                src_node = src_node + 2
                dest_node = dest_node + 2'''

        signal_name = vector_details[2]
        if signal_type not in signal_name:
            continue

        signal_values =  timeseries

        if aggregate_per_path:
            path_id = int(signal_name.split("_")[1])
            cur_info = node_signal_info.get(src_node, dict())
            dest_info = cur_info.get(dest_node, dict())
            dest_info[path_id] = signal_values
            cur_info[dest_node] = dest_info
            node_signal_info[src_node] = cur_info
        else: 
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
def plot_relevant_stats(data, pdf, signal_type, compute_router_wealth=False, per_path_info=False, window_info=None):
    color_opts = ['#fa9e9e', '#a4e0f9', '#57a882', '#ad62aa']
    router_wealth_info =[]
    
    for router, channel_info in data.items():
        channel_bal_timeseries = []
        plt.figure()
        plt.rc('axes', prop_cycle = (cycler('color', ['r', 'g', 'b', 'y', 'c', 'm', 'y', 'k']) +
            cycler('linestyle', ['-']*8)))
                #, '--', ':', '-.', '-', '--', ':', '-.'])))
        
        
        i = 0
        for channel, info in channel_info.items():
            # plot one plot per src dest pair and multiple lines per path
            if per_path_info:
                color_cycle_for_path = cycle(['r', 'g', 'b', 'y', 'c', 'm', 'y', 'k'])
                for path, path_signals in info.items():
                    time = [t[0] for t in path_signals]
                    values = [t[1] for t in path_signals]
                    
                    c = next(color_cycle_for_path)

                    if window_info is not None:
                        window_signals = window_info[router][channel][path]
                        window_val = [t[1] for t in window_signals]
                        label_name = str(path) + "_Window"
                        plt.plot(time, window_val, label=label_name, linestyle="--", color=c)
                    
                    label_name = str(path)
                    start = int(len(values)/4)
                    plt.plot(time, values, \
                            label=label_name + "(" + str(np.average(values[start:])) + ")", color=c)
                plt.title(signal_type + " " + str(router) + "->" + str(channel))
                plt.xlabel("Time")
                plt.ylabel(signal_type)
                plt.legend()
                plt.grid()
                pdf.savefig()  # saves the current figure into a pdf page
                plt.close()

            # one plot per src with multiple lines per dest
            else:
                time = [t[0] for t in info]
                values = [t[1] for t in info]
                label_name = str(router) + "->" + str(channel)

                start = int(len(values)/4)
                plt.plot(time, values, label=label_name + \
                        "(" + str(np.average(values[start:])) + ")")
                if compute_router_wealth:
                    channel_bal_timeseries.append((time, values))
                i += 1

        # aggregate info for all router's wealth
        if compute_router_wealth:
            router_wealth = []
            for i, time in enumerate(channel_bal_timeseries[0][0]):
                wealth = 0
                for j in channel_bal_timeseries:
                    wealth += j[1][i]
                router_wealth.append(wealth)
            router_wealth_info.append((router,channel_bal_timeseries[0][0], router_wealth))
        
        # close plot after every router unless it is per path information
        if not per_path_info:
            plt.title(signal_type + " for Router " + str(router))
            plt.xlabel("Time")
            plt.ylabel(signal_type)
            plt.legend()
            plt.grid()
            pdf.savefig()  # saves the current figure into a pdf page
            plt.close()

    # one giant plot for all router's wealth
    if compute_router_wealth:
        for (r, time, wealth) in router_wealth_info:
            plt.plot(time, wealth, label=str(r))
        plt.title("Router Wealth Timeseries")
        plt.xlabel("Time")
        plt.ylabel("Router Wealth")
        plt.legend()
        plt.grid()
        pdf.savefig()  # saves the current figure into a pdf page
        plt.close()


# see if infligh plus balance was ever more than capacity
def find_problem(balance_timeseries, inflight_timeseries) :
    for router, channel_info in balance_timeseries.items():
            for channel, info in channel_info.items():
                for i, (time, value) in enumerate(info):
                    my_balance = value
                    try:
                        remote_balance = balance_timeseries[channel][router][i][1]
                        inflight = inflight_timeseries[channel][router][i][1]
                        # num inflight might be a little inconsistent depending on clear state
                        # but others should tally up
                        if my_balance + remote_balance > ROUTER_CAPACITY:
                            print " problem at router", router, "with ", channel,   " at time " , time
                            print my_balance, remote_balance, inflight
                    except:
                        print channel, router, i
        


# plot per router channel information on a per router basis depending on the type of signal wanted
def plot_per_payment_channel_stats(args, text_to_add):
    color_opts = ['#fa9e9e', '#a4e0f9', '#57a882', '#ad62aa']
    dims = plt.rcParams["figure.figsize"]
    plt.rcParams["figure.figsize"] = dims
    data_to_plot = dict()

    with PdfPages(args.save + "_per_channel_info.pdf") as pdf:
        all_timeseries, vec_id_to_info_map, parameters = parse_vec_file(args.vec_file, "per_channel_plot")
        firstPage = plt.figure()
        firstPage.clf()
        txt = 'Parameters:\n' + parameters
        txt += str(text_to_add[0]) + "\n"
        txt += "Completion over arrival " + str(text_to_add[1]) + "\n"
        txt += "Completion over attempted " + str(text_to_add[2]) + "\n"

        firstPage.text(0.5, 0, txt, transform=firstPage.transFigure, ha="center")
        pdf.savefig()
        plt.close()

        if args.balance: 
            data_to_plot = aggregate_info_per_node(all_timeseries, vec_id_to_info_map, "balance", True)
            plot_relevant_stats(data_to_plot, pdf, "Balance", compute_router_wealth=True)

            inflight = aggregate_info_per_node(all_timeseries, vec_id_to_info_map, "numInflight", True)
            plot_relevant_stats(inflight, pdf, "Inflight funds")
            find_problem(data_to_plot, inflight)

            '''if args.inflight:
                inflight_data_to_plot = aggregate_inflight_info(data_to_plot)
                plot_relevant_stats(inflight_data_to_plot, pdf, "Inflight Funds")'''

            
        if args.queue_info:
            data_to_plot = aggregate_info_per_node(all_timeseries, vec_id_to_info_map, "numInQueue", True)
            plot_relevant_stats(data_to_plot, pdf, "Number in Queue")

        if args.num_sent_per_channel:
            data_to_plot = aggregate_info_per_node(all_timeseries, vec_id_to_info_map, "numSent", True)
            plot_relevant_stats(data_to_plot, pdf, "Number Sent")

        if args.lambda_val:
            data_to_plot = aggregate_info_per_node(all_timeseries, vec_id_to_info_map, "lambda", True, is_both=False)
            plot_relevant_stats(data_to_plot, pdf, "Lambda")
        
        if args.mu_local:
            data_to_plot = aggregate_info_per_node(all_timeseries, vec_id_to_info_map, "muLocal", True, is_both=False)
            plot_relevant_stats(data_to_plot, pdf, "Mu Local")
        
        if args.mu_remote:
            data_to_plot = aggregate_info_per_node(all_timeseries, vec_id_to_info_map, "muRemote", True, is_both=False)
            plot_relevant_stats(data_to_plot, pdf, "Mu Remote")
        
        if args.x_local:
            data_to_plot = aggregate_info_per_node(all_timeseries, vec_id_to_info_map, "xLocal", True, is_both=False)
            plot_relevant_stats(data_to_plot, pdf, "xLocal")
        
        if args.n_local:
            data_to_plot = aggregate_info_per_node(all_timeseries, vec_id_to_info_map, "nValue", True, is_both=False)
            plot_relevant_stats(data_to_plot, pdf, "nValue")

        if args.service_arrival_ratio:
            data_to_plot = aggregate_info_per_node(all_timeseries, vec_id_to_info_map, "serviceRate", True, is_both=False)
            plot_relevant_stats(data_to_plot, pdf, "service arrival ratio")


        if args.inflight_outgoing:
            data_to_plot = aggregate_info_per_node(all_timeseries, vec_id_to_info_map, "inflightOutgoing", True, is_both=False)
            plot_relevant_stats(data_to_plot, pdf, "inflight outgoing on channel")


        if args.inflight_incoming:
            data_to_plot = aggregate_info_per_node(all_timeseries, vec_id_to_info_map, "inflightIncoming", True, is_both=False)
            plot_relevant_stats(data_to_plot, pdf, "inflight incoming on channel")

    print "http://" + EC2_INSTANCE_ADDRESS + ":" + str(PORT_NUMBER) + "/scripts/figures/timeouts/" + \
            os.path.basename(args.save) + "_per_channel_info.pdf"
    


# plot per router channel information on a per router basis depending on the type of signal wanted
def plot_per_src_dest_stats(args, text_to_add):
    color_opts = ['#fa9e9e', '#a4e0f9', '#57a882', '#ad62aa']
    dims = plt.rcParams["figure.figsize"]
    plt.rcParams["figure.figsize"] = dims
    data_to_plot = dict()

    with PdfPages(args.save + "_per_src_dest_stats.pdf") as pdf:
        all_timeseries, vec_id_to_info_map, parameters = parse_vec_file(args.vec_file, "per_src_dest_plot")
 
        firstPage = plt.figure()
        firstPage.clf()
        txt = 'Parameters:\n' + parameters
        txt += text_to_add[0] + "\n"
        txt += "Completion over arrival " + str(text_to_add[1]) + "\n"
        txt += "Completion over attempted " + str(text_to_add[2]) + "\n"

        firstPage.text(0.5, 0, txt, transform=firstPage.transFigure, ha="center")
        pdf.savefig()
        plt.close()
        
        if args.timeouts: 
            data_to_plot = aggregate_info_per_node(all_timeseries, vec_id_to_info_map,  "numTimedOutPerDest", False)
            plot_relevant_stats(data_to_plot, pdf, "Number of Transactions Timed Out")
        
        if args.frac_completed_window: 
            successful = aggregate_info_per_node(all_timeseries, vec_id_to_info_map, "rateCompleted", False)
            attempted = aggregate_info_per_node(all_timeseries, vec_id_to_info_map, "rateArrived", False)
            data_to_plot = aggregate_frac_successful_info(successful, attempted)
            plot_relevant_stats(data_to_plot, pdf, "Fraction of successful txns in each window")

        if args.frac_completed:
            data_to_plot = aggregate_info_per_node(all_timeseries, vec_id_to_info_map, "fracSuccessful", False)
            plot_relevant_stats(data_to_plot, pdf, "Fraction of successful txns")

        if args.path:
            data_to_plot = aggregate_info_per_node(all_timeseries, vec_id_to_info_map, "pathPerTrans", False)
            plot_relevant_stats(data_to_plot, pdf, "Path Per Transaction")

        if args.timeouts_sender:
            data_to_plot = aggregate_info_per_node(all_timeseries, vec_id_to_info_map, "numTimedOutAtSender", False)
            plot_relevant_stats(data_to_plot, pdf, "Number of Transactions Timed Out At Sender")

        if args.waiting:
            data_to_plot = aggregate_info_per_node(all_timeseries, vec_id_to_info_map, "numWaiting", False)
            plot_relevant_stats(data_to_plot, pdf, "Number of Transactions Waiting at sender To Given Destination")

        if args.probabilities:
            data_to_plot = aggregate_info_per_node(all_timeseries, vec_id_to_info_map, "probability", False, True)
            plot_relevant_stats(data_to_plot, pdf, "Probability of picking paths", per_path_info=True)

        if args.bottlenecks:
            data_to_plot = aggregate_info_per_node(all_timeseries, vec_id_to_info_map, "bottleneck", False, True)
            plot_relevant_stats(data_to_plot, pdf, "Bottleneck Balance", per_path_info=True)

        if args.rate_to_send:
            data_to_plot = aggregate_info_per_node(all_timeseries, vec_id_to_info_map, "rateToSendTrans", False, True)
            plot_relevant_stats(data_to_plot, pdf, "Rate to send per path", per_path_info=True)

        if args.rate_sent:
            data_to_plot = aggregate_info_per_node(all_timeseries, vec_id_to_info_map, "rateSent", False, True)
            plot_relevant_stats(data_to_plot, pdf, "Rate actually sent per path", per_path_info=True)
        
        if args.amt_inflight_per_path:
            data_to_plot = aggregate_info_per_node(all_timeseries, vec_id_to_info_map, \
                    "sumOfTransUnitsInFlight", False, True)
            window_info = aggregate_info_per_node(all_timeseries, vec_id_to_info_map, \
                    "window", False, True)
            plot_relevant_stats(data_to_plot, pdf, "Amount Inflight/Window per path", per_path_info=True, \
                    window_info=window_info)
        
        if args.price:
            data_to_plot = aggregate_info_per_node(all_timeseries, vec_id_to_info_map, "priceLastSeen", False, True)
            plot_relevant_stats(data_to_plot, pdf, "Price per path", per_path_info=True)

        if args.demand:
            data_to_plot = aggregate_info_per_node(all_timeseries, vec_id_to_info_map, "demandEstimate", False)
            plot_relevant_stats(data_to_plot, pdf, "Demand Estimate per Path")

 
    print "http://" + EC2_INSTANCE_ADDRESS + ":" + str(PORT_NUMBER) + "/scripts/figures/timeouts/" + \
            os.path.basename(args.save) + "_per_src_dest_stats.pdf"



def main():
    '''matplotlib.rcParams['figure.figsize'] = [15, 10]
    plt.rc('font', size=40)          # controls default text sizes
    plt.rc('axes', titlesize=42)     # fontsize of the axes title
    plt.rc('axes', labelsize=40)    # fontsize of the x and y labels
    plt.rc('xtick', labelsize=32)    # fontsize of the tick labels
    plt.rc('ytick', labelsize=32)    # fontsize of the tick labels
    plt.rc('legend', fontsize=34)    # legend fontsize'''
    text_to_add = parse_sca_files(args.sca_file)
    plot_per_payment_channel_stats(args, text_to_add)
    plot_per_src_dest_stats(args, text_to_add)
main()
