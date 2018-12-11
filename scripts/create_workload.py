import sys
import textwrap
import argparse
import numpy as np
import networkx as nx
import random

SCALE_AMOUNT = 5

# generates the start and end nodes for a fixed set of topologies - hotnets/line/simple graph
def generate_workload_standard(filename, payment_graph_topo, workload_type, total_time, \
        exp_size, txn_size_mean):
    # define start and end nodes and amounts
    # edge a->b in payment graph appears in index i as start_nodes[i]=a, and end_nodes[i]=b
    if payment_graph_topo == 'hotnets_topo':
        start_nodes = [0,3,0,1,2,3,2,4]
        end_nodes = [4,0,1,3,1,2,4,2]
        amt_relative = [1,2,1,2,1,2,2,1]
        amt_absolute = [SCALE_AMOUNT * x for x in amt_relative]

    elif payment_graph_topo == 'simple_deadlock':
        start_nodes = [1,0,2]
        end_nodes = [2,2,0]
        amt_relative = [2,1,2]
        amt_absolute = [SCALE_AMOUNT * x for x in amt_relative]

    elif payment_graph_topo == 'simple_line':
        start_nodes = [0,3]
        end_nodes = [3,0]
        amt_relative = [1,1]
        amt_absolute = [SCALE_AMOUNT * x for x in amt_relative]

    write_txns_to_file(filename, start_nodes, end_nodes, amt_absolute,\
            workload_type, total_time, exp_size, txn_size_mean)


# write the given set of txns denotes by start_node -> end_node with absolute_amts as passed in
# to a separate workload file
# workload file of form
# [amount] [timeSent] [sender] [receiver] [priorityClass]
# write to file - assume no priority for now
# transaction sizes are either constant or exponentially distributed around their mean
def write_txns_to_file(filename, start_nodes, end_nodes, amt_absolute,\
        workload_type, total_time, exp_size, txn_size_mean):
    outfile = open(filename, "w")

    if distribution == 'uniform':
        # constant transaction size generated at uniform intervals
        for i in range(total_time):
            for k in range(len(start_nodes)):
                for l in range(amt_absolute[k]):
                    rate = amt_absolute[k]
                    time_start = i*1.0 + (l*1.0) / (1.0*rate)
                    txn_size = np.random_exponential(txn_size_mean) if exp_size else txn_size_mean
                    outfile.write(str(txn_size) + " " + str(time_start) + " " + str(start_nodes[k]) + \
                            " " + str(end_nodes[k]) + " 0\n")

    elif distribution == 'poisson':
        # constant transaction size to be sent in a poisson fashion
        for i in range(total_time):
            for k in range(len(start_nodes)):
                current_time = 0.0
                rate = amt_absolute[k]*1.0
                beta = (1.0) / (1.0 * rate)
                # if the rate is higher, given pair will have more transactions in a single second
                while (current_time < 1.0):
                    time_start = i*1.0 + current_time
                    txn_size = np.random_exponential(txn_size_mean) if exp_size else txn_size_mean
                    outfile.write(str(txn_size) + " " + str(time_start) + " " + str(start_nodes[k]) + \
                            " " + str(end_nodes[k]) + " 0\n")
                    time_incr = np.random.exponential(beta)
                    current_time = current_time + time_incr 

    outfile.close()



# generate workload for arbitrary topology
def generate_workload_custom(filename, graph, workload_type, total_time, \
        exp_size, txn_size_mean):
    n = graph.number_of_nodes()
    num_sender_receiver_pairs = n/4

    start_nodes, end_nodes, amt_relative = [], [], []
    for i in range(num_sender_receiver_pairs):
        sender_receiver_pair = random.sample(xrange(0, n - 1, 1), 2)
        start_nodes.append(sender_receiver_pair[0])
        end_nodes.append(sender_receiver_pair[1])
        amt_relative.append(random.choice(range(1, 10, 1)))

    amt_absolute = [SCALE_AMOUNT * x for x in amt_relative]

    write_txns_to_file(filename, start_nodes, end_nodes, amt_absolute,\
            workload_type, total_time, exp_size, txn_size_mean)



# parse topology file to get graph structure
def parse_topo(topo_filename):
    g = nx.Graph()
    with open(topo_filename) as topo_file:
        for line in topo_file:
            n1 = int(line.split()[0])
            n2 = int(line.split()[1])
            g.add_edge(n1, n2)
    return g




# parse arguments
parser = argparse.ArgumentParser(description="Create arbitrary txn workloads to run the omnet simulator on")
parser.add_argument('--payment-graph-type', \
        choices=['hotnets_topo', 'simple_line', 'simple_deadlock', 'custom'],\
        help='type of graph (Small world or scale free or custom topology)', default='simple_line')
parser.add_argument('--topo-filename', dest='topo_filename', type=str, \
        help='name of topology file to generate worklooad for')
parser.add_argument('output_filename', type=str, help='name of the output workload file', \
        default='simple_workload.txt')
parser.add_argument('distribution', choices=['uniform', 'poisson'],\
        help='time between transactions is determine by this', default='poisson')
parser.add_argument('--experiment-time', dest='total_time', type=int, \
        help='time to generate txns for', default=30)
parser.add_argument('--txn-size-mean', dest='txn_size_mean', type=int, \
        help='mean_txn_size', default=1)
parser.add_argument('--exp_size', action='store_true', help='should txns be exponential in size')

args = parser.parse_args()

output_filename = args.output_filename
payment_graph_type = args.payment_graph_type
distribution = args.distribution
total_time = args.total_time
txn_size_mean = args.txn_size_mean
exp_size = args.exp_size
topo_filename = args.topo_filename



# generate workloads
if payment_graph_type != 'custom':
    generate_workload_standard(output_filename, payment_graph_type, distribution, \
            total_time, exp_size, txn_size_mean)
elif topo_filename is None:
    raise Exception("Topology needed for custom file")
else:
    graph = parse_topo(topo_filename)
    generate_workload_custom(output_filename, graph, distribution, total_time, exp_size,\
            txn_size_mean)







    
