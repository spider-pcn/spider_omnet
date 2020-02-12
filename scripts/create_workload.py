import sys
import textwrap
import argparse
import numpy as np
import networkx as nx
import random
import json
from config import *
from max_circulation import *
import pickle
from functools import reduce

# generates the start and end nodes for a fixed set of topologies - hotnets/line/simple graph
def generate_workload_standard(filename, payment_graph_topo, workload_type, total_time, \
        log_normal, kaggle_size, txn_size_mean, timeout_value, generate_json_also, circ_frac, std_workload=True):
    # by default ASSUMES NO END HOSTS

    dag_frac = round(1 - circ_frac,3)
    demand_dict_dag = dict()
    demand_dict_circ = dict()

    # define start and end nodes and amounts
    # edge a->b in payment graph appears in index i as start_nodes[i]=a, and end_nodes[i]=b
    if payment_graph_topo == 'hotnets_topo':
        if circ_frac == 1:
            start_nodes = [0, 1, 2, 2, 3, 3, 4]
            end_nodes = [1, 3, 1, 4, 2, 0, 2]
            amt_relative = [1, 2, 1, 1, 1, 1, 1]
            amt_absolute = [SCALE_AMOUNT * x for x in amt_relative]
        else:
            start_nodes = [0,3,0,1,2,3,2,4]
            end_nodes = [4,0,1,3,1,2,4,2]
            amt_relative = [1,2,1,2,1,2,2,1]
            amt_absolute = [SCALE_AMOUNT * x for x in amt_relative]
        graph = hotnets_topo_graph

    elif payment_graph_topo == 'toy_dctcp':
        start_nodes = [2, 4, 6, 8, 10, 3, 5, 7, 9, 11]
        end_nodes = [3, 5, 7, 9, 11, 2, 4, 6, 8, 10]
        amt_relative = [1] * 10
        print("here generating topo")
        amt_absolute = [SCALE_AMOUNT * MEAN_RATE * x for x in amt_relative]
        graph = toy_dctcp_graph

    elif payment_graph_topo == 'simple_deadlock':
        start_nodes = [1,0,2]
        end_nodes = [2,2,0]
        amt_relative = [2,1,2]
        amt_absolute = [SCALE_AMOUNT * x for x in amt_relative]
        graph = simple_deadlock_graph

    elif payment_graph_topo == 'dag_example':
        start_nodes = [0, 2, 1]
        end_nodes = [2, 1, 2]
        amt_relative = [10, 5, 5]
        amt_absolute = [SCALE_AMOUNT * x for x in amt_relative]
        graph = dag_example_graph

    elif payment_graph_topo == 'parallel_graph':
        start_nodes = [0, 2, 1, 3]
        end_nodes = [2, 0, 3, 1]
        amt_relative = [1, 1, 1, 1]
        amt_absolute = [SCALE_AMOUNT * x for x in amt_relative]
        graph = parallel_graph


    elif payment_graph_topo == 'simple_line':
        if "five" in filename:
            num_nodes = 5
            graph = five_line_graph
        else:
            num_nodes = 3
            graph = simple_line_graph
        print(num_nodes)

        start_nodes = [0, num_nodes - 1]
        end_nodes = [num_nodes - 1, 0]
        amt_relative = [MEAN_RATE] * 2
        '''start_nodes = [0, 2, 0, 1] 
        end_nodes = [2, 0, 1, 0]
        amt_relative = [MEAN_RATE, MEAN_RATE, 2*MEAN_RATE, 2*MEAN_RATE]'''
        amt_absolute = [SCALE_AMOUNT * x for x in amt_relative]
        
        demand_dict_circ[0,num_nodes - 1] = MEAN_RATE
        demand_dict_circ[num_nodes - 1, 0] = MEAN_RATE
        demand_dict_dag[0, num_nodes - 1] = MEAN_RATE


    elif payment_graph_topo == 'hardcoded_circ':
        start_nodes = [0, 1, 2, 3, 4]
        end_nodes = [1, 2, 3, 4, 0]
        amt_relative = [MEAN_RATE] * 5
        amt_absolute = [SCALE_AMOUNT * x for x in amt_relative]
        graph = five_node_graph

    # generate circulation instead if you need a circulation
    if not std_workload:
        start_nodes, end_nodes, amt_relative = [], [], []
        num_nodes = graph.number_of_nodes()
    	
        """ generate circulation and dag demand """


        if circ_frac > 0:
    	    demand_dict_circ = circ_demand(list(graph), mean=MEAN_RATE, \
                    std_dev=CIRCULATION_STD_DEV)
        if dag_frac > 0:
            demand_dict_dag = dag_demand(list(graph), mean=MEAN_RATE, \
                    std_dev=CIRCULATION_STD_DEV)
                    
    demand_dict = { key: circ_frac * demand_dict_circ.get(key, 0) + 
            dag_frac * demand_dict_dag.get(key, 0) \
            for key in set(demand_dict_circ) | set(demand_dict_dag) }

    print(demand_dict)


    for i, j in list(demand_dict.keys()):
        start_nodes.append(i)
        end_nodes.append(j)
        amt_relative.append(demand_dict[i, j])
    if payment_graph_topo != 'hardcoded_circ':
        amt_absolute = [SCALE_AMOUNT * MEAN_RATE * x for x in amt_relative]

    print(amt_absolute)


    if generate_json_also:
        generate_json_files(filename + '.json', graph, graph, start_nodes, end_nodes, amt_absolute)

    write_txns_to_file(filename + '_workload.txt', start_nodes, end_nodes, amt_absolute,\
            workload_type, total_time, log_normal, kaggle_size, txn_size_mean, timeout_value)




