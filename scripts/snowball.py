""" code to sample a small subgraph of a given input graph via 
the snowball sampling method.
Ref: https://arxiv.org/pdf/1308.5865.pdf """


import matplotlib.pyplot as plt 

import copy
import networkx as nx 
import random 
from config import *
import matplotlib.pyplot as plt
import numpy as np

random.seed(SEED)

def sample_ngbr(graph, ngbr_set, v_set, k):
    """ for each vertex in ngbr_set sample k random neighbors
    in graph """
    
    new_ngbr_set = set() 
    new_edge_set = set()

    for v in ngbr_set:
        ngbr = [u for u in graph.neighbors(v)]
        if len(ngbr) > k:
            ngbr_sample = set(random.sample(ngbr, k))
        else:
            ngbr_sample = set(ngbr)
        new_ngbr_set = new_ngbr_set | ngbr_sample
        new_edge_set = new_edge_set | {(u, v) for u in ngbr_sample}
    
    new_ngbr_set = new_ngbr_set - v_set
    return new_ngbr_set, new_edge_set



def snowball_sample(graph, init_seed, max_sample_size=100, k=4):
    """ sample a subgraph no more than max_sample_size starting with 
    vertices in init_seed """

    if len(graph.nodes()) <= max_sample_size:
        return graph

    assert len(init_seed) <= max_sample_size

    v_set = copy.deepcopy(init_seed)
    e_set = set()
    old_v_set = v_set
    old_e_set = e_set
    ngbr_set = copy.deepcopy(init_seed)

    while len(v_set) <= max_sample_size:
        new_ngbr_set, new_edge_set = sample_ngbr(graph, ngbr_set, v_set, k)
        old_v_set = v_set.copy()
        old_e_set = e_set.copy()
        v_set = v_set | new_ngbr_set
        e_set = e_set | new_edge_set
        ngbr_set = new_ngbr_set

    sampled_graph = nx.Graph()
    sampled_graph.add_nodes_from(list(old_v_set))
    sampled_graph.add_edges_from(list(old_e_set))

    for e in sampled_graph.edges():
        sampled_graph.edges[e]['capacity'] = graph.edges[e]['capacity']

    return sampled_graph



def prune_deg_one_nodes(sampled_graph):
    """ prune out degree one nodes from graph """
    deg_one_nodes = []
    for v in sampled_graph.nodes():
        if sampled_graph.degree(v) == 1:
            deg_one_nodes.append(v)

    for v in deg_one_nodes:
        sampled_graph.remove_node(v)

    return sampled_graph



def write_capacities_to_file(filename, capacities):
    with open(filename, "w+") as f:
        f.write("values\n")
        for c in capacities:
            f.write(str(c) + "\n")


#lnd_file_list = ["lnd_dec4_2018", "lnd_dec28_2018"]
#lnd_file_list = ["lnd_july15_2019"]
lnd_file_list = ["clightning_oct5_2020"]
for filename in lnd_file_list: 
    graph = nx.read_edgelist(LND_FILE_PATH + filename + ".edgelist")

    rename_dict = {v: int(str(v)) for v in graph.nodes()}
    graph = nx.relabel_nodes(graph, rename_dict)

    # convert all capacities to EUROS
    count = 0
    for e in graph.edges():
        edge_cap = round(graph.edges[e]['capacity'] / SAT_TO_EUR)
        if edge_cap < 10:
            edge_cap = 10
            count += 1
        graph.edges[e]['capacity'] = edge_cap
    print("massaged", count, "out of", len(graph.edges()), "edges")
    
    init_seed = {784, 549, 989}

    """ max_sample_size is the maximum size of sampled graph. Returned graph 
    might be smaller than that. k is how many neighbors to sample (by each node)
    in each round """
    sampled_graph = snowball_sample(graph, init_seed, max_sample_size=1000, k=12)

    """ prune out degree one nodes until there are no degree one nodes """
    graph_size = len(sampled_graph.nodes()) + 1
    while graph_size > len(sampled_graph.nodes()):
        graph_size = len(sampled_graph.nodes())
        sampled_graph = prune_deg_one_nodes(sampled_graph)

    
    """ make all node numbers start from 0 """
    numbered_graph = nx.convert_node_labels_to_integers(sampled_graph)
    print("graph size: ", numbered_graph.number_of_nodes(), " nodes" , \
            numbered_graph.number_of_edges(), " edges")

    nx.write_edgelist(numbered_graph, LND_FILE_PATH + filename + "_reducedsize" + ".edgelist")
        
    capacities = nx.get_edge_attributes(sampled_graph, 'capacity')
    capacities = [float(c) for c in list(capacities.values())]
    write_capacities_to_file(filename + "_data_min25", capacities)
    #plt.hist(capacities, bins=100, normed=True, cumulative=True)
    print(np.mean(np.array(capacities)), "stddev" , np.std(np.array(capacities),), min(capacities), \
            np.median(np.array(capacities)), np.percentile(np.array(capacities), 25))
    #plt.show()
