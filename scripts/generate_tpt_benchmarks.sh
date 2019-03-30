#!/bin/bash
PATH_NAME="/home/ubuntu/omnetpp-5.4.1/samples/spider_omnet/benchmarks/circulations/"


demand_scale=("10" "20" "40" "80" "160") # "60" "90")
capacity_list=("20" "40" "80" "160" "320")
random_init_bal=false
random_capacity=false
simulationLength=4100


cp hostNodeBase.ned ${PATH_NAME}
cp hostNodeWaterfilling.ned ${PATH_NAME}
cp hostNodeLandmarkRouting.ned ${PATH_NAME}
cp hostNodePriceScheme.ned ${PATH_NAME}
cp hostNodeLndBaseline.ned ${PATH_NAME}
cp routerNode.ned ${PATH_NAME}

arraylength=${#prefix[@]}
PYTHON="/usr/bin/python"
mkdir -p ${PATH_NAME}


prefix="lnd_uniform"
length=${#capacity_list[@]}

for (( i=0; i<$length; i++ ));
do
    capacity=${capacity_list[i]}
    scale=${demand_scale[i]}
    network="${prefix}_cap${capacity}_circ_net"
    topofile="${PATH_NAME}${prefix}_cap${capacity}_topo.txt"
    graph_type="lnd_uniform"
    delay="30"

    echo $capacity
    echo $scale
    
    # STEP 1: create topology
    $PYTHON scripts/create_topo_ned_file.py $graph_type\
            --network-name ${PATH_NAME}$network\
            --topo-filename $topofile\
            --num-nodes 102\
            --balance-per-channel $capacity\
            --separate-end-hosts \
            --delay-per-channel $delay\
            --randomize-start-bal $random_init_bal\
            --random-channel-capacity $random_capacity


    # create workload files and run different demand levels
    workloadname="${prefix}_circ_demand${scale}"
    workload="${PATH_NAME}$workloadname"
    inifile="${PATH_NAME}${workloadname}_default.ini"
    payment_graph_topo="custom"
        


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
                --balance-per-channel $capacity\
                --timeout-value 5 \
                --scale-amount $scale 
done