# write the given set of txns denotes by start_node -> end_node with absolute_amts as passed in
# to a separate workload file
# workload file of form
# [amount] [timeSent] [sender] [receiver] [priorityClass] [timeout_value]
# write to file - assume no priority for now
# transaction sizes are either constant or exponentially distributed around their mean
def write_txns_to_file(filename, start_nodes, end_nodes, amt_absolute,\
        workload_type, total_time, log_normal, kaggle_size, txn_size_mean, timeout_value, mode="w", start_time=0):
    outfile = open(filename, mode)
    if "newseed" in filename:
        print("newseed")
        np.random.seed(12493)

    if distribution == 'uniform':
        # constant transaction size generated at uniform intervals
        for k in range(len(start_nodes)):
            cur_time = 0
            '''if (start_nodes[k] == 1 or end_nodes[k] == 1):
                cur_time = 300'''

            while cur_time < total_time:
                rate = amt_absolute[k]
                if log_normal:
                    txn_size = MIN_TXN_SIZE/10
                    while (txn_size < MIN_TXN_SIZE or txn_size > MAX_TXN_SIZE):
                        txn_power = np.random.normal(loc=LOG_NORMAL_MEAN, scale=LOG_NORMAL_SCALE)
                        txn_size = round(10 ** txn_power, 1) 
                else:
                    txn_size = txn_size_mean

                outfile.write(str(txn_size) + " " + str(cur_time + start_time) + " " + str(start_nodes[k]) + \
                        " " + str(end_nodes[k]) + " 0 " + str(timeout_value) + "\n")
                cur_time += (1.0 / rate)

    elif distribution == 'poisson':
        if kaggle_size:
            print("generating from kaggle for size")
            amt_dist = np.load(KAGGLE_AMT_DIST_FILENAME)
            num_amts = amt_dist.item().get('p').size

        # constant transaction size to be sent in a poisson fashion
        for k in range(len(start_nodes)):
            current_time = 0.0
            rate = amt_absolute[k]*1.0
            beta = (1.0) / (1.0 * rate)

            # if the rate is higher, given pair will have more transactions in a single second
            while current_time < total_time:

                if log_normal:
                    txn_size = MIN_TXN_SIZE/10
                    while (txn_size < MIN_TXN_SIZE or txn_size > MAX_TXN_SIZE):
                        txn_power = np.random.normal(loc=LOG_NORMAL_SCALE, scale=LOG_NORMAL_SCALE)
                        txn_size = round(10 ** txn_power, 1) 
                elif kaggle_size:
                    # draw an index according to the amount pmf
                    txn_idx = np.random.choice(num_amts, 1, \
                                           p=amt_dist.item().get('p'))[0]
                    # map the index to a tx amount
                    txn_size = int(round(amt_dist.item().get('bins')[txn_idx], 1))
                else:                
                    txn_size = txn_size_mean

                outfile.write(str(txn_size) + " " + str(current_time + start_time) + " " + str(start_nodes[k]) \
                        + " " + str(end_nodes[k]) + " 0 " + str(timeout_value) + "\n")

                time_incr = np.random.exponential(beta)
                current_time = current_time + time_incr 

    elif distribution == 'kaggle':
        # load the data
        amt_dist = np.load(KAGGLE_AMT_DIST_FILENAME)
        time_dist = np.load(KAGGLE_TIME_DIST_FILENAME)
        num_amts = amt_dist.item().get('p').size
        num_times = time_dist.item().get('p').size
        # transaction sizes drawn from kaggle data, as is inter-transaction time
        for k in range(len(start_nodes)):
            current_time = 0.0
            while current_time < total_time:
                # draw an index according to the amount pmf
                txn_idx = np.random.choice(num_amts, 1, \
                                           p=amt_dist.item().get('p'))[0]
                # map the index to a tx amount
                txn_size = amt_dist.item().get('bins')[txn_idx]
                outfile.write(str(txn_size) + " " + str(current_time + start_time) + " " + str(start_nodes[k]) + \
                        " " + str(end_nodes[k]) + " 0 " + str(timeout_value) + "\n")
                # draw an index according to the time pmf
                time_idx = np.random.choice(num_times, 1, \
                                            p=time_dist.item().get('p'))[0]
                # map index to an inter-tx time
                time_incr = time_dist.item().get('bins')[time_idx]
                current_time = current_time + time_incr

    outfile.close()
    np.random.seed(SEED_LIST[args.run_num])




