import sys
import textwrap
import argparse
import networkx as nx
from config import *
import re
import os
import math
import random
import numpy as np

def parse_node_name(node_name, max_router, max_host):
    try:
        val = int(node_name[:-1])
        if(node_name[-1] == 'r'):
            if(val > max_router):
                max_router = val
            return ("router[" + str(val) + "]", max_router, max_host)
        if(node_name[-1] == 'e'):
            if(val > max_host):
                max_host = val
            return ("host[" + str(val) + "]", max_router, max_host)
        return -1
    except:
        return -1



# take the topology file in a specific format and write it to a ned file
def write_ned_file(topo_filename, output_filename, network_name, routing_alg):
    # topo_filename must be a text file where each line contains the ids of two neighbouring nodes that 
    # have a payment channel between them, relative delays in each direction,  initial balance on each 
    # end (see sample-topology.txt)
    # each line is of form:
    # [node1] [node2] [1->2 delay] [2->1 delay] [balance @ 1] [balance @ 2]
    topo_file = open(topo_filename).readlines() 
    outfile = open(output_filename, "w")

    # metadata used for forwarding table
    neighbor_interfaces = dict()
    node_interface_count = dict()
    node_used_interface = dict()
    linklist = list()
    max_val = -1 #used to find number of nodes, assume nodes start at 0 and number consecutively
    max_router = -1
    max_host = -1
    line_num = 0
    for line in topo_file:
        line_num += 1

        # landmark line
        if line_num == 1:
            continue

        if line == "\n":
            continue

        n1 = parse_node_name(line.split()[0], max_router, max_host)
        if(n1 == -1):
            print("Bad line " + line)
            continue
        max_router = n1[1]
        max_host = n1[2]

        n2 = parse_node_name(line.split()[1], max_router, max_host)
        if(n2 == -1):
            print("Bad line " + line)
            continue
        max_router = n2[1]
        max_host = n2[2]

        n3 = float(line.split()[2]) # delay going from n1 to n2
        n4 = float(line.split()[3]) # delay going from n2 to n1

        linklist.append((n1[0], n2[0], n3, n4))

    max_router = max_router + 1
    max_host = max_host + 1

    # generic routerNode and hostNode definition that every network will have
    print(routing_alg)
    if (routing_alg == 'shortestPath'):
        host_node_type = 'hostNodeBase'
        router_node_type = 'routerNodeBase'
    else:
        if routing_alg == 'DCTCPBal' or routing_alg == 'DCTCPQ' or routing_alg == 'TCP' or routing_alg == 'TCPCubic':
            host_node_type = 'hostNodeDCTCP'
        elif routing_alg == 'DCTCPRate':
            host_node_type = 'hostNodePropFairPriceScheme'
        else:
            host_node_type = 'hostNode' + routing_alg[0].upper() + routing_alg[1:]
        
        if routing_alg == 'landmarkRouting':
            router_node_type = 'routerNodeWaterfilling'
        elif routing_alg == 'DCTCPRate' or routing_alg == 'DCTCPQ' or routing_alg == 'TCP' or routing_alg == 'TCPCubic':
            router_node_type = 'routerNodeDCTCP'
        else:
            router_node_type = 'routerNode' + routing_alg[0].upper() + routing_alg[1:]
        print(router_node_type)

    outfile.write("import " + router_node_type + ";\n")
    outfile.write("import " + host_node_type + ";\n\n")

    outfile.write("network " + network_name + "_" + routing_alg + "\n")
    outfile.write("{\n")

    # This script (meant for a simpler datacenter topology) just assigns the same link delay to all links.
    # You need to change this such that the parameter values are instead assigned on a per node basis and 
    # are read from an additional 'delay' column and 'channel balance' columns in the text file.
    outfile.write('\tparameters:\n\t\tdouble linkDelay @unit("s") = default(100us);\n')
    outfile.write('\t\tdouble linkDataRate @unit("Gbps") = default(1Gbps);\n')
    outfile.write('\tsubmodules:\n')
    outfile.write('\t\thost['+str(max_host)+']: ' + host_node_type + ' {} \n')
    outfile.write('\t\trouter['+str(max_router)+']: ' + router_node_type + ' {} \n')
    outfile.write('\tconnections: \n')

    for link in linklist:
        a = link[0]
        b = link[1]
        abDelay = link[2]
        baDelay = link[3]

        outfile.write('\t\t' + a + '.out++ --> {delay = ' + str(abDelay) +'ms; }')
        outfile.write(' --> ' + b + '.in++;  \n')
        outfile.write('\t\t' + a + '.in++ <-- {delay = ' + str(baDelay) +'ms; }')
        outfile.write(' <-- ' + b + '.out++;  \n')
    outfile.write('}\n')




