import json
import networkx as nx
from config import *
import matplotlib.pyplot as plt
import numpy as np
import collections

# read the relevant JSON and produce some stats
# on the channel distribution
def read_file(filename, file_format="LND"):
    if file_format == "LND":
        lnd_graph, capacity_list = read_file_lnd(filename)
    else:
        lnd_graph, capacity_list =  read_file_c_lightning(filename)
    
    print("Number of nodes in lnd graph:", lnd_graph.number_of_nodes())
    print("Number of edge in lnd graph:", lnd_graph.number_of_edges())

    capacities = nx.get_edge_attributes(lnd_graph, "capacity")
    capacities = [float(str(c))/SAT_TO_EUR for c in list(capacities.values()) if float(str(c))/SAT_TO_EUR > 2.0]
    print(len(capacities))
    plt.hist(capacities, bins=100, density=True, cumulative=True)
    print(np.mean(np.array(capacities)), "stddev" , np.std(np.array(capacities)), "min", min(capacities), "max", max(capacities))
    plt.show()

    return lnd_graph

# read a json captured from listchannels and listnodes from c-lightning
def read_file_c_lightning(filename):
    capacity_list = []
    node_list = dict()
    lnd_graph = nx.Graph()
    
    with open(filename + "_nodes.json") as f:
        data = json.load(f)
        for i, node in enumerate(data):
            node_list[node["nodeid"]] = i
            
    with open(filename + "_channels.json") as f:
        data = json.load(f)
        
        for edge in data:
            n1 = edge["source"]
            n2 = edge["destination"]
            cap = edge["satoshis"]
            capacity_list.append(cap)

            try: 
                n1_id = node_list[n1]
                n2_id = node_list[n2]

            except:
                print("nodes for edge ", n1, "->", n2, "not found")
                continue

            lnd_graph.add_edge(n1_id, n2_id, capacity=cap)
    return lnd_graph, capacity_list

    
# read a json produced by lncli describegraph from LND
def read_file_lnd(filename):
    capacity_list = []
    with open(filename + ".json") as f:
        lnd_graph = nx.Graph()
        data = json.load(f)

        node_list = dict()

        for i, node in enumerate(data["nodes"]):
            node_list[node["pub_key"]] = i
        
        for edge in data["edges"]:
            n1 = edge["node1_pub"]
            n2 = edge["node2_pub"]
            cap = edge["capacity"]
            capacity_list.append(cap)

            try: 
                n1_id = node_list[n1]
                n2_id = node_list[n2]

            except:
                print("nodes for edge ", n1, "->", n2, "not found")
                continue

            lnd_graph.add_edge(n1_id, n2_id, capacity=cap)
    return lnd_graph, capacity_list

# retrieve the subgraph of nodes that have degree more than the passed number
def remove_nodes_based_on_degree(graph, degree):
    new_nodes = [n for n in graph.nodes() if graph.degree[n] > degree]
    new_graph = graph.subgraph(new_nodes)
    print("Number of nodes in sub graph:", new_graph.number_of_nodes())
    print("Number of edge in sub graph:", new_graph.number_of_edges())

    """ 
    capacities = nx.get_edge_attributes(new_graph, "capacity")
    capacities = [float(str(c))/SAT_TO_EUR for c in capacities.values() if float(str(c))/SAT_TO_EUR > 2.0]
    plt.hist(capacities, bins=100, normed=True, cumulative=True)
    print np.mean(np.array(capacities)), "stddev" , np.std(np.array(capacities)), "min", min(capacities)
    plt.show()
    """

    return new_graph

# plot a histogram of degree distribution for given graph
# from: https://networkx.github.io/documentation/stable/auto_examples/drawing/plot_degree_histogram.html
def plot_degree_distribution(graph):
    degree_cap = {}
    for n, d in graph.degree():
        current_node_sum = 0
        for edge in graph.edges(n, data=True):
            current_node_sum += float(edge[2]["capacity"])/SAT_TO_EUR
        sum_for_this_degree = degree_cap.get(d, 0)
        degree_cap[d] = sum_for_this_degree + current_node_sum

    degree_sequence = sorted([d for n, d in graph.degree()], reverse=True)  # degree sequence
    degreeCount = collections.Counter(degree_sequence)
    deg, cnt = list(zip(*list(degreeCount.items())))
    print(degreeCount)
    print(degree_cap)
    fig, ax = plt.subplots()
    plt.plot(deg, cnt)

    plt.title("Degree Histogram")
    plt.ylabel("Count")
    plt.xlabel("Degree")
    plt.xscale("log")
    plt.show()


lnd_file_list = ["lnd_dec4_2018", "lnd_dec28_2018", "lnd_july15_2019", "clightning_oct5_2020"]
for filename in lnd_file_list:
    file_format = "LND" if "lnd" in filename else "c-lightning"
    graph = read_file(LND_FILE_PATH + filename, file_format)
    #new_graph = remove_nodes_based_on_degree(graph, 6)
    plot_degree_distribution(graph)
    nx.write_edgelist(graph, LND_FILE_PATH + filename + ".edgelist")
