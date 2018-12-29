#!/bin/sh
PATH="../benchmarks/"

num_nodes=("2" "3" "4" "5" "10" "0" "0" "40" "60" "80" "100" "200" "400" "600" "800" "1000" \
    "40" "60" "80" "100" "200" "400" "600" "800" "1000")

balance=100

prefix=("two_node" "three_node" "four_node" "five_node" \
    "hotnets" "lnd_dec4_2018" "lnd_dec28_2018" \
    "40_sw_routers" "60_sw_routers" "80_sw_routers"  \
    "100_sw_routers" "200_sw_routers" "400_sw_routers" "600_sw_routers" \
    "800_sw_routers" "1000_sw_routers"\
    "40_sf_routers" "60_sf_routers" "80_sf_routers"  \
    "100_sf_routers" "200_sf_routers" "400_sf_routers" "600_sf_routers" \
    "800_sf_routers" "1000_sf_routers")


arraylength=${#prefix[@]}
PYTHON="/usr/bin/python"

# generate the files
for (( i=0; i<${arraylength}; i++ ));
do 
    # generate the graph first to ned file
    workloadname="${prefix[i]}_circ"
    network="$PATH${workloadname}_net"
    topofile="$PATH${prefix[i]}_topo.txt"
    workload="$PATH$workloadname"
    inifile="$PATH$workloadname.ini"

    if [ $i -le 3 ]; then
        graph_type="simple_topologies"
    elif [ $i == 4 ]; then
        graph_type="hotnets_topo"
    elif [ $i == 5 ]; then
        graph_type="lnd_dec4_2018"
    elif [ $i == 6 ]; then
        graph_type="lnd_dec28_2018"
    elif [ $i -le 15 ]; then
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