# generate either a small world or scale free graph
def generate_graph(size, graph_type):
    if graph_type == 'random':
        G = nx.dense_gnm_random_graph(size, size * 5,seed=SEED)
    elif graph_type == 'small_world':
        G = nx.watts_strogatz_graph(size, 8, 0.25, seed=SEED)
    elif graph_type == 'small_world_sparse':
        G = nx.watts_strogatz_graph(size, size/8, 0.25, seed=SEED)
    elif graph_type == 'scale_free':
        # regular expts
        G = nx.barabasi_albert_graph(size, 8, seed=SEED) 

        # implementation, celer expts - 10 node graph
        # G = nx.barabasi_albert_graph(size, 5, seed=12)
    elif graph_type == 'scale_free_sparse':
        G = nx.barabasi_albert_graph(size, size/8, seed=SEED)
    elif graph_type == 'tree':
        G = nx.random_tree(size, seed=SEED)

    # remove self loops and parallel edges
    G.remove_edges_from(G.selfloop_edges())
    G = nx.Graph(G)

    print('Generated a ', graph_type, ' graph')
    print('number of nodes: ', G.number_of_nodes())
    print('Number of Edges: ', G.number_of_edges())
    print('Number of connected components: ', nx.number_connected_components(G))
    return G




# print the output in the desired format for write_ned_file to read
# generate extra end host nodes if need be
# make the first line list of landmarks for this topology
def print_topology_in_format(G, balance_per_channel, delay_per_channel, output_filename, separate_end_hosts,\
        randomize_init_bal=False, random_channel_capacity=False, lnd_capacity=False, is_lnd=False, rebalancing_enabled=False):
    f1 = open(output_filename, "w+")
    end_host_delay = delay_per_channel

    offset = G.number_of_nodes()
    if (separate_end_hosts == False):
        offset = 0

    nodes_sorted_by_degree = sorted(G.degree, key=lambda x: x[1], reverse=True)

    # generate landmarks based on degree
    i = 0
    landmarks, current_list = [], []
    max_degree = -1
    while len(landmarks) < NUM_LANDMARKS and i < len(nodes_sorted_by_degree):
        num_remaining = NUM_LANDMARKS - len(landmarks)
        if nodes_sorted_by_degree[i][1] == max_degree:
            current_list.append(nodes_sorted_by_degree[i][0])
        else:
            spaced_indices = np.round(np.linspace(0, len(current_list)-1, \
                    min(num_remaining, len(current_list)))).astype(int)
            if max_degree != -1:
                landmarks.extend([current_list[x] for x in spaced_indices])
            current_list = [nodes_sorted_by_degree[i][0]]
            max_degree = nodes_sorted_by_degree[i][1]
        i += 1
    if len(landmarks) < NUM_LANDMARKS:
        spaced_indices = np.round(np.linspace(0, len(current_list)-1, \
                    min(num_remaining, len(current_list)))).astype(int)
        landmarks.extend([current_list[x] for x in spaced_indices])

     
    # make the first line the landmarks and make them all router nodes
    for l in landmarks[:NUM_LANDMARKS]:
        f1.write(str(l) + "r ")
    f1.write("\n")

    total_budget = balance_per_channel * len(G.edges())
    weights = {e: min(G.degree(e[0]), G.degree(e[1])) for e in G.edges()}
    sum_weights = sum(weights.values())
    capacity_dict = dict()

    # get lnd capacity data
    lnd_capacities_graph = nx.read_edgelist(LND_FILE_PATH + 'lnd_july15_2019_reducedsize' + '.edgelist')
    lnd_capacities = list(nx.get_edge_attributes(lnd_capacities_graph, 'capacity').values()) 

    # write rest of topology
    real_rtts = np.loadtxt(LND_FILE_PATH + "ping_times_data")
    for e in G.edges():

        f1.write(str(e[0]) + "r " + str(e[1]) +  "r ")
        
        if not random_channel_capacity and is_lnd and "uniform" not in output_filename:
            delay_per_channel = np.random.choice(real_rtts) / 2.0
            f1.write(str(delay_per_channel) + " " + str(delay_per_channel) + " ")
        else:
            f1.write(str(delay_per_channel) + " " + str(delay_per_channel) + " ")
        
        if random_channel_capacity:
            balance_for_this_channel = -1
            while balance_for_this_channel < 2:
                balance_for_this_channel = round(np.random.normal(balance_per_channel, \
                        0.75 * balance_per_channel))
        
        elif lnd_capacity:
            balance_for_this_channel = -1
            while balance_for_this_channel < 40:
                balance_for_this_channel = round(np.random.choice(lnd_capacities) * \
                    (balance_per_channel / np.mean(lnd_capacities)))
        
        elif is_lnd and "uniform" not in output_filename:
            if "lessScale" in output_filename:
                balance_for_this_channel = int(round(G[e[0]][e[1]]['capacity'] *10 * balance_per_channel))
            else:
                balance_for_this_channel = int(round(G[e[0]][e[1]]['capacity'] * balance_per_channel))
        
        else:
            balance_for_this_channel = balance_per_channel

        capacity_dict[e] = balance_for_this_channel

        if randomize_init_bal:
            one_end_bal = random.randint(1, balance_for_this_channel)
            other_end_bal = balance_for_this_channel - one_end_bal
            f1.write(str(one_end_bal) + " " + str(other_end_bal) + "\n")
        else:
            f1.write(str(round(balance_for_this_channel/2)) + " " + \
                    str(round(balance_for_this_channel/2)) + "\n")

    # generate extra end host nodes
    if separate_end_hosts : 
        for n in G.nodes():
            f1.write(str(n) + "e " + str(n) + "r ")
            f1.write(str(end_host_delay) + " " + str(end_host_delay) + " ")
            if rebalancing_enabled:
                f1.write(str(REASONABLE_BALANCE) + " " + str(REASONABLE_ROUTER_BALANCE) + "\n")
            else:
                f1.write(str(LARGE_BALANCE/2) + " " + str(LARGE_BALANCE/2) + "\n")

        if args.graph_type == "parallel_graph":
            for (e,r) in zip([1,3], [0, 2]):
                f1.write(str(e) + "e " + str(r) + "r ")
                f1.write(str(end_host_delay) + " " + str(end_host_delay) + " ")
                f1.write(str(LARGE_BALANCE/2) + " " + str(LARGE_BALANCE/2) + "\n")
    f1.close()

    nx.set_edge_attributes(G, capacity_dict, 'capacity')

