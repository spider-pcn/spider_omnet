#!/bin/bash
PATH_NAME="/home/ubuntu/omnetpp-5.4.1/samples/spider_omnet/benchmarks/circulations/"
GRAPH_PATH="/home/ubuntu/omnetpp-5.4.1/samples/spider_omnet/scripts/figures/"

num_nodes=("2" "2" "3" "4" "5" "5" "5" "0" "0" "10" "20" "50" "60" "80" "100" "200" "400" "600" "800" "1000" \
    "10" "20" "50" "60" "80" "100" "200" "400" "600" "800" "1000" "40" "10" "20" "30" "40" "0" "0" "0" "4")


prefix=("two_node_imbalance" "two_node_capacity" "three_node" "four_node" "five_node_hardcoded" \
    "hotnets" "five_line" "lnd_dec4_2018" "lnd_dec4_2018lessScale" \
    "sw_10_routers" "sw_20_routers" "sw_50_routers" "sw_60_routers" "sw_80_routers"  \
    "sw_100_routers" "sw_200_routers" "sw_400_routers" "sw_600_routers" \
    "sw_800_routers" "sw_1000_routers"\
    "sf_10_routers" "sf_20_routers" \
    "sf_50_routers" "sf_60_routers" "sf_80_routers"  \
    "sf_100_routers" "sf_200_routers" "sf_400_routers" "sf_600_routers" \
    "sf_800_routers" "sf_1000_routers" "tree_40_routers" "random_10_routers" "random_20_routers"\
    "random_30_routers" "sw_sparse_40_routers" "lnd_gaussian" "lnd_uniform" "lnd_july15_2019"\
    "parallel_graphs")


demand_scale=("3") # 10 for implementation comparison 
random_init_bal=false
random_capacity=false
lnd_capacity=false


#general parameters that do not affect config names
simulationLength=5100
statCollectionRate=100
timeoutClearRate=1
timeoutEnabled=true
signalsEnabled=true
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


mkdir -p ${PATH_NAME}
for suffix in "Base" "Waterfilling" "LndBaseline" "PriceScheme" "DCTCP"
do
    cp hostNode${suffix}.ned ${PATH_NAME}
    cp routerNode${suffix}.ned ${PATH_NAME}
done
cp hostNodeLandmarkRouting.ned ${PATH_NAME}
cp hostNodePropFairPriceScheme.ned ${PATH_NAME}
cp routerNodeDCTCPBal.ned ${PATH_NAME}

arraylength=${#prefix[@]}
PYTHON="/usr/bin/python"
mkdir -p ${PATH_NAME}

# TODO: find the indices in prefix of the topologies you want to run on and then specify them in array
# adjust experiment time as needed
array=(20) 
for i in "${array[@]}" 
do
    for balance in 100 200 400 # 50 100 200 400      
        # 900 1350 2750 4000 8750
    do
        if [ $random_capacity == "true" ]; then
            prefix_to_use="${prefix[i]}_randomCap"
            echo "random"
        elif [ $lnd_capacity == "true" ]; then
            prefix_to_use="${prefix[i]}_lndCap"
            echo "lnd"
        else
            prefix_to_use="${prefix[i]}"
        fi

        network="${prefix_to_use}_circ_net"
        topofile="${PATH_NAME}${prefix_to_use}_topo${balance}.txt"

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
        elif [ ${prefix_to_use:0:8} == "parallel" ]; then
            graph_type="parallel_graph"
        else
            graph_type="simple_topologies"
        fi
        
        # set delay amount
        if [ ${prefix_to_use:0:3} == "two" ]; then
            delay="120"
        else
            # delay="150" for implementation comparison
            delay="30"
        fi
        
        # STEP 1: create topology
        $PYTHON scripts/create_topo_ned_file.py $graph_type\
                --network-name ${PATH_NAME}$network\
                --topo-filename $topofile\
                --num-nodes ${num_nodes[i]}\
                --balance-per-channel $balance\
                --separate-end-hosts \
                --delay-per-channel $delay \
                --randomize-start-bal $random_init_bal\
                --random-channel-capacity $random_capacity\
                --lnd-channel-capacity $lnd_capacity
    done

    # create 5 workload files for 5 runs
    for num in {0..4}
    do

        # create workload files and run different demand levels
        for scale in "${demand_scale[@]}"
        do

            # generate the graph first to ned file
            workloadname="${prefix[i]}_circ${num}_demand${scale}"
            workload="${PATH_NAME}$workloadname"
            inifile="${PATH_NAME}${workloadname}_default.ini"
            payment_graph_topo="custom"
            
            # figure out payment graph/workload topology
            if [ ${prefix_to_use:0:9} == "five_line" ]; then
                payment_graph_topo="simple_line"
            elif [ ${prefix_to_use:0:4} == "five" ]; then
                payment_graph_topo="hardcoded_circ"
            elif [ ${prefix_to_use:0:7} == "hotnets" ]; then
                payment_graph_topo="hotnets_topo"
            fi

            echo $network
            echo $topofile
            echo $inifile
            echo $graph_type

            # STEP 2: create transactions corresponding to this experiment run
            $PYTHON scripts/create_workload.py $workload poisson \
                    --graph-topo $payment_graph_topo \
                    --payment-graph-dag-percentage 0\
                    --topo-filename $topofile\
                    --experiment-time $simulationLength \
                    --balance-list $balance \
                    --generate-json-also \
                    --timeout-value 5 \
                    --scale-amount $scale\
                    --kaggle-size \
                    --run-num ${num}
        done
    done
done
