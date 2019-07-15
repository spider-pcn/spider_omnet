import json
import networkx as nx
from config import *

def read_file(filename):
    with open(filename) as f:
        lnd_graph = nx.Graph()
        data = json.load(f)

        node_list = dict()

        for i, node in enumerate(data["nodes"]):
            node_list[node["pub_key"]] = i
        
        for edge in data["edges"]:
            n1 = edge["node1_pub"]
            n2 = edge["node2_pub"]
            cap = edge["capacity"]

            try: 
                n1_id = node_list[n1]
                n2_id = node_list[n2]

            except:
                print "nodes for edge ", n1, "->", n2, "not found"
                continue

            lnd_graph.add_edge(n1_id, n2_id, capacity=cap)

    print "Number of nodes in lnd graph:", lnd_graph.number_of_nodes()
    print "Number of edge in lnd graph:", lnd_graph.number_of_edges()

    return lnd_graph

lnd_file_list = ["lnd_dec4_2018", "lnd_dec28_2018", "lnd_july15_2019"]
for filename in lnd_file_list: 
    graph = read_file(LND_FILE_PATH + filename + ".json")
    nx.write_edgelist(graph, LND_FILE_PATH + filename + ".edgelist")
