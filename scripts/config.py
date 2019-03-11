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

# five node line
five_line_graph = nx.Graph()
five_line_graph.add_edge(0, 1)
five_line_graph.add_edge(1, 2)
five_line_graph.add_edge(2, 3)
five_line_graph.add_edge(3, 4)

# Filenames for Kaggle data
KAGGLE_PATH = './data/'
KAGGLE_AMT_DIST_FILENAME = KAGGLE_PATH + 'amt_dist.npy'
KAGGLE_TIME_DIST_FILENAME = KAGGLE_PATH + 'time_dist.npy'


# CONSTANTS
SEED = 23
SCALE_AMOUNT = 30
MEAN_RATE = 10
CIRCULATION_STD_DEV = 2
LARGE_BALANCE = 1000000000
MIN_TXN_SIZE = 0.1
MAX_TXN_SIZE = 10

EC2_INSTANCE_ADDRESS="ec2-18-213-2-219.compute-1.amazonaws.com"
PORT_NUMBER=8000

# json parameters for lnd testbed
ENDHOST_LND_ONE_WAY_CAPACITY = 1000000000
ROUTER_CAPACITY = 100
LND_FILE_PATH = "../lnd_data/"
LOG_NORMAL_MEAN=0.0
LOG_NORMAL_SCALE=0.5



''' OMNET SPECIFIC STUFF '''
# maximum number of paths to consider or plot data for
MAX_K = 4
NUM_LANDMARKS = MAX_K

# List of recognized and parsable omnet signals
RECOGNIZED_OMNET_SIGNALS = ["numInQueuePerChannel",\
        "numSentPerChannel", "balancePerChannel", "numProcessedPerChannel",\
        "numAttemptedPerDest_Total", "numCompletedPerDest_Total", "numTimedOutPerDest_Total",\
        "fracSuccessfulPerDest_Total",\
        "bottleneckPerDestPerPath", "rateCompletedPerDestPerPath", "rateAttemptedPerDestPerPath"]


# map of what field to parse for what plot
INTERESTING_SIGNALS = dict()
INTERESTING_SIGNALS["completion_rate_cdfs"] = ["rateCompletedPerDest",\
        "rateArrivedPerDest"]
INTERESTING_SIGNALS["rateCompleted"] = ["rateCompletedPerDest_Total"] 
INTERESTING_SIGNALS["rateArrived"] = ["rateArrivedPerDest_Total"]
INTERESTING_SIGNALS["rateToSendTrans"] = ["rateToSendTransPerDestPerPath"]
INTERESTING_SIGNALS["rateSent"] = ["rateSentPerDestPerPath"]
INTERESTING_SIGNALS["window"] = ["windowPerDestPerPath"]
INTERESTING_SIGNALS["sumOfTransUnitsInFlight"] = ["sumOfTransUnitsInFlightPerDestPerPath"]
INTERESTING_SIGNALS["priceLastSeen"] = ["priceLastSeenPerDestPerPath"]

# DO NOT CHANGE THIS: PAINFULLY HARDCODED TO NOT INTERFERE WITH numTimedOutAtSender
INTERESTING_SIGNALS["numTimedOutPerDest"] = ["numTimedOutPerDest"]

per_dest_list = []
for signal in ["numWaiting", "probability", "bottleneck", "pathPerTrans", \
        "fracSuccessful", "demandEstimate"]:
    INTERESTING_SIGNALS[signal] = signal + "PerDest"
    per_dest_list.append(signal + "PerDest")
per_dest_list.extend(["rateCompletedPerDest_Total", "rateArrivedPerDest_Total", \
        "rateToSendTransPerDestPerPath", "rateSentPerDestPerPath", \
        "sumOfTransUnitsInFlightPerDestPerPath", "windowPerDestPerPath", \
        "priceLastSeenPerDestPerPath", "numTimedOutPerDest"])

per_channel_list = []
for signal in ["balance", "numInQueue", "lambda", "muLocal", "xLocal", "nValue", "balSum", "inFlightSum", \
        "numSent", "muRemote", "numInflight"]:
    INTERESTING_SIGNALS[signal] = signal + "PerChannel"
    per_channel_list.append(signal + "PerChannel")

INTERESTING_SIGNALS["per_src_dest_plot"] = per_dest_list
INTERESTING_SIGNALS["per_channel_plot"] = per_channel_list

