import json
import subprocess
import pingparsing as pp
import statistics as stat
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import argparse
from config import *


parser = argparse.ArgumentParser(description='Collect ping times to lnd nodes')
parser.add_argument('--rerun-ping', action="store_true", help="whether to rerun the pings")
args = parser.parse_args()

# get the list of ip addresses to ping
def parse_json_data():
    with open(LND_FILE_PATH + 'lnd_july15_2019.json') as f:
        nodes = (json.load(f))["nodes"]

    ping_addresses = []
    for n in nodes:
        for a in n["addresses"]:
            if "onion" not in a["addr"]:
                ping_addresses.append(a["addr"])

    return ping_addresses




# ping a specific node at the given ip address and return stats for it
def ping_node(ip_addr):
    ping_parser = pp.PingParsing()
    transmitter = pp.PingTransmitter()
    transmitter.destination = ip_addr
    transmitter.count = 10
    result = transmitter.ping()
    try: 
        return ping_parser.parse(result.stdout).as_dict()
    except: 
        print(result)
        return None



# ping all nodes in the list and aggregate stats over all of them
# all stats are in milliseconds
def ping_nodes(addr_list):
    overall_rtts = []
    f = open(LND_FILE_PATH + "ping_times_data", "w+")
    for a in addr_list:
        ip_address = a.split(":")[0]
        result = ping_node(ip_address)
        rtt_avg = result["rtt_avg"] if result is not None else None

        if rtt_avg is not None:
            overall_rtts.append(rtt_avg)
            if (rtt_avg) < 10:
                print(ip_address, rtt_avg)
            f.write(str(rtt_avg) + "\n")
    f.close()

    print("Mean", stat.mean(overall_rtts))
    print("Median", stat.median(overall_rtts))
    print("Variance", stat.variance(overall_rtts))

    return overall_rtts


# parse rtts from text file
def parse_rtts_from_file():
    rtts = []
    with open(LND_FILE_PATH + "ping_times_data") as f:
        for line in f.readlines():
            if "None" not in f and " " not in f:
                rtts.append(float(line))
    return rtts


# visualize the data
def visualize_rtts(rtts):
    f = plt.figure()
    plt.hist(rtts, 100, density=True, histtype='step', cumulative=True)
    f.savefig("lnd_ping_rtt_spread.pdf")
    print("Mean", stat.mean(rtts))
    print("Median", stat.median(rtts))
    print("Variance", stat.variance(rtts))
    print("Max", max(rtts))
    print("Min", min(rtts))



address_list = parse_json_data()
print(len(address_list))
all_rtts = ping_nodes(address_list) if args.rerun_ping else parse_rtts_from_file()
visualize_rtts(all_rtts)
