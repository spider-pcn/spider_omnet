import sys
import textwrap

linkfile = open(sys.argv[1]).readlines() 
#a text file where each line contains the ids of two neighbouring nodes that have a payment channel between them, relative delays in each direction, initial balance on each end (see sample-topology.txt)
#each line is of form:
# [node1] [node2] [1->2 delay] [2->1 delay] [balance @ 1] [balance @ 2]
outfile = open(sys.argv[2], "w")


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


outfile.write("simple routerNode\n")
outfile.write("{\n")
outfile.write("\tparameters:\n")

#signals
#outfile.write("\t\t \n")
"""
outfile.write('\t\t@signal[numInQueue](type="long"); \n')
outfile.write('\t\t@statistic[numInQueue](title="total num in queue"; source="numInQueue"; record=vector,stats; interpolationmode=none); \n\n')
outfile.write('\t\t@signal[numProcessed](type="long"); \n')
outfile.write('\t\t@statistic[numProcessed](title="num processing events in statRate time"; source="numProcessed"; record=vector,stats; interpolationmode=none); \n\n')

"""
outfile.write('\t\t@signal[completionTime](type="long"); \n')

outfile.write('\t\t@statistic[completionTime](title="completionTime"; source="completionTime"; record=vector,stats; interpolationmode=none); \n\n')
outfile.write('\t\t@display("i=block/routing"); \n\n')

outfile.write('\t\t@signal[numInQueuePerChannel*](type="long"); // note an asterisk and the type of emitted values  \n')
outfile.write('\t\t@statisticTemplate[numInQueuePerChannelTemplate](record=vector,stats);   \n\n')

outfile.write('\t\t@signal[numProcessedPerChannel*](type="long");  \n')
outfile.write('\t\t@statisticTemplate[numProcessedPerChannelTemplate](record=vector, stats);   \n\n')

outfile.write('\t\t@signal[numSentPerChannel*](type="long");  \n')
outfile.write('\t\t@statisticTemplate[numSentPerChannelTemplate](record=vector, stats);   \n\n')

outfile.write('\t\t@signal[numCompletedPerDest*](type="long");  \n')
outfile.write('\t\t@statisticTemplate[numCompletedPerDestTemplate](record=vector, stats);   \n\n')

outfile.write('\t\t@signal[numAttemptedPerDest*](type="long"); \n')
outfile.write('\t\t@statisticTemplate[numAttemptedPerDestTemplate](record=vector, stats);   \n\n')

outfile.write("\tgates:\n\t\tinput in[];\n\t\toutput out[];\n}\n\n")
outfile.write("network simpleNet\n")
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



