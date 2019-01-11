#!/bin/bash
PATH_NAME="benchmarks/dag25/"

prefix=("sw_40_routers" "sf_40_routers"\
    "sw_400_routers" "sf_400_routers")
    #"sw_1000_routers" "sf_1000_routers")

arraylength=${#prefix[@]}

cp hostNode.ned ${PATH_NAME}
cp routerNode.ned ${PATH_NAME}

for (( i=0; i<${arraylength}; i++));
do 
    inifile=${PATH_NAME}${prefix[i]}_dag25.ini
    network=${prefix[i]}_dag25_net

    # run the omnetexecutable with the right parameters
    ./spiderNet -u Cmdenv -f $inifile -c $network -n ${PATH_NAME}
done

