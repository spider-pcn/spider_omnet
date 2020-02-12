import networkx as nx
import os

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

# topology for dctcp
toy_dctcp_graph = nx.Graph()
toy_dctcp_graph.add_edge(0, 1)
toy_dctcp_graph.add_edge(0, 2)
toy_dctcp_graph.add_edge(0, 4)
toy_dctcp_graph.add_edge(0, 6)
toy_dctcp_graph.add_edge(0, 8)
toy_dctcp_graph.add_edge(0, 10)
toy_dctcp_graph.add_edge(1, 3)
toy_dctcp_graph.add_edge(1, 5)
toy_dctcp_graph.add_edge(1, 7)
toy_dctcp_graph.add_edge(1, 9)
toy_dctcp_graph.add_edge(1, 11)

# three node graph - triangle
dag_example_graph = nx.Graph()
dag_example_graph.add_edge(0, 1)
dag_example_graph.add_edge(1, 2)


# parallel graph
parallel_graph = nx.Graph()
parallel_graph.add_edge(0,2)
parallel_graph.add_edge(1,3)

# Filenames for Kaggle data
HOME = os.getenv('HOME')
OMNET = os.getenv('OMNET')
KAGGLE_PATH = HOME + '/' + OMNET + '/samples/spider_omnet/scripts/data/'
KAGGLE_AMT_DIST_FILENAME = KAGGLE_PATH + 'amt_dist.npy'
KAGGLE_AMT_MODIFIED_DIST_FILENAME = KAGGLE_PATH + 'amt_dist_cutoff.npy'
KAGGLE_TIME_DIST_FILENAME = KAGGLE_PATH + 'time_dist.npy'
PATH_PKL_DATA = "path_data/"
SAT_TO_EUR = 9158

# CONSTANTS
SEED = 23
SEED_LIST = [23, 4773, 76189, 99889, 1968, 2329]
SCALE_AMOUNT = 30
MEAN_RATE = 10
CIRCULATION_STD_DEV = 2
LARGE_BALANCE = 1000000000
REASONABLE_BALANCE = 15000 
REASONABLE_ROUTER_BALANCE = 1000
MIN_TXN_SIZE = 0.1
MAX_TXN_SIZE = 10
SMALLEST_UNIT=1
MEASUREMENT_INTERVAL = 200 # transStatEnd - start in experiments

EC2_INSTANCE_ADDRESS="ec2-34-224-216-215.compute-1.amazonaws.com"
PORT_NUMBER=8000

# json parameters for lnd testbed
ENDHOST_LND_ONE_WAY_CAPACITY = 1000000000
ROUTER_CAPACITY = 100
LND_FILE_PATH = HOME + "/" + OMNET + "/samples/spider_omnet/lnd_data/"
LOG_NORMAL_MEAN=-0.6152
LOG_NORMAL_SCALE=0.7310



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
INTERESTING_SIGNALS["numCompleted"] = ["numCompletedPerDest_Total"] 
INTERESTING_SIGNALS["numArrived"] = ["numArrivedPerDest_Total"] 
INTERESTING_SIGNALS["rateArrived"] = ["rateArrivedPerDest_Total"]
INTERESTING_SIGNALS["rateToSendTrans"] = ["rateToSendTransPerDestPerPath"]
INTERESTING_SIGNALS["rateSent"] = ["rateSentPerDestPerPath"]
INTERESTING_SIGNALS["window"] = ["windowPerDestPerPath"]
INTERESTING_SIGNALS["sumOfTransUnitsInFlight"] = ["sumOfTransUnitsInFlightPerDestPerPath"]
INTERESTING_SIGNALS["priceLastSeen"] = ["priceLastSeenPerDestPerPath"]
INTERESTING_SIGNALS["fractionMarked"] = ["fractionMarkedPerDestPerPath"]
INTERESTING_SIGNALS["measuredRTT"] = ["measuredRTTPerDestPerPath"]
INTERESTING_SIGNALS["smoothedFractionMarked"] = ["smoothedFractionMarkedPerDestPerPath"]
INTERESTING_SIGNALS["rateOfAcks"] = ["rateOfAcksPerDestPerPath"]

# DO NOT CHANGE THIS: PAINFULLY HARDCODED TO NOT INTERFERE WITH numTimedOutAtSender
INTERESTING_SIGNALS["numTimedOutPerDest"] = ["numTimedOutPerDest"]

per_dest_list = []
for signal in ["numWaiting", "probability", "bottleneck", "pathPerTrans", \
        "fracSuccessful", "demandEstimate", "destQueue", "queueTimedOut"]:
    INTERESTING_SIGNALS[signal] = signal + "PerDest"
    per_dest_list.append(signal + "PerDest")
per_dest_list.extend(["rateCompletedPerDest_Total", "rateArrivedPerDest_Total", \
        "numCompletedPerDest_Total", "numArrivedPerDest_Total", \
        "rateToSendTransPerDestPerPath", "rateSentPerDestPerPath", "rateOfAcksPerDestPerPath", \
        "fractionMarkedPerDestPerPath", "sumOfTransUnitsInFlightPerDestPerPath", "windowPerDestPerPath", \
        "priceLastSeenPerDestPerPath", "numTimedOutPerDest", "smoothedFractionMarkedPerDestPerPath",\
        "measuredRTTPerDestPerPath"])

per_channel_list = []
for signal in ["balance", "numInQueue", "lambda", "muLocal", "xLocal", "nValue", "serviceRate", "arrivalRate",
        "inflightOutgoing", "inflightIncoming", 'queueDelayEWMA', 'fakeRebalanceQ', "capacity", "bank", "kStar", \
        "numSent", "muRemote", "numInflight", "timeInFlight", "explicitRebalancingAmt", "implicitRebalancingAmt"]:
    INTERESTING_SIGNALS[signal] = signal + "PerChannel"
    per_channel_list.append(signal + "PerChannel")

INTERESTING_SIGNALS["per_src_dest_plot"] = per_dest_list
INTERESTING_SIGNALS["per_channel_plot"] = per_channel_list

# added for celer_network
per_channel_dest_list = []
for signal in ["cpi"]:
    INTERESTING_SIGNALS[signal] = signal + "PerChannelPerDest"
    per_channel_dest_list.append(signal + "PerChannelPerDest")

INTERESTING_SIGNALS["per_channel_dest_plot"] = per_channel_dest_list




## ggplot related constants
PLOT_DIR = "data/"
GGPLOT_DATA_DIR = "ggplot_data/"
SUMMARY_DIR = "figures/"
RESULT_DIR = HOME + "/" + OMNET + "/samples/spider_omnet/benchmarks/circulations/results/"

# define scheme codes for ggplot
SCHEME_CODE = { "priceSchemeWindow": "PS",\
        "lndBaseline": "LND",\
        "landmarkRouting": "LR",\
        "shortestPath": "SP",\
        "waterfilling": "WF",\
        "DCTCP": "DCTCP",\
        "DCTCPRate": "DCTCPRate", \
        "DCTCPQ": "DCTCP_qdelay", \
        "DCTCPBal": "DCTCPBal",\
        "celer": "celer"}

# define actual dag percent mapping for ggplot
PERCENT_MAPPING = { '0' : 0,\
        '20': 5, \
        '45' : 20, \
        '65' : 40 }
