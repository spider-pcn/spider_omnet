#!/bin/bash
PATH_NAME="../benchmarks/circulations/"

num_nodes=("2" "2" "3" "4" "5" "5" "5" "0" "0" "10" "20" "40" "60" "80" "100" "200" "400" "600" "800" "1000" \
    "10" "40" "60" "80" "100" "200" "400" "600" "800" "1000" "40" "10" "20" "30" "40")

balance=100

prefix=("two_node_imbalance" "two_node_capacity" "three_node" "four_node" "five_node_hardcoded" \
    "hotnets" "five_line" "lnd_dec4_2018" "lnd_dec28_2018" \
    "sw_10_routers" "sw_20_routers" "sw_40_routers" "sw_60_routers" "sw_80_routers"  \
    "sw_100_routers" "sw_200_routers" "sw_400_routers" "sw_600_routers" \
    "sw_800_routers" "sw_1000_routers"\
    "sf_10_routers" \
    "sf_40_routers" "sf_60_routers" "sf_80_routers"  \
    "sf_100_routers" "sf_200_routers" "sf_400_routers" "sf_600_routers" \
    "sf_800_routers" "sf_1000_routers" "tree_40_routers" "random_10_routers" "random_20_routers"\
    "random_30_routers" "sw_sparse_40_routers")

demand_scale=("5" "10" "15" "20")

arraylength=${#prefix[@]}
PYTHON="/usr/bin/python"
mkdir -p ${PATH_NAME}

# TODO: find the indices in prefix of the topologies you want to run on and then specify them in array
# adjust experiment time as needed
#array=( 0 1 4 5 8 19 32)
array=( 6 )
for scale in "${demand_scale[@]}"
do
    for i in "${array[@]}"
    do 
        # generate the graph first to ned file
        workloadname="${prefix[i]}_circ_demand${scale}"
        network="${PATH_NAME}${workloadname}_net"
        topofile="${PATH_NAME}${prefix[i]}_topo.txt"
        workload="${PATH_NAME}$workloadname"
        inifile="${PATH_NAME}${workloadname}_default.ini"
        payment_graph_topo="custom"

        if [ ${prefix[i]:0:2} == "sw" ]; then
            graph_type="small_world"
        elif [ ${prefix[i]:0:2} == "sf" ]; then
            graph_type="scale_free"
        elif [ ${prefix[i]:0:4} == "tree" ]; then
            graph_type="tree"
        elif [ ${prefix[i]:0:3} == "lnd" ]; then
            graph_type=$prefix{i}
        elif [ ${prefix[i]} == "hotnets" ]; then
            graph_type="hotnets_topo"
        elif [ ${prefix[i]:0:6} == "random" ]; then
            graph_type="random"
        else
            graph_type="simple_topologies"
        fi
        
        if [ ${prefix[i]:0:3} == "two" ]; then
            delay="120"
        else
            delay="30"
        fi

        if [ ${prefix[i]:0:9} == "five_line" ]; then
            payment_graph_topo="simple_line"
        elif [ ${prefix[i]:0:4} == "five" ]; then
            payment_graph_topo="hardcoded_circ"
        elif [ ${prefix[i]:0:7} == "hotnets" ]; then
            payment_graph_topo="hotnets_topo"
        fi

        echo $network
        echo $topofile
        echo $inifile
        echo $graph_type

        
        $PYTHON create_topo_ned_file.py $graph_type\
                --network-name $network\
                --topo-filename $topofile\
                --num-nodes ${num_nodes[i]}\
                --balance-per-channel $balance\
                --separate-end-hosts \
                --delay-per-channel $delay\
                #--randomize-start-bal


        # create transactions corresponding to this experiment run
        $PYTHON create_workload.py $workload poisson \
                --graph-topo $payment_graph_topo \
                --payment-graph-dag-percentage 0\
                --topo-filename $topofile\
                --experiment-time 4000 \
                --balance-per-channel $balance\
                --generate-json-also \
                --timeout-value 5 \
                --scale-amount $scale

        # create the ini file
        $PYTHON create_ini_file.py \
                --network-name $network\
                --topo-filename ${topofile:3} \
                --workload-filename ${workload:3}_workload.txt\
                --ini-filename $inifile
    done
done

