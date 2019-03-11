#!/bin/bash
PATH_PREFIX="../benchmarks/"

num_nodes=("2" "2" "3" "4" "5" "10" "0" "0" "10" "20" "40" "60" "80" "100" "200" "400" "600" "800" "1000" "20"\
    "40" "60" "80" "100" "200" "400" "600" "800" "1000" "40" "10" "20" "30" "40")

balance=100


prefix=("two_node_imbalance" "two_node_capacity" "three_node" "four_node" "five_node_hardcoded" \
    "hotnets" "lnd_dec4_2018" "lnd_dec28_2018" \
    "sw_10_routers" "sw_20_routers" "sw_40_routers" "sw_60_routers" "sw_80_routers"  \
    "sw_100_routers" "sw_200_routers" "sw_400_routers" "sw_600_routers" \
    "sw_800_routers" "sw_1000_routers" "sf_20_routers"\
    "sf_40_routers" "sf_60_routers" "sf_80_routers"  \
    "sf_100_routers" "sf_200_routers" "sf_400_routers" "sf_600_routers" \
    "sf_800_routers" "sf_1000_routers" "tree_40_routers" "random_10_routers" "random_20_routers"\
    "random_30_routers" "sw_sparse_40_routers")

dag_percent=("0" "5" "25")

arraylength=${#prefix[@]}
PYTHON="/usr/bin/python"


# TODO: find the indices in prefix of the topologies you want to run on and then specify them in array
# adjust experiment time as needed
array=( 10 20 )
for i in "${array[@]}";
do 
    for (( j=0; j<${#dag_percent[@]}; j++ ));
    do
        # make the directory if it doesn't exist
        path="${PATH_PREFIX}dag${dag_percent[j]}/"
        mkdir -p $path

        # generate the graph first to ned file
        workloadname="${prefix[i]}_dag${dag_percent[j]}"
        network="$path${workloadname}_net"
        topofile="$path${prefix[i]}_topo.txt"
        workload="$path$workloadname"
        inifile="$path${workloadname}_default.ini"
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


        # create transactions corresponding to this experiment run
        $PYTHON create_workload.py $workload poisson\
                --graph-topo $payment_graph_topo \
                --payment-graph-dag-percentage ${dag_percent[j]} \
                --topo-filename $topofile\
                --experiment-time 1000\
                --balance-per-channel $balance\
                --generate-json-also\
                --timeout-value 5

        # create the ini file
        $PYTHON create_ini_file.py \
                --network-name $network\
                --topo-filename ${topofile:3}\
                --workload-filename ${workload:3}_workload.txt\
                --ini-filename $inifile
    done 
done
