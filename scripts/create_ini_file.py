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
parser.add_argument('--routing-scheme', type=str, help='must be in [shortestPath, waterfilling, LP,' +
        'silentWhispers, smoothWaterfilling, priceScheme, landmarkRouting, lndBaseline],' +  
        'else will default to waterfilling', 
        dest='routingScheme', default='default')
parser.add_argument('--num-path-choices', type=str, help='number of path choices', dest='numPathChoices', default='default')
parser.add_argument('--demand-scale', type=int, help='how much has demand been scaled up by', dest='demandScale')
parser.add_argument('--transStatStart', type=int, help='when to start collecting stats from', default=3000)
parser.add_argument('--transStatEnd', type=int,help='when to end stats collection', default=5000)
parser.add_argument('--path-choice', type=str, help='type of path choices', dest='pathChoice', default='shortest',
        choices=['widest', 'oblivious', 'kspYen', 'shortest'])
parser.add_argument('--capacity', type=int,help='mean cap for topology')



# price based scheme parameters
parser.add_argument('--eta', type=float, help='step size for mu', dest='eta', default=0.01)
parser.add_argument('--kappa', type=float, help='step size for lambda', dest='kappa', default=0.01)
parser.add_argument('--alpha', type=float, help='step size for rate', dest='alpha', default=0.01)
parser.add_argument('--rho', type=float, help='nesterov or accelerated gradient parameter', dest='rho', default=0.05)
parser.add_argument('--capacityFactor', type=float, help='fraction of capacity used for lambda', dest='capacityFactor')
parser.add_argument('--update-query-time', type=float, help='time of update and query', \
        dest='updateQueryTime', default=0.8)
parser.add_argument('--zeta', type=float, help='memory factor with demand estimation', dest='zeta', default=0.01)
parser.add_argument('--xi', type=float, help='factor to weigh queue draining', dest='xi', default=1)
parser.add_argument('--router-queue-drain-time', type=float, help='time to drain queue (seconds)', 
        dest='routerQueueDrainTime', default=1)
parser.add_argument('--service-arrival-window', type=int, help='number of packets to track arrival/service', 
        dest='serviceArrivalWindow', default=100)
parser.add_argument('--min-rate', type=float, help='minimum rate when rate is too small in price scheme', \
        dest='minRate', default=0.25)

# smooth waterfilling arguments
parser.add_argument('--tau', type=float, help='time unit factor in smooth waterfillin', \
        dest='tau', default=10)
parser.add_argument('--normalizer', type=float, help='C in probability update', dest='normalizer', \
        default=100)

parser.add_argument('--rebalancing-enabled', action='store_true', dest="rebalancingEnabled")
parser.add_argument('--gamma', type=float, help='factor to weigh rebalancing', dest='gamma', default=1)
parser.add_argument('--rebalancing-queue-delay-threshold', type=float, help='threshold for rebalancing', 
        dest='rebalancingQueueDelayThreshold', default=3)
parser.add_argument('--gamma-imbalance-queue-size', type=float, help='threshold queue size for rebalancing', 
        dest='gammaImbalanceQueueSize', default=5)


args = parser.parse_args()



configname = os.path.basename(args.network_name)

if args.routingScheme != 'default':
    configname = configname + "_" + args.routingScheme
if args.demandScale != None:
    configname += "_demand" + str(args.demandScale)

print "in ini file, path choice", args.pathChoice, " capacity : ",args.capacity
configname += "_" + args.pathChoice
if args.capacity is not None:
    configname += '_cap' + str(args.capacity)

if args.rebalancingEnabled:
    configname += "_rebalancing_"
    configname += str(args.gamma)
    configname += "_" + str(args.gammaImabalanceQueueSize)
    configname += str(args.rebalancingQueueingDelayThreshold)

#arg parse might support a cleaner way to deal with this
if args.routingScheme not in ['shortestPath', 'waterfilling', 'priceScheme', 'silentWhispers', \
        'smoothWaterfilling', 'priceSchemeWindow', 'priceSchemeWindowNoQueue', 'landmarkRouting', 'lndBaseline'] :
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
if "NoQueue" in args.routingScheme:
    modifiedRoutingScheme = modifiedRoutingScheme[:-7]

if args.numPathChoices != 'default':
    configname = configname + "_" + args.numPathChoices
else:   
    args.numPathChoices = '4'

if args.capacityFactor != None:
    configname = configname + "_capacityFactorTimes10_" + str(int(float(args.capacityFactor)*10))

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
f.write("**.serviceArrivalWindow = " + str(args.serviceArrivalWindow) + "\n")
f.write("**.transStatStart = " + str(args.transStatStart) + "\n")
f.write("**.transStatEnd = " + str(args.transStatEnd) + "\n")

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
    f.write("**.xi = " + str(args.xi) + "\n")
    f.write("**.routerQueueDrainTime = " + str(args.routerQueueDrainTime) + "\n")
    if args.capacityFactor != None:
        f.write("**.capacityFactor = " + str(args.capacityFactor) + "\n")
    if 'NoQueue' in args.routingScheme: 
        f.write("**.useQueueEquation = false\n")

if args.routingScheme == "smoothWaterfilling":
    f.write("**.smoothWaterfillingEnabled = true\n")
    f.write("**.tau = " + str(args.tau) + "\n")
    f.write("**.normalizer = " + str(args.normalizer) + "\n")

if args.routingScheme == 'landmarkRouting':
    f.write("**.landmarkRoutingEnabled = true\n")

if args.routingScheme == 'lndBaseline':
    f.write("**.lndBaselineEnabled = true\n")

if args.pathChoice == "oblivious":
    f.write("**.obliviousRoutingEnabled = true\n")
elif args.pathChoice == "widest":
    f.write("**.widestPathsEnabled = true\n")
elif args.pathChoice == "kspYen":
    f.write("**.kspYenEnabled = true\n")


if args.rebalancingEnabled:
    f.write("**.rebalancingEnabled = true\n")
    if "priceScheme" in args.routingScheme:
        f.write("**.gamma = " + str(args.gamma) + "\n")
        f.write("**.gammaImbalanceQueueSize = " + str(args.gammaImbalanceQueueSize) + "\n")
    else:
        f.write("**.queueDelayThreshold = " + str(args.rebalancingQueueingDelayThreshold) + "\n")

f.close()


