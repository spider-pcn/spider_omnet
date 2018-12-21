import sys
import textwrap
import argparse
import networkx as nx
from config import *

# take the topology file in a specific format and write it to a ned file
def write_ned_file(topo_filename, output_filename, network_name):
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
    for line in topo_file:
        if line == "\n":
            continue
        n1 = int(line.split()[0])
        n2 = int(line.split()[1])
        if (n1 > max_val):
            max_val = n1
        if (n2 > max_val):
            max_val = n2
        n3 = float(line.split()[2]) # delay going from n1 to n2
        n4 = float(line.split()[3]) # delay going from n2 to n1
        linklist.append((n1, n2, n3, n4))
    max_val = max_val + 1

    # replace with any packages (if any) you need to import for your ned file.
    """
    outfile.write("package mlnxnet.simulations.GenericFatTree;\n")
    outfile.write("import inet.networklayer.autorouting.ipv4.IPv4NetworkConfigurator;\n")
    outfile.write("import mlnxnet.nodes.inet.MLNX_StandardHost;\n")
    outfile.write("import mlnxnet.nodes.ethernet.MLNX_EtherSwitch;\n")
    outfile.write("import mlnxnet.nodes.ethernet.MLNX_EtherSwitch4FatTree;\n")
    """

    # generic routerNode definition that every network will have
    outfile.write("import routerNode;\n\n")

    outfile.write("network " + network_name + "\n")
    outfile.write("{\n")

    # This script (meant for a simpler datacenter topology) just assigns the same link delay to all links.
    # You need to change this such that the parameter values are instead assigned on a per node basis and 
    # are read from an additional 'delay' column and 'channel balance' columns in the text file.
    outfile.write('\tparameters:\n\t\tdouble linkDelay @unit("s") = default(100us);\n')
    outfile.write('\t\tdouble linkDataRate @unit("Gbps") = default(1Gbps);\n')
    outfile.write('\tsubmodules:\n\t\tnode['+str(max_val)+']: routerNode {} \n connections: \n')

    for link in linklist:
        a = link[0]
        b = link[1]
        abDelay = link[2]
        baDelay = link[3]

        outfile.write('\t\tnode[' + str(a) + '].out++ --> {delay = ' + str(abDelay) +'ms; }')
        outfile.write(' --> node[' + str(b) + '].in++;  \n')
        outfile.write('\t\tnode[' + str(a) + '].in++ <-- {delay = ' + str(baDelay) +'ms; }')
        outfile.write(' <-- node[' + str(b) + '].out++;  \n')
    outfile.write('}\n')




# generate either a small world or scale free graph
def generate_graph(size, graph_type):
    if graph_type == 'small_world':
        G = nx.watts_strogatz_graph(size, size/8, 0.25)
    elif graph_type == 'scale_free':
        G = nx.scale_free_graph(size)

    # remove self loops and parallel edges
    G.remove_edges_from(G.selfloop_edges())
    G = nx.Graph(G)

    print 'Generated a ', graph_type, ' graph'
    print 'number of nodes: ', G.number_of_nodes()
    print 'Number of Edges: ', G.number_of_edges()
    return G




# print the output in the desired format for write_ned_file to read
# generate extra end host nodes if need be
def print_topology_in_format(G, balance_per_channel, delay_per_channel, output_filename, separate_end_hosts):
    f = open(output_filename, "w+")

    for e in G.edges():
        f.write(str(e[0]) + " " + str(e[1]) +  " ")
        f.write(str(delay_per_channel) + " " + str(delay_per_channel) + " ")
        f.write(str(balance_per_channel/2) + " " + str(balance_per_channel/2) + "\n")


    # generate extra end host nodes
    if separate_end_hosts: 
        f.write("\n")
        index = G.number_of_nodes()
        for n in G.nodes():
            f.write(str(n) + " " + str(index) + " ")
            f.write(str(delay_per_channel) + " " + str(delay_per_channel) + " ")
            f.write(str(0) + " " + str(LARGE_BALANCE) + "\n")
            index += 1
    f.close()




# parse arguments
parser = argparse.ArgumentParser(description="Create arbitrary topologies to run the omnet simulator on")
parser.add_argument('--num-nodes', type=int, dest='num_nodes', help='number of nodes in the graph', default=20)
parser.add_argument('--delay-per-channel', type=int, dest='delay_per_channel', \
        help='delay between nodes (ms)', default=30)
parser.add_argument('graph_type', choices=['small_world', 'scale_free', 'hotnets_topo', 'simple_line', \
        'simple_deadlock'], \
        help='type of graph (Small world or scale free or custom topology list)', default='small_world')
parser.add_argument('--balance-per-channel', type=int, dest='balance_per_channel', default=100)
parser.add_argument('--topo-filename', dest='topo_filename', type=str, \
        help='name of intermediate output file', default="topo.txt")
parser.add_argument('--network-name', type=str, dest='network_name', \
        help='name of the output ned filename', default='simpleNet')
parser.add_argument('--separate-end-hosts', action='store_true', \
        help='do you need separate end hosts that only send transactions')

args = parser.parse_args()


# generate graph and print topology and ned file
if args.graph_type in ['small_world', 'scale_free']:
    G = generate_graph(args.num_nodes, args.graph_type)
elif args.graph_type == 'hotnets_topo':
    G = hotnets_topo_graph
elif args.graph_type == 'simple_deadlock':
    G = simple_deadlock_graph
    args.separate_end_hosts = False
else:
    G = simple_line_graph
    args.separate_end_hosts = False


print_topology_in_format(G, args.balance_per_channel, args.delay_per_channel, args.topo_filename, \
        args.separate_end_hosts)
write_ned_file(args.topo_filename, args.network_name + '.ned', args.network_name)