# generates the json file necessary for the distributed testbed to be used to test
# the lnd implementation
def generate_json_files(filename, graph, inside_graph, start_nodes, end_nodes, amt_absolute):
    for balance in balance_list: 
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
                cap = balance * 400 / 2
                node_type = "r"

            if u <= v: 
                edge = {"src": str(u) + "r", "dst": str(v) + node_type, "capacity" : cap}
                edges.append(edge)

        json_string["lnd_channels"] = edges

        # creates the string for the demands
        demands = []
        for s, e, a in zip(start_nodes, end_nodes, amt_absolute):
            demand_entry = {"src": str(s) + "e", "dst": str(e) + "e",\
                            "rate": a}
            demands.append(demand_entry)

        json_string["demands"] = demands

        with open(filename + '_' + str(balance) + '.json', 'w') as outfile:
                json.dump(json_string, outfile, indent=8)




# generate workload for arbitrary topology by uniformly sampling
# the set of nodes for sender-receiver pairs
# size of transaction is determined when writing to the file to
# either be exponentially distributed or constant size
def generate_workload_for_provided_topology(filename, inside_graph, whole_graph, end_host_map, \
        workload_type, total_time, \
        log_normal, kaggle_size, txn_size_mean, timeout_value, generate_json_also, circ_frac):
    num_nodes = inside_graph.number_of_nodes()
    start_nodes, end_nodes, amt_relative = [], [], []
    

    """ generate circulation and dag demand """
    circ_frac = round(circ_frac, 3)
    dag_frac = round(1 - circ_frac, 3)
    demand_dict_dag = dict()
    demand_dict_circ = dict()

    if circ_frac > 0:
        demand_dict_circ = circ_demand(list(inside_graph), mean=MEAN_RATE, \
            std_dev=CIRCULATION_STD_DEV)
    if dag_frac > 0: 
        demand_dict_dag = dag_demand(list(inside_graph), mean=MEAN_RATE, \
            std_dev=CIRCULATION_STD_DEV, skew_param=dag_frac*10, gen_method="src_skew")
    
    circ_total = reduce(lambda x, value: x + value, iter(demand_dict_circ.values()), 0)
    dag_total = reduce(lambda x, value: x + value, iter(demand_dict_dag.values()), 0)

    if "weird" not in filename or dag_frac == 0.20 or dag_frac == 0.45:
        demand_dict = { key: circ_frac * demand_dict_circ.get(key, 0) + dag_frac * demand_dict_dag.get(key, 0) \
            for key in set(demand_dict_circ) | set(demand_dict_dag) } 
    else:
        # just add dag and don't weigh
        demand_dict = { key: demand_dict_circ.get(key, 0) + dag_frac * demand_dict_dag.get(key, 0) \
            for key in set(demand_dict_circ) | set(demand_dict_dag) } 

    total = reduce(lambda x, value: x + value, iter(demand_dict.values()), 0)
    
    print("Circulation", circ_total)
    print("Dag", dag_total)
    print("total", total)
    print(circ_frac)
    print(dag_frac)


    '''
    pkl_op = open(filename + '_demand.pkl', 'wb')
    pickle.dump(demand_dict, pkl_op)
    pkl_op.close()
    '''
    

    if "two_node_imbalance" in filename:
        demand_dict = dict()
        demand_dict[0, 1] = MEAN_RATE
        demand_dict[1, 0] = 5 * MEAN_RATE
        print(demand_dict)
    elif "two_node_capacity" in filename:
        demand_dict = dict()
        demand_dict[0, 1] = 2 * MEAN_RATE
        demand_dict[1, 0] = 5 * MEAN_RATE
        print(demand_dict)

    if "three_node" in filename:
        demand_dict = dict()
        demand_dict[0, 2] = MEAN_RATE
        demand_dict[1, 2] = MEAN_RATE
        demand_dict[2, 1] = MEAN_RATE
        demand_dict[1, 0] = MEAN_RATE

    for i, j in list(demand_dict.keys()):
    	start_nodes.append(end_host_map[i])
    	end_nodes.append(end_host_map[j])
    	amt_relative.append(demand_dict[i, j])	
    amt_absolute = [SCALE_AMOUNT * x for x in amt_relative]

    print("generated workload") 

    max_circ = max_circulation(demand_dict)
    if total != 0:
        print("ALERT!", "maximum circulation: ", max_circ, " or ", float(max_circ)/total)

    if generate_json_also:
        generate_json_files(filename, whole_graph, inside_graph, start_nodes, end_nodes, amt_absolute)


    if "weird" not in filename:
        print("generting txns here")
        write_txns_to_file(filename + '_workload.txt', start_nodes, end_nodes, amt_absolute,\
            workload_type, total_time, log_normal, kaggle_size, txn_size_mean, timeout_value)

    else:
        kaggle_size = False
        start_nodes_circ, end_nodes_circ, amt_relative_circ = [], [], []
        for i, j in list(demand_dict_circ.keys()):
            start_nodes_circ.append(end_host_map[i])
            end_nodes_circ.append(end_host_map[j])
            amt_relative_circ.append(demand_dict_circ[i, j])
        amt_absolute_circ = [SCALE_AMOUNT * x for x in amt_relative_circ]
        
        # circ for 1000s
        if dag_frac == 0.20 or dag_frac == 0.45 or dag_frac == 0.8:
            # dag plus circ for 2000s
            write_txns_to_file(filename + '_workload.txt', start_nodes, end_nodes, amt_absolute,\
                workload_type, 2000, log_normal, kaggle_size, txn_size_mean, timeout_value)

            # circ again for 1000s
            write_txns_to_file(filename + '_workload.txt', start_nodes_circ, end_nodes_circ, amt_absolute_circ,\
                workload_type, 1000, log_normal, kaggle_size, txn_size_mean, timeout_value, "a", 2000)

        else:
            write_txns_to_file(filename + '_workload.txt', start_nodes_circ, end_nodes_circ, amt_absolute_circ,\
            workload_type, 1000, log_normal, kaggle_size, txn_size_mean, timeout_value)

            # dag plus circ for 1000s
            write_txns_to_file(filename + '_workload.txt', start_nodes, end_nodes, amt_absolute,\
                workload_type, 1000, log_normal, kaggle_size, txn_size_mean, timeout_value, "a", 1000)

            # circ again for 1000s
            write_txns_to_file(filename + '_workload.txt', start_nodes_circ, end_nodes_circ, amt_absolute_circ,\
                workload_type, 1000, log_normal, kaggle_size, txn_size_mean, timeout_value, "a", 2000)


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

    line_num = 0
    with open(topo_filename) as topo_file:
        for line in topo_file:
            line_num += 1

            # landmark line
            if line_num == 1:
                continue

            if line == '\n':
                continue
            n1 = parse_node(line.split()[0])
            n2 = parse_node(line.split()[1])
            if n1 == -1 or n2 == -1:
                print("Bad line " + line)
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
    print("MEAN DEMAND", mean)

    assert type(mean) is int
    assert type(std_dev) is int

    demand_dict = {}
    num_nodes = len(node_list)

    """ sum of 'mean' number of random permutation matrices """
    """ note any permutation matrix is a circulation demand """
    """ matrix indices are shifted by number of nodes to account """
    for i in range(mean):
        perm = np.random.permutation(node_list)
        for j, k in enumerate(perm):
            if (j, k) in list(demand_dict.keys()):
                demand_dict[j, k] += 1
            else:
                demand_dict[j, k] = 1

    """ add 'std_dev' number of additional cycles to the demand """
    for i in range(std_dev):
        cycle_len = np.random.choice(list(range(1, num_nodes+1)))
        cycle = np.random.choice(node_list, cycle_len)
        cycle = set(cycle)
        cycle = list(cycle)
        cycle.append(cycle[0])
        for j in range(len(cycle[:-1])):
            if (cycle[j], cycle[j+1]) in list(demand_dict.keys()):
                demand_dict[cycle[j], cycle[j+1]] += 1
            else:
                demand_dict[cycle[j], cycle[j+1]] = 1			

    """ remove diagonal entries of demand matrix """
    for (i, j) in list(demand_dict.keys()):
        if i == j:
            del demand_dict[i, j]

    return demand_dict

