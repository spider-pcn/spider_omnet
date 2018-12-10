import sys
import textwrap

def generate_workload_custom(filename, payment_graph_topo, workload_type, total_time, exp_size, txn_size_mean):
    # define start and end nodes and amounts
    # edge a->b in payment graph appears in index i as start_nodes[i]=a, and end_nodes[i]=b
    if payment_graph_topo == 'hotnets_topo':
        start_nodes = [0,3,0,1,2,3,2,4]
        end_nodes = [4,0,1,3,1,2,4,2]
        amt_relative = [1,2,1,2,1,2,2,1]
        amt_absolute = [50*x for x in amt_relative]

    elif payment_graph_topo == 'simple_deadlock':
        start_nodes = [1,0,2]
        end_nodes = [2,2,0]
        amt_relative = [2,1,2]
        amt_absolute = [50*x for x in amt_relative]

    elif payment_graph_topo == 'simple_line':
        start_nodes = [0,3]
        end_nodes = [3,0]
        amt_relative = [1,1]
        amt_absolute = [400*x for x in amt_relative]


    # workload file of form
    # [amount] [timeSent] [sender] [receiver] [priorityClass]
    # write to file - assume no priority for now
    # transaction sizes are either constant or exponentially distributed around their mean
    outfile = open(filename, "w")

    if distirbution == 'uniform':
        # constant transaction size generated at uniform intervals
        for i in range(total_time):
            for k in range(len(start_nodes)):
                for l in range(amt_absolute[k]):
                    rate = amt_absolute[k]
                    time_start = i*1.0 + (l*1.0) / (1.0*rate)
                    txn_size = np.random_exponential(txn_size_mean) if exp_size else txn_size_mean
                    outfile.write(txn_size + " " + str(time_start) + " " + str(start_nodes[k]) + " " +\
                            str(end_nodes[k]) + " 0\n")

    elif distribution == 'poisson_process':
        # constant transaction size to be sent in a poisson fashion
        for i in range(total_time):
            for k in range(len(start_nodes)):
                current_time = 0.0
                rate = amt_absolute[k]*1.0
                beta = (1.0) / (1.0 * rate)
                # if the rate is higher, given pair will have more transactions in a single second
                while (current_time < 1.0):
                    time_start = i*1.0 + current_time
                    txn_size = np.random_exponential(txn_size_mean) if exp_size else txn_size_mean
                    outfile.write(txn_size + " " + str(time_start) + " " + str(start_nodes[k]) + " " +\
                            str(end_nodes[k]) + " 0\n")
                    time_incr = np.random.exponential(beta)
                    current_time = current_time + time_incr 

    outfile.close()

# TODO: generate workload for arbitrary topology


# parse arguments
parser = argparse.ArgumentParser(description="Create arbitrary txn workloads to run the omnet simulator on")
parser.add_argument('--payment-graph-type', choices=['hotnets_topo', 'simple_line', 'simple_deadlock'],\
        help='type of graph (Small world or scale free or custom topology)', default='simple_line')
parser.add_argument('--topo-filename', dest='topo_filename', type=str, \
        help='name of topology file to generate worklooad for', \
        default="topo.txt")
parser.add_argument('output_filename', type=str, help='name of the output workload file', \
        default='simple_workload.txt')
parser.add_argument('--distribution', dest='distribution', choices=['uniform', 'poisson'],\
        help='time between transactions is determine by this', default='poisson')
parser.add_argument('--experiment-time', dest='total_time', type=int, \
        help='time to generate txns for', default=30)
parser.add_argument('--txn-size-mean', dest='txn_size_mean', type=int, \
        help='mean_txn_size', default=1)
parser.add_argument('--exp_size', action='store_true', help='should txns be exponential in size')

args = parser.parse_args()

output_filename = args.output_filename
payment_graph_type = args.payment_graph_type
distribution = args.distribution
total_time = args.total_time
txn_size_mean = args.mean_txn_size
exp_size = args.exp_size



# generate workloads
if workload_type == 'custom':
    generate_workload_custom(output_filename, payment_graph_type, distribution, \
            total_time, exp_size, txn_size_mean)
# else







    
