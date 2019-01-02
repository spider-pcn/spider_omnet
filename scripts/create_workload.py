import sys
import textwrap
import argparse
import numpy as np
import networkx as nx
import random
import json
from config import *


# generates the start and end nodes for a fixed set of topologies - hotnets/line/simple graph
def generate_workload_standard(filename, payment_graph_topo, workload_type, total_time, \
        exp_size, txn_size_mean, generate_json_also, is_circulation):
    # by default ASSUMES NO END HOSTS

    # define start and end nodes and amounts
    # edge a->b in payment graph appears in index i as start_nodes[i]=a, and end_nodes[i]=b
    if payment_graph_topo == 'hotnets_topo':
        start_nodes = [0,3,0,1,2,3,2,4]
        end_nodes = [4,0,1,3,1,2,4,2]
        amt_relative = [1,2,1,2,1,2,2,1]
        amt_absolute = [SCALE_AMOUNT * x for x in amt_relative]
        graph = hotnets_topo_graph

    elif payment_graph_topo == 'simple_deadlock':
        start_nodes = [1,0,2]
        end_nodes = [2,2,0]
        amt_relative = [2,1,2]
        amt_absolute = [SCALE_AMOUNT * x for x in amt_relative]
        graph = simple_deadlock_graph

    elif payment_graph_topo == 'simple_line':
        start_nodes = [0,3]
        end_nodes = [3,0]
        amt_relative = [1,1]
        amt_absolute = [SCALE_AMOUNT * x for x in amt_relative]
        graph = simple_line_graph

    # generate circulation instead if you need a circulation
    if is_circulation:
        start_nodes, end_nodes, amt_relative = [], [], []
        num_nodes = graph.number_of_nodes()
    	""" generate circution demand """
    	demand_dict = circ_demand(num_nodes, mean=MEAN_RATE, std_dev=CIRCULATION_STD_DEV)

    	for i, j in demand_dict.keys():
            start_nodes.append(i)
    	    end_nodes.append(j)
    	    amt_relative.append(demand_dict[i, j])
        amt_absolute = [SCALE_AMOUNT * x for x in amt_relative]


    if generate_json_also:
        generate_json_files(filename + '.json', graph, graph, start_nodes, end_nodes, amt_absolute)

    write_txns_to_file(filename + '_workload.txt', start_nodes, end_nodes, amt_absolute,\
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




# generates the json file necessary for the distributed testbed to be used to test
# the lnd implementation
def generate_json_files(filename, graph, inside_graph, start_nodes, end_nodes, amt_absolute):
    json_string = {}

    # create btcd connections
    # routers connected to each other and end hosts connected to respective router
    btcd_connections = []
    for i in range(graph.number_of_nodes() - 1):
        connection = {"src": str(i) + "r", "dst" : str(i + 1) + "r"}
        btcd_connections.append(connection)
        connection = {"src": str(i) + "e", "dst" : str(i) + "r"}
        btcd_connections.append(connection)
    connection = {"src": str(graph.number_of_nodes() - 1) + "e", "dst" : str(graph.number_of_nodes() - 1) + "r"}
    btcd_connections.append(connection)
    json_string["btcd_connections"] = btcd_connections

    # miner node
    json_string["miner"] = "0r"


    # create nodesi for end hosts and router nodes and assign them distinct ips
    nodes = []
    for n in graph.nodes():
        node = {"name": str(n) + "r", "ip" : "10.0.1." + str(100 + n)}
        nodes.append(node)
        node = {"name": str(n) + "e", "ip" : "10.0.2." + str(100 + n)}
        nodes.append(node)
    json_string["nodes"] = nodes

    # creates all the lnd channels
    edges = []
    for (u,v) in graph.edges():
        if  u == v:
            cap = ENDHOST_LND_ONE_WAY_CAPACITY
            node_type = "e"
        else:
            cap = balance/2
            node_type = "r"

        if u <= v: 
            edge = {"src": str(u) + "r", "dst": str(v) + node_type, "capacity" : cap}
            edges.append(edge)

    json_string["lnd_channels"] = edges

    # creates the string for the demands
    demands = []
    for s, e, a in zip(start_nodes, end_nodes, amt_absolute):
        demand_entry = {"src": str(s) + "e", "dst": str(e) + "e",\
                        "rate": a/SCALE_AMOUNT}
        demands.append(demand_entry)

    json_string["demands"] = demands

    with open(filename, 'w') as outfile:
            json.dump(json_string, outfile, indent=8)




# generate workload for arbitrary topology by uniformly sampling
# the set of nodes for sender-receiver pairs
# size of transaction is determined when writing to the file to
# either be exponentially distributed or constant size
def generate_workload_for_provided_topology(filename, inside_graph, whole_graph, end_host_map, \
        workload_type, total_time, \
        exp_size, txn_size_mean, generate_json_also, is_circulation):
    num_nodes = inside_graph.number_of_nodes()
    start_nodes, end_nodes, amt_relative = [], [], []
    
    if is_circulation:
    	""" generate circution demand """
    	demand_dict = circ_demand(inside_graph.nodes, mean=MEAN_RATE, std_dev=CIRCULATION_STD_DEV)

    	for i, j in demand_dict.keys():
    	    start_nodes.append(end_host_map[i])
    	    end_nodes.append(end_host_map[j])
    	    amt_relative.append(demand_dict[i, j])	

    else:
        num_sender_receiver_pairs = num_nodes
        for i in range(num_sender_receiver_pairs):
            sender_receiver_pair = random.sample(inside_graph.nodes, 2)
            start_nodes.append(end_host_map[sender_receiver_pair[0]])
            end_nodes.append(end_host_map[sender_receiver_pair[1]])
            amt_relative.append(random.choice(range(1, MEAN_RATE * 2, 1)))

    amt_absolute = [SCALE_AMOUNT * x for x in amt_relative]

    print "generated workload" 

    if generate_json_also:
        generate_json_files(filename + '.json', whole_graph, inside_graph, start_nodes, end_nodes, amt_absolute)

    write_txns_to_file(filename + '_workload.txt', start_nodes, end_nodes, amt_absolute,\
            workload_type, total_time, exp_size, txn_size_mean)


# parse a given line of edge relationships from the topology file
# and return whether this is a router node and its identifier
def parse_node(node_name):
    try:
        val = int(node_name[:-1])
        if node_name[-1] == 'r':
            return True, val
        if node_name[-1] == 'e':
            return False, val
        return -1
    except:
        return -1

# parse topology file to get graph structure
def parse_topo(topo_filename):
    g = nx.Graph()
    router_graph = nx.Graph()
    end_host_map = dict()

    with open(topo_filename) as topo_file:
        for line in topo_file:
            if line == '\n':
                continue
            n1 = parse_node(line.split()[0])
            n2 = parse_node(line.split()[1])
            if n1 == -1 or n2 == -1:
                print "Bad line " + line
                continue

            g.add_edge(n1[1], n2[1])

            if n1[0] and n2[0]: 
                router_graph.add_edge(n1[1], n2[1])
            elif n1[0]:
                end_host_map[n1[1]] = n2[1]
            elif n2[0]:
                end_host_map[n2[1]] = n1[1]
    
    return g, router_graph, end_host_map



# generate circulation demand for node ids mentioned in node_list,
# with average total demand at a node equal to 'mean', and a 
# perturbation of 'std_dev' 
def circ_demand(node_list, mean, std_dev):

	assert type(mean) is int
	assert type(std_dev) is int

	demand_dict = {}
        
        
        node_list = list(node_list)
        num_nodes = len(node_list)

	""" sum of 'mean' number of random permutation matrices """
	""" note any permutation matrix is a circulation demand """
        """ matrix indices are shifted by number of nodes to account """
	for i in range(mean):
		perm = np.random.permutation(node_list)
		for j, k in enumerate(perm):
			if (j, k) in demand_dict.keys():
				demand_dict[j, k] += 1
			else:
				demand_dict[j, k] = 1

	""" add 'std_dev' number of additional cycles to the demand """
	for i in range(std_dev):
		cycle_len = np.random.choice(range(1, num_nodes+1))
		cycle = np.random.choice(node_list, cycle_len)
		cycle = set(cycle)
		cycle = list(cycle)
		cycle.append(cycle[0])
		for j in range(len(cycle[:-1])):
			if (cycle[j], cycle[j+1]) in demand_dict.keys():
				demand_dict[cycle[j], cycle[j+1]] += 1
			else:
				demand_dict[cycle[j], cycle[j+1]] = 1			

	""" remove diagonal entries of demand matrix """
	for (i, j) in demand_dict.keys():
		if i == j:
			del demand_dict[i, j]

	return demand_dict


# parse arguments
parser = argparse.ArgumentParser(description="Create arbitrary txn workloads to run the omnet simulator on")
parser.add_argument('--graph-topo', \
        choices=['hotnets_topo', 'simple_line', 'simple_deadlock', 'custom'],\
        help='type of graph (Small world or scale free or custom topology)', default='simple_line')
parser.add_argument('--payment-graph-type', choices=['dag', 'circulation'], \
	help='type of payment graph (dag or circulation)', default='circulation')
parser.add_argument('--topo-filename', dest='topo_filename', type=str, \
        help='name of topology file to generate worklooad for')
parser.add_argument('output_file_prefix', type=str, help='name of the output workload file', \
        default='simple_workload.txt')
parser.add_argument('interval_distribution', choices=['uniform', 'poisson'],\
        help='time between transactions is determine by this', default='poisson')
parser.add_argument('--experiment-time', dest='total_time', type=int, \
        help='time to generate txns for', default=30)
parser.add_argument('--txn-size-mean', dest='txn_size_mean', type=int, \
        help='mean_txn_size', default=1)
parser.add_argument('--exp_size', action='store_true', help='should txns be exponential in size')
parser.add_argument('--generate-json-also', action="store_true", help="do you need to generate json file also \
        for the custom topology")
parser.add_argument('--balance-per-channel', type=int, dest='balance_per_channel', default=100)

args = parser.parse_args()

output_prefix = args.output_file_prefix
is_circulation = True if args.payment_graph_type == 'circulation' else False
distribution = args.interval_distribution
total_time = args.total_time
txn_size_mean = args.txn_size_mean
exp_size = args.exp_size
topo_filename = args.topo_filename
generate_json_also = args.generate_json_also
graph_topo = args.graph_topo
balance = args.balance_per_channel



# generate workloads
np.random.seed(SEED)
random.seed(SEED)
if graph_topo != 'custom':
    generate_workload_standard(output_prefix, graph_topo, distribution, \
            total_time, exp_size, txn_size_mean, generate_json_also, is_circulation)
elif topo_filename is None:
    raise Exception("Topology needed for custom file")
else:
    whole_graph, inside_graph, end_host_map = parse_topo(topo_filename)
    generate_workload_for_provided_topology(output_prefix, inside_graph, whole_graph, end_host_map,\
            distribution, total_time, exp_size,\
            txn_size_mean, generate_json_also, is_circulation)
