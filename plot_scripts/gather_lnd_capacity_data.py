import networkx as nx
import sys
import numpy as np

EXCHANGE_RATE = 9158
# 1 EUR = 9158 SAT

filename= sys.argv[1]

graph = nx.read_edgelist(filename)
capacities = nx.get_edge_attributes(graph, 'capacity')
capacities = [float(c) / EXCHANGE_RATE for c in capacities.values()]

ggplot_file = open("lnd_cap_data", "w+")
ggplot_file.write("values\n")
for c in capacities:
    ggplot_file.write(str(c) + "\n")
ggplot_file.close()

print "Average", np.average(capacities)
print "Median", np.median(capacities)
print "min", min(capacities)
print "max", max(capacities)
