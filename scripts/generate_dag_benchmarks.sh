#!/bin/bash
PATH_PREFIX="/home/ubuntu/omnetpp-5.4.1/samples/spider_omnet/benchmarks/"
GRAPH_PATH="/home/ubuntu/omnetpp-5.4.1/samples/spider_omnet/scripts/figures/"

num_nodes=("2" "2" "3" "4" "5" "5" "5" "0" "0" "10" "20" "50" "50" "60" "80" "30" "40" "0" "0" "50" "50" "0")


prefix=("two_node_imbalance" "two_node_capacity" "three_node" "four_node" "five_node_hardcoded" \
    "hotnets" "five_line" "lnd_dec4_2018" "lnd_dec4_2018lessScale" \
    "sw_10_routers" "sw_20_routers" "sw_50_routers" \
   "sf_50_routers" "sf_60_routers" "sf_80_routers"  \
    "random_30_routers" "sw_sparse_40_routers" "lnd_july15_2019" "lnd_uniform" \
    "sw_weird_combo" "sf_weird_combo", "lnd_weird_combo")

scale=3
random_init_bal=false
random_capacity=false
lnd_capacity=true


#general parameters that do not affect config names
simulationLength=2010
statCollectionRate=100
timeoutClearRate=1
timeoutEnabled=true
signalsEnabled=false
loggingEnabled=false

# scheme specific parameters
eta=0.025
alpha=0.2
kappa=0.025
updateQueryTime=1.5
minPriceRate=0.25
zeta=0.01
rho=0.04
tau=10
normalizer=100
xi=1
routerQueueDrainTime=5
serviceArrivalWindow=300



arraylength=${#prefix[@]}
PYTHON="/usr/bin/python"
mkdir -p ${PATH_PREFIX}

dag_percent=("45") # "20" "65")
balance=4000

# TODO: find the indices in prefix of the topologies you want to run on and then specify them in array
# adjust experiment time as needed
array=(20) 
for i in "${array[@]}" 
do    
    # create workload files and run different demand levels
    for dag_amt in "${dag_percent[@]}"
    do
        # generate the graph first to ned file
        PATH_NAME="${PATH_PREFIX}dag${dag_amt}/"
        mkdir -p ${PATH_NAME}
        for suffix in "Base" "Waterfilling" "LndBaseline" "PriceScheme" "DCTCP"
        do
            cp hostNode${suffix}.ned ${PATH_NAME}
            cp routerNode${suffix}.ned ${PATH_NAME}
        done
        cp hostNodeLandmarkRouting.ned ${PATH_NAME}
        cp hostNodePropFairPriceScheme.ned ${PATH_NAME}
        cp routerNodeDCTCPBal.ned ${PATH_NAME}
        
        if [ $random_capacity == "true" ]; then
            prefix_to_use="${prefix[i]}_randomCap"
            echo "random"
        elif [ $lnd_capacity == "true" ]; then
            prefix_to_use="${prefix[i]}_lndCap"
            echo "lnd"
        else
            prefix_to_use="${prefix[i]}"
        fi
        
        network="${prefix_to_use}_dag${dag_amt}_net"
        topofile="${PATH_NAME}${prefix_to_use}_topo${balance}.txt"

        # identify graph type for topology
        if [ ${prefix[i]:0:2} == "sw" ]; then
            graph_type="small_world"
        elif [ ${prefix[i]:0:2} == "sf" ]; then
            graph_type="scale_free"
        elif [ ${prefix[i]:0:4} == "tree" ]; then
            graph_type="tree"
        elif [ ${prefix[i]:0:11} == "lnd_uniform" ]; then
            graph_type="lnd_uniform"
        elif [ ${prefix[i]:0:3} == "lnd" ]; then
            graph_type="lnd_july15_2019"
        elif [ ${prefix[i]} == "hotnets" ]; then
            graph_type="hotnets_topo"
        elif [ ${prefix[i]:0:6} == "random" ]; then
            graph_type="random"
        else
            graph_type="simple_topologies"
        fi
        
        # set delay amount
        if [ ${prefix[i]:0:3} == "two" ]; then
            delay="120"
        else
            delay="30"
        fi
        
        # STEP 1: create topology
        $PYTHON scripts/create_topo_ned_file.py $graph_type\
                --network-name ${PATH_NAME}$network\
                --topo-filename $topofile\
                --num-nodes ${num_nodes[i]}\
                --balance-per-channel $balance\
                --separate-end-hosts \
                --delay-per-channel $delay\
                --randomize-start-bal $random_init_bal\
                --random-channel-capacity $random_capacity \
                --lnd-channel-capacity $lnd_capacity

        # identify graph type for topology
        if [ ${prefix_to_use:0:2} == "sw" ]; then
            graph_type="small_world"
        elif [ ${prefix_to_use:0:2} == "sf" ]; then
            graph_type="scale_free"
        elif [ ${prefix_to_use:0:4} == "tree" ]; then
            graph_type="tree"
        elif [ ${prefix_to_use:0:3} == "lnd" ]; then
            graph_type=${prefix_to_use}
        elif [ ${prefix_to_use} == "hotnets" ]; then
            graph_type="hotnets_topo"
        elif [ ${prefix_to_use:0:6} == "random" ]; then
            graph_type="random"
        else
            graph_type="simple_topologies"
        fi
        
        # set delay amount
        if [ ${prefix_to_use:0:3} == "two" ]; then
            delay="120"
        else
            delay="30"
        fi       

        echo $network
        echo $topofile
        echo $inifile
        echo $graph_type
    
        # create 5 workload files for 5 runs
        for num in #{0..4}
        do
            echo "generating dag ${num} for ${dag_amt}"
            workloadname="${prefix[i]}_demand${scale}_dag${dag_amt}_num${num}"
            workload="${PATH_NAME}$workloadname"
            inifile="${PATH_NAME}${workloadname}_default.ini"
            payment_graph_topo="custom"

            # STEP 2: create transactions corresponding to this experiment run
            $PYTHON scripts/create_workload.py $workload poisson \
                    --graph-topo $payment_graph_topo \
                    --payment-graph-dag-percentage ${dag_amt}\
                    --topo-filename $topofile\
                    --experiment-time $simulationLength \
                    --balance-list $balance\
                    --generate-json-also \
                    --timeout-value 5 \
                    --kaggle-size \
                    --scale-amount $scale \
                    --run-num ${num}
        done
    done
    #rm $topofile
done

