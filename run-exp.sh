#!/bin/sh
network=$1
topofile=$2
num_nodes=$3
balance=$4
workload=$5
inifile=$6

# generate the graph first to ned file
python scripts/create_topo_ned_file.py scale_free \
        --network-name $network\
        --topo-filename $topofile\
        --num-nodes $num_nodes\
        --balance-per-channel $balance


# create transactions corresponding to this experiment run
python scripts/create_workload.py $workload

# create the ini file
python scripts/create_ini_file.py \
        --network-name $network\
        --topo-filename $topofile\
        --workload-filename $workload\
        --ini-filename $inifile

# run the omnetexecutable with the right parameters
./spiderNet -u Cmdenv -f $inifile -c $network -n .

# cleanup?
