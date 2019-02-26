import argparse
import os

# parse arguments
parser = argparse.ArgumentParser(description="Create ini file for the given arguments")
parser.add_argument('--workload-filename', type=str, dest='workload_filename', default='sample_workload.txt')
parser.add_argument('--topo-filename', type=str, help='name of intermediate output file', default='topo.txt')
parser.add_argument('--network-name', type=str, help='name of the output ned filename', default='simpleNet')
parser.add_argument('--ini-filename', type=str, help='name of ini file', default='omnetpp.ini')
parser.add_argument('--simulation-length', type=str, help='duration of simulation in seconds', dest='simulationLength', default='30.0')
parser.add_argument('--stat-collection-rate', type=str, help='rate of collecting stats', dest='statRate', \
        default='0.2')
parser.add_argument('--signals-enabled', type=str, help='are signals enabled?', dest='signalsEnabled', default='false')
parser.add_argument('--logging-enabled', type=str, help='is logging enabled?', dest='loggingEnabled', default='false')
parser.add_argument('--window-enabled', type=str, help='is window enabled?', dest='windowEnabled', default='false')
parser.add_argument('--timeout-clear-rate', type=str, help='rate of clearing data after timeouts', dest='timeoutClearRate', default='0.5')
parser.add_argument('--timeout-enabled', type=str, help='are timeouts enabled?', dest='timeoutEnabled', default='true')
parser.add_argument('--routing-scheme', type=str, help='must be in [shortestPath, waterfilling, LP, silentWhispers, smoothWaterfilling, priceScheme], else will default to waterfilling', dest='routingScheme', default='default')
parser.add_argument('--num-path-choices', type=str, help='number of path choices', dest='numPathChoices', default='default')

# price based scheme parameters
parser.add_argument('--eta', type=float, help='step size for mu', dest='eta', default=0.01)
parser.add_argument('--kappa', type=float, help='step size for lambda', dest='kappa', default=0.01)
parser.add_argument('--alpha', type=float, help='step size for rate', dest='alpha', default=0.01)
parser.add_argument('--rho', type=float, help='nesterov or accelerated gradient parameter', dest='rho', default=0.05)
parser.add_argument('--update-query-time', type=float, help='time of update and query', \
        dest='updateQueryTime', default=0.8)

parser.add_argument('--zeta', type=float, help='ewma factor for demand', dest='zeta', default=0.01)
parser.add_argument('--min-rate', type=float, help='minimum rate when rate is too small in price scheme', \
        dest='minRate', default=0.25)

# smooth waterfilling arguments
parser.add_argument('--tau', type=float, help='time unit factor in smooth waterfillin', \
        dest='tau', default=10)
parser.add_argument('--normalizer', type=float, help='C in probability update', dest='normalizer', \
        default=100)


args = parser.parse_args()



configname = os.path.basename(args.network_name)

if args.routingScheme != 'default':
    configname = configname + "_" + args.routingScheme

#arg parse might support a cleaner way to deal with this
if args.routingScheme not in ['shortestPath', 'waterfilling', 'priceScheme', 'silentWhispers', \
        'smoothWaterfilling', 'priceSchemeWindow'] :
    if args.routingScheme != 'default':
        print "******************"
        print "WARNING: ill-specified routing scheme, defaulting to waterfilling,",\
            "with no special config generated"
        print "******************"
    args.routingScheme = 'waterfilling'

if args.routingScheme == 'smoothWaterfilling':
    modifiedRoutingScheme = 'waterfilling'
elif "Window" in args.routingScheme:
    modifiedRoutingScheme = args.routingScheme[:-6]
else:
    modifiedRoutingScheme = args.routingScheme

if args.numPathChoices != 'default':
    configname = configname + "_" + args.numPathChoices
else:   
    args.numPathChoices = '4'

f = open(args.ini_filename, "w+")
f.write("[General]\n\n")
f.write("[Config " +  configname + "]\n")
f.write("network = " + os.path.basename(args.network_name) + "_" + modifiedRoutingScheme + "\n")
f.write("**.topologyFile = \"" + args.topo_filename + "\"\n")
f.write("**.workloadFile = \"" + args.workload_filename + "\"\n")
f.write("**.simulationLength = " + args.simulationLength + "\n")
f.write("**.statRate = " + args.statRate + "\n")
f.write("**.signalsEnabled = " + args.signalsEnabled + "\n")
f.write("**.loggingEnabled = " + args.loggingEnabled + "\n")
f.write("**.windowEnabled = " + args.windowEnabled + "\n")
f.write("**.timeoutClearRate = " + args.timeoutClearRate + "\n")
f.write("**.timeoutEnabled = " + args.timeoutEnabled + "\n")
f.write("**.numPathChoices = " + args.numPathChoices + "\n")


if args.routingScheme in ['waterfilling', 'smoothWaterfilling']:
    f.write("**.waterfillingEnabled = true\n")
if 'priceScheme' in args.routingScheme:
    f.write("**.priceSchemeEnabled = true\n")
    f.write("**.eta = " + str(args.eta) + "\n")
    f.write("**.kappa = " + str(args.kappa) + "\n")
    f.write("**.alpha = " + str(args.alpha) + "\n")
    f.write("**.zeta = " + str(args.zeta) + "\n")
    f.write("**.updateQueryTime = " + str(args.updateQueryTime) + "\n")
    f.write("**.minRate = " + str(args.minRate) + "\n")
    f.write("**.rhoValue = " + str(args.rho) + "\n")


if args.routingScheme == "smoothWaterfilling":
    f.write("**.smoothWaterfillingEnabled = true\n")
    f.write("**.tau = " + str(args.tau) + "\n")
    f.write("**.normalizer = " + str(args.normalizer) + "\n")

f.close()


