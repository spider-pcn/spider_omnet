import argparse
import os

# parse arguments
parser = argparse.ArgumentParser(description="Create ini file for the given arguments")
parser.add_argument('--workload-filename', type=str, dest='workload_filename', default='sample_workload.txt')
parser.add_argument('--topo-filename', type=str, help='name of intermediate output file', default='topo.txt')
parser.add_argument('--network-name', type=str, help='name of the output ned filename', default='simpleNet')
parser.add_argument('--ini-filename', type=str, help='name of ini file', default='omnetpp.ini')
parser.add_argument('--simulation-length', type=str, help='duration of simulation in seconds', dest='simulationLength', default='30.0')
parser.add_argument('--stat-collection-rate', type=str, help='rate of collecting stats', dest='statRate', default='0.2')
parser.add_argument('--signals-enabled', type=str, help='are signals enabled?', dest='signalsEnabled', default='false')
parser.add_argument('--logging-enabled', type=str, help='is logging enabled?', dest='loggingEnabled', default='false')
parser.add_argument('--timeout-clear-rate', type=str, help='rate of clearing data after timeouts', dest='timeoutClearRate', default='0.5')
parser.add_argument('--timeout-enabled', type=str, help='are timeouts enabled?', dest='timeoutEnabled', default='true')
parser.add_argument('--routing-scheme', type=str, help='must be in [shortestPath, waterfilling, LP, silentWhispers], else will default to waterfilling', dest='routingScheme', default='default')
parser.add_argument('--num-path-choices', type=str, help='number of path choices', dest='numPathChoices', default='default')
args = parser.parse_args()



configname = os.path.basename(args.network_name)

if(args.routingScheme != 'default'):
    configname = configname + "_" + args.routingScheme

#arg parse might support a cleaner way to deal with this
if(args.routingScheme not in ['shortestPath', 'waterfilling', 'LP', 'silentWhispers']):
    if(args.routingScheme != 'default'):
        print "******************"
        print "WARNING: ill-specified routing scheme, defaulting to waterfilling, with no special config generated"
        print "******************"
    args.routingScheme = 'waterfilling'

if(args.numPathChoices != 'default'):
    configname = configname + "_" + args.numPathChoices
else:   
    args.numPathChoices = '4'

f = open(args.ini_filename, "w+")
f.write("[General]\n\n")
f.write("[Config " +  configname + "]\n")
f.write("network = " + os.path.basename(args.network_name) + "\n")
f.write("**.topologyFile = \"" + args.topo_filename + "\"\n")
f.write("**.workloadFile = \"" + args.workload_filename + "\"\n\n")
f.write("**.simulationLength = " + args.simulationLength + "\n\n")
f.write("**.statRate = " + args.statRate + "\n\n")
f.write("**.signalsEnabled = " + args.signalsEnabled + "\n\n")
f.write("**.loggingEnabled = " + args.loggingEnabled + "\n\n")
f.write("**.timeoutClearRate = " + args.timeoutClearRate + "\n\n")
f.write("**.timeoutEnabled = " + args.timeoutEnabled + "\n\n")
f.write("**.numPathChoices = " + args.numPathChoices + "\n\n")
if(args.routingScheme == 'waterfilling'):
    f.write("**.waterfillingEnabled = true\n\n")
f.close()


