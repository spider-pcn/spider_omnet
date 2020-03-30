#!/bin/bash
PATH_PREFIX=$HOME"/"${OMNET}"/samples/spider_omnet/benchmarks/"
CIRC_PATH_NAME=${PATH_PREFIX}/"circulations/"
GRAPH_PATH=$HOME"/"${OMNET}"/samples/spider_omnet/scripts/figures/"
DEFAULT_PATH=$HOME"/"${OMNET}"/samples/spider_omnet"

function setup {
    path_name=$1
    mkdir -p ${path_name}
    for suffix in "Base" "Waterfilling" "LndBaseline" "PriceScheme" "DCTCP" "Celer"
    do
        cp ${DEFAULT_PATH}/hostNode${suffix}.ned ${path_name}
        cp ${DEFAULT_PATH}/routerNode${suffix}.ned ${path_name}
    done
    cp ${DEFAULT_PATH}/hostNodeLandmarkRouting.ned ${path_name}
    cp ${DEFAULT_PATH}/routerNodeDCTCPBal.ned ${path_name}
}