# generate dag for node ids mentioned in node_list,
# with average total demand out of a node equal to 'mean', and a 
# perturbation of 'std_dev' 
def dag_demand(node_list, mean, std_dev, skew_param=5,gen_method="topological_sort"):
        print("DAG_DEMAND", mean)

        assert type(mean) is int
        assert type(std_dev) is int

        demand_dict = {}

        if gen_method == "src_skew":
            """ sample receiver uniformly at random and source from exponential distribution """
            for i in range(len(node_list) * mean):

                sender = len(node_list)
                while sender >= len(node_list):
                    sender = int(np.random.exponential(len(node_list)/skew_param))
                
                receiver_list = np.random.permutation(node_list)
                receiver_index = len(node_list)
                while receiver_index >= len(node_list):
                    receiver_index = int(np.random.exponential(len(node_list)/skew_param))
                receiver = receiver_list[receiver_index]

                demand_dict[sender, receiver] = demand_dict.get((sender, receiver), 0) + 1
        else:
            perm = np.random.permutation(node_list)
            print("root is ", perm[0])

            """ use a random ordering of the nodes """
            """ as the topological sort of the DAG demand to produce """
            """ generate demand from a node to only nodes higher """
            """ than it in the random ordering """
            for i, node in enumerate(perm[:-1]):
                receiver_node_list = perm[i + 1:]
                total_demand_from_node = mean + np.random.choice([std_dev, -1*std_dev])

                for j in range(total_demand_from_node):
                    receiver = np.random.choice(receiver_node_list)
                    demand_dict[node, receiver] = demand_dict.get((node, receiver), 0) + 1

        """ remove diagonal entries of demand matrix """
        for (i, j) in list(demand_dict.keys()):
                if i == j:
                        del demand_dict[i, j]

        return demand_dict


