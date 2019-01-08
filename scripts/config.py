import networkx as nx


# create simple line graph
simple_line_graph = nx.Graph()
simple_line_graph.add_edge(0, 1)
simple_line_graph.add_edge(1, 2)
simple_line_graph.add_edge(3, 2)

# create hotnets topo
hotnets_topo_graph = nx.Graph()
hotnets_topo_graph.add_edge(3, 4)
hotnets_topo_graph.add_edge(2, 3)
hotnets_topo_graph.add_edge(2, 1)
hotnets_topo_graph.add_edge(3, 1)
hotnets_topo_graph.add_edge(0, 4)
hotnets_topo_graph.add_edge(0, 1)

# create simple deadlock
simple_deadlock_graph = nx.Graph()
simple_deadlock_graph.add_edge(0, 1)
simple_deadlock_graph.add_edge(1, 2)

# LIST OF STANDARD ROUTER GRAPHS
# two node graph
two_node_graph = nx.Graph()
two_node_graph.add_edge(0, 1)

# three node graph - triangle
three_node_graph = nx.Graph()
three_node_graph.add_edge(0, 1)
three_node_graph.add_edge(1, 2)
three_node_graph.add_edge(2, 0)

# four node graph - square
four_node_graph = nx.Graph()
four_node_graph.add_edge(0, 1)
four_node_graph.add_edge(1, 2)
four_node_graph.add_edge(2, 3)
four_node_graph.add_edge(3, 0)

# five node graph - pentagon
five_node_graph = nx.Graph()
five_node_graph.add_edge(0, 1)
five_node_graph.add_edge(1, 2)
five_node_graph.add_edge(2, 3)
five_node_graph.add_edge(3, 4)
five_node_graph.add_edge(4, 0)


# CONSTANTS
SEED = 17
SCALE_AMOUNT = 5
MEAN_RATE = 10
CIRCULATION_STD_DEV = 2
LARGE_BALANCE = 1000000000

# json parameters for lnd testbed
ENDHOST_LND_ONE_WAY_CAPACITY = 1000000000

LND_FILE_PATH = "../lnd_data/"



''' OMNET SPECIFIC STUFF '''
# maximum number of paths to consider or plot data for
MAX_K = 4

# List of recognized and parsable omnet signals
RECOGNIZED_OMNET_SIGNALS = ["numInQueuePerChannel",\
        "numSentPerChannel", "balancePerChannel", "numProcessedPerChannel",\
        "numAttemptedPerDest_Total", "numCompletedPerDest_Total", "numTimedOutPerDest_Total",\
        "fracSuccessfulPerDest_Total",\
        "bottleneckPerDestPerPath", "rateCompletedPerDestPerPath", "rateAttemptedPerDestPerPath"]


# map of what field to parse for what plot
INTERESTING_SIGNALS = dict()
INTERESTING_SIGNALS["completion_rate_cdfs"] = ["rateCompletedPerDestPerPath",\
        "rateAttemptedPerDestPerPath"]
INTERESTING_SIGNALS["balance"] = ["balancePerChannel"]
INTERESTING_SIGNALS["queue_info"] = ["numInQueuePerChannel"]
INTERESTING_SIGNALS["num_sent_per_channel"] = ["numSentPerChannel"]


