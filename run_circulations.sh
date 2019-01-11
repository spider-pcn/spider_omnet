#!/bin/bash
PATH="benchmarks/circulations/"

prefix=("sw_40_routers" "sf_40_routers"\
    "sw_400_routers" "sf_400_routers")
    #"sw_1000_routers" "sf_1000_routers")

arraylength=${#prefix[@]}

cp hostNode.ned $PATH
cp routerNode.ned $PATH

for (( i=0; i<${arraylength}; i++));
do 
    inifile=$PATH${prefix[i]}_circ.ini
    network=${prefix[i]}_circ_net

    # run the omnetexecutable with the right parameters
    ./spiderNet -u Cmdenv -f $inifile -c $network -n $PATH
done