# parse arguments
parser = argparse.ArgumentParser(description="Create arbitrary txn workloads to run the omnet simulator on")
parser.add_argument('--graph-topo', \
        choices=['hotnets_topo', 'simple_line', 'simple_deadlock', 'custom', 'hardcoded_circ', 'toy_dctcp', 'dag_example', 'parallel_graph'],\
        help='type of graph (Small world or scale free or custom topology)', default='simple_line')
parser.add_argument('--payment-graph-dag-percentage', type=int,\
	help='percentage of circulation to put in the payment graph', default=0)
parser.add_argument('--topo-filename', dest='topo_filename', type=str, \
        help='name of topology file to generate worklooad for')
parser.add_argument('output_file_prefix', type=str, help='name of the output workload file', \
        default='simple_workload.txt')
parser.add_argument('interval_distribution', choices=['uniform', 'poisson','kaggle'],\
        help='time between transactions is determine by this', default='poisson')
parser.add_argument('--experiment-time', dest='total_time', type=int, \
        help='time to generate txns for', default=30)
parser.add_argument('--txn-size-mean', dest='txn_size_mean', type=int, \
        help='mean_txn_size', default=1)
parser.add_argument('--log-normal', action='store_true', help='should txns be exponential in size')
parser.add_argument('--kaggle-size', action='store_true', help='should txns be kaggle in size')
parser.add_argument('--generate-json-also', action="store_true", help="do you need to generate json file also \
        for the custom topology")
