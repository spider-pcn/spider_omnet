#!/bin/bash
PATH_NAME=$HOME"/"${OMNET}"/samples/spider_omnet/benchmarks/circulations/"
GRAPH_PATH=$HOME"/"${OMNET}"/samples/spider_omnet/scripts/figures/"
DEFAULT_PATH=$HOME"/"${OMNET}"/samples/spider_omnet"

for suffix in "Base" "Waterfilling" "LndBaseline" "PriceScheme" "DCTCP" "Celer"
do
    cp ${DEFAULT_PATH}/hostNode${suffix}.ned ${PATH_NAME}
    cp ${DEFAULT_PATH}/routerNode${suffix}.ned ${PATH_NAME}
done
cp ${DEFAULT_PATH}/hostNodeLandmarkRouting.ned ${PATH_NAME}
cp ${DEFAULT_PATH}/routerNodeDCTCPBal.ned ${PATH_NAME}

mkdir -p ${PATH_NAME}