# parse arguments
parser = argparse.ArgumentParser(description="Create arbitrary topologies to run the omnet simulator on")
parser.add_argument('--num-nodes', type=int, dest='num_nodes', help='number of nodes in the graph', default=20)
parser.add_argument('--delay-per-channel', type=int, dest='delay_per_channel', \
        help='delay between nodes (ms)', default=30)
parser.add_argument('graph_type', choices=['small_world', 'scale_free', 'hotnets_topo', 'simple_line', 'toy_dctcp', \
        'simple_deadlock', 'simple_topologies', 'parallel_graph', 'dag_example', 'lnd_dec4_2018','lnd_dec4_2018lessScale', \
        'lnd_dec4_2018_randomCap', 'lnd_dec4_2018_modified', 'lnd_uniform', 'tree', 'random', \
        'lnd_july15_2019'], \
        help='type of graph (Small world or scale free or custom topology list)', default='small_world')
parser.add_argument('--balance-per-channel', type=int, dest='balance_per_channel', default=100)
parser.add_argument('--topo-filename', dest='topo_filename', type=str, \
        help='name of intermediate output file', default="topo.txt")
parser.add_argument('--network-name', type=str, dest='network_name', \
        help='name of the output ned filename', default='simpleNet')
parser.add_argument('--separate-end-hosts', action='store_true', \
        help='do you need separate end hosts that only send transactions')
