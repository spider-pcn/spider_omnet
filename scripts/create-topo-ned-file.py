import sys
import textwrap

linkfile = open(sys.argv[1]).readlines() 
#a text file where each line contains the ids of two neighbouring nodes that have a payment channel between them, relative delays in each direction, initial balance on each end (see sample-topology.txt)
#each line is of form:
# [node1] [node2] [1->2 delay] [2->1 delay] [balance @ 1] [balance @ 2]
outfile = open(sys.argv[2], "w")
try:
  networkName = sys.argv[3]
except:
  networkName = "simpleNet"

#metadata used for forwarding table
neighborInterfaces = dict()

nodeInterfaceCount = dict()
nodeUsedInterface = dict()
linklist = list()
maxVal=-1 #used to find number of nodes, assume nodes start at 0 and number consecutively
for line in linkfile:
  n1 = int(line.split()[0])
  n2 = int(line.split()[1])
  if (n1 > maxVal):
      maxVal = n1
  if (n2 > maxVal):
      maxVal = n2
  n3 = float(line.split()[2]) #delay going from n1 to n2
  n4 = float(line.split()[3]) #delay going from n2 to n1
  linklist.append((n1,n2,n3,n4))
maxVal = maxVal + 1

#replace with any packages (if any) you need to import for your ned file.
"""
outfile.write("package mlnxnet.simulations.GenericFatTree;\n")
outfile.write("import inet.networklayer.autorouting.ipv4.IPv4NetworkConfigurator;\n")
outfile.write("import mlnxnet.nodes.inet.MLNX_StandardHost;\n")
outfile.write("import mlnxnet.nodes.ethernet.MLNX_EtherSwitch;\n")
outfile.write("import mlnxnet.nodes.ethernet.MLNX_EtherSwitch4FatTree;\n")
"""

outfile.write("import routerNode;\n\n")

outfile.write("network " + networkName + "\n")
outfile.write("{\n")



# This script (meant for a simpler datacenter topology) just assigns the same link delay to all links.
# You need to change this such that the parameter values are instead assigned on a per node basis and are read from an additional 'delay' column and 'channel balance' columns in the text file.
outfile.write('\tparameters:\n\t\tdouble linkDelay @unit("s") = default(100us);\n\t\tdouble linkDataRate @unit("Gbps") = default(1Gbps);\n')
outfile.write("\tsubmodules:\n\t\tnode["+str(maxVal)+"]: routerNode {} \n connections: \n")

for link in linklist:
  a = link[0]
  b = link[1]
  abDelay = link[2]
  baDelay = link[3]

  outfile.write('\t\tnode[' + str(a) + '].out++ --> {delay = ' + str(abDelay) +'ms; } --> node[' + str(b) + '].in++;  \n')
  outfile.write('\t\tnode[' + str(a) + '].in++ <-- {delay = ' + str(baDelay) +'ms; } <-- node[' + str(b) + '].out++;  \n')

outfile.write('}\n')