parser.add_argument('--balance-list', type=int, nargs='+', dest='balance_list', default=[100])
parser.add_argument('--timeout-value', type=float, help='generic time out for all transactions', default=5)
parser.add_argument('--scale-amount', type=int, help='how much to scale the mean deamnd by', default=5)
parser.add_argument('--run-num', type=int, help='influences the seed', default=1)


args = parser.parse_args()

output_prefix = args.output_file_prefix
circ_frac = (100 - args.payment_graph_dag_percentage) / 100.0
distribution = args.interval_distribution
total_time = args.total_time
txn_size_mean = args.txn_size_mean
log_normal = args.log_normal
kaggle_size = args.kaggle_size
topo_filename = args.topo_filename
generate_json_also = args.generate_json_also
graph_topo = args.graph_topo
balance_list = args.balance_list
timeout_value = args.timeout_value
SCALE_AMOUNT = args.scale_amount

if kaggle_size:
    log_normal = False


# generate workloads
np.random.seed(SEED_LIST[args.run_num])
random.seed(SEED_LIST[args.run_num])
if graph_topo != 'custom':
    generate_workload_standard(output_prefix, graph_topo, distribution, \
            total_time, log_normal, kaggle_size, txn_size_mean, timeout_value, generate_json_also, circ_frac)
elif topo_filename is None:
    raise Exception("Topology needed for custom file")
else:
    whole_graph, inside_graph, end_host_map = parse_topo(topo_filename)
    generate_workload_for_provided_topology(output_prefix, inside_graph, whole_graph, end_host_map,\
            distribution, total_time, log_normal, kaggle_size,\
            txn_size_mean, timeout_value, generate_json_also, circ_frac)
