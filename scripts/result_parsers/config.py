import os
# Filenames for Kaggle data
HOME = os.getenv('HOME')
OMNET = os.getenv('OMNET')
KAGGLE_PATH = HOME + '/' + OMNET + '/samples/spider_omnet/scripts/data/'
KAGGLE_AMT_DIST_FILENAME = KAGGLE_PATH + 'amt_dist.npy'
KAGGLE_AMT_MODIFIED_DIST_FILENAME = KAGGLE_PATH + 'amt_dist_cutoff.npy'
KAGGLE_TIME_DIST_FILENAME = KAGGLE_PATH + 'time_dist.npy'
PATH_PKL_DATA = "path_data/"
SAT_TO_EUR = 9158
LND_FILE_PATH = HOME + "/" + OMNET + "/samples/spider_omnet/lnd_data/"

# CONSTANTS
SCALE_AMOUNT = 30
MEAN_RATE = 10
MIN_TXN_SIZE = 0.1
MAX_TXN_SIZE = 10
SMALLEST_UNIT=1
MEASUREMENT_INTERVAL = 200 # transStatEnd - start in experiments

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