parser.add_argument('--randomize-start-bal', type=str, dest='randomize_start_bal', \
        help='Do not start from pergect balance, but rather randomize it', default='False')
parser.add_argument('--random-channel-capacity', type=str, dest='random_channel_capacity', \
        help='Give channels a random balance between bal/2 and bal', default='False')
parser.add_argument('--lnd-channel-capacity', type=str, dest='lnd_capacity', \
        help='Give channels a random balance sampled from lnd', default='False')
parser.add_argument('--rebalancing-enabled', type=str, dest="rebalancing_enabled",\
        help="should the end host router channel be reasonably sized", default="false")
routing_alg_list = ['shortestPath', 'priceScheme', 'waterfilling', 'landmarkRouting', 'lndBaseline', \
        'DCTCP', 'DCTCPBal', 'DCTCPRate', 'DCTCPQ', 'TCP', 'TCPCubic', 'celer']

args = parser.parse_args()
np.random.seed(SEED)
random.seed(SEED)

# generate graph and print topology and ned file
if args.num_nodes <= 5 and args.graph_type == 'simple_topologies':
    if args.num_nodes == 2:
        G = two_node_graph
    elif args.num_nodes == 3:
        G = three_node_graph
    elif args.num_nodes == 4:
        G = four_node_graph
    elif 'line' in args.network_name:
        G = five_line_graph
    else:
        G = five_node_graph
elif args.graph_type in ['small_world', 'scale_free', 'tree', 'random']:
    if "sparse" in args.topo_filename:
        args.graph_type = args.graph_type + "_sparse"
    G = generate_graph(args.num_nodes, args.graph_type)
elif args.graph_type == 'toy_dctcp':
    G = toy_dctcp_graph
elif args.graph_type == 'dag_example':
    print("generating dag example")
    G = dag_example_graph
elif args.graph_type == 'parallel_graph':
    G = parallel_graph
elif args.graph_type == 'hotnets_topo':
    G = hotnets_topo_graph
elif args.graph_type == 'simple_deadlock':
    G = simple_deadlock_graph
    args.separate_end_hosts = False
elif args.graph_type.startswith('lnd_'):
    G = nx.read_edgelist(LND_FILE_PATH + 'lnd_july15_2019_reducedsize' + '.edgelist')
else:
    G = simple_line_graph
    args.separate_end_hosts = False

args.randomize_start_bal = args.randomize_start_bal == 'true'
args.random_channel_capacity = args.random_channel_capacity == 'true'
args.lnd_capacity = args.lnd_capacity == 'true'

print_topology_in_format(G, args.balance_per_channel, args.delay_per_channel, args.topo_filename, \
        args.separate_end_hosts, args.randomize_start_bal, args.random_channel_capacity,\
        args.lnd_capacity, args.graph_type.startswith('lnd_'), args.rebalancing_enabled == "true")
network_base = os.path.basename(args.network_name)

for routing_alg in routing_alg_list:
    write_ned_file(args.topo_filename, args.network_name + '_' + routing_alg + '.ned', \
            network_base, routing_alg)



