#!/bin/sh
PATH="../benchmarks/"

toponame=("two_nodes_topo.txt" "three_nodes_topo.txt" "four_nodes_topo.txt" "five_nodes_topo.txt"\
    "hotnets_topo.txt" "40_sw_routers_topo.txt" "60_sw_routers_topo.txt" \
    "80_sw_routers_topo.txt" "100_sw_routers_topo.txt"\
    "200_sw_routers_topo.txt" "400_sw_routers_topo.txt" "600_sw_routers_topo.txt" \
    "800_sw_routers_topo.txt" "1000_sw_routers_topo.txt"\
    "40_sf_routers_topo.txt" "60_sf_routers_topo.txt" \
    "80_sf_routers_topo.txt" "100_sf_routers_topo.txt"\
    "200_sf_routers_topo.txt" "400_sf_routers_topo.txt" "600_sf_routers_topo.txt"\
    "800_sf_routers_topo.txt" "1000_sf_routers_topo.txt")

num_nodes=("2" "3" "4" "5" "10" "40" "60" "80" "100" "200" "400" "600" "800" "1000" \
    "40" "60" "80" "100" "200" "400" "600" "800" "1000")

balance=100

workloadname=("two_node_circ" "three_node_circ" "four_node_circ" "five_node_circ" \
    "hotnets_circ" "40_sw_routers_circ" "60_sw_routers_circ" "80_sw_routers_circ"  \
    "100_sw_routers_circ" "200_sw_routers_circ" "400_sw_routers_circ" "600_sw_routers_circ" \
    "800_sw_routers_circ" "1000_sw_routers_circ"\
    "40_sf_routers_circ" "60_sf_routers_circ" "80_sf_routers_circ"  \
    "100_sf_routers_circ" "200_sf_routers_circ" "400_sf_routers_circ" "600_sf_routers_circ" \
    "800_sf_routers_circ" "1000_sf_routers_circ")


arraylength=${#workloadname[@]}
PYTHON="/usr/bin/python"

# generate the files
for (( i=0; i<${arraylength}; i++ ));
do 
    # generate the graph first to ned file
    network="$PATH${workloadname[i]}_net"
    topofile="$PATH${toponame[i]}"
    workload="$PATH${workloadname[i]}"
    inifile="$PATH${workloadname[i]}.ini"

    if [ $i -le 3 ]; then
        graph_type="simple_topologies"
    elif [ $i == 4 ]; then
        graph_type="hotnets_topo"
    elif [ $i -le 13 ]; then
        graph_type="small_world"
    else
        graph_type="scale_free"
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
            --separate-end-hosts


    # create transactions corresponding to this experiment run
    $PYTHON create_workload.py $workload uniform\
            --graph-topo custom\
            --payment-graph-type circulation\
            --topo-filename $topofile\
            --experiment-time 30\
            --balance-per-channel $balance\
            --generate-json-also\

    # create the ini file
    $PYTHON create_ini_file.py \
            --network-name $network\
            --topo-filename $topofile\
            --workload-filename $workload\
            --ini-filename $inifile
done 

