#!/bin/bash
PATH_NAME="benchmarks/circulations/"

prefix=("five_node_hardcoded") 
 #"two_node_imbalance" "two_node_capacity" ) #"sw_sparse_40_routers") # "sw_40_routers" "sf_40_routers")
    #"sw_400_routers" "sf_400_routers")
    #"sw_1000_routers" "sf_1000_routers")

arraylength=${#prefix[@]}

#general parameters that do not affect config names
simulationLength=25.0
statCollectionRate=1
timeoutClearRate=1
timeoutEnabled=true
signalsEnabled=true
loggingEnabled=true
routing_scheme_list=("smoothWaterfilling" "waterfilling")

eta=0.02
alpha=0.1
kappa=0.02
updateQueryTime=1.5
minPriceRate=0.25
zeta=0.01
rho=0.05

tau=10
normalizer=100

cp hostNodeBase.ned ${PATH_NAME}
cp hostNodeWaterfilling.ned ${PATH_NAME}
cp routerNode.ned ${PATH_NAME}

for (( i=0; i<${arraylength}; i++));
do 
    workloadname="${prefix[i]}_circ"
    topofile="${PATH_NAME}${prefix[i]}_topo.txt"
    workload="${PATH_NAME}$workloadname"
    network=${prefix[i]}_circ_net

#    #routing schemes where number of path choices doesn't matter
    for routing_scheme in shortestPath #silentWhispers
    do
        if [[ " ${routing_scheme_list[*]} " == *"$routing_scheme"* ]]; then
          inifile=${PATH_NAME}${prefix[i]}_circ_${routing_scheme}.ini

          # create the ini file with specified parameters
          python scripts/create_ini_file.py \
                  --network-name ${network}\
                  --topo-filename ${topofile}\
                  --workload-filename ${workload}_workload.txt\
                  --ini-filename $inifile\
                  --signals-enabled $signalsEnabled\
                  --logging-enabled $loggingEnabled\
                  --simulation-length $simulationLength\
                  --stat-collection-rate $statCollectionRate\
                  --timeout-clear-rate $timeoutClearRate\
                  --timeout-enabled $timeoutEnabled\
                  --routing-scheme ${routing_scheme}


          # run the omnetexecutable with the right parameters
          ./spiderNet -u Cmdenv -f $inifile -c ${network}_${routing_scheme} -n ${PATH_NAME}
      fi
    done

  #routing schemes where number of path choices matter
    for routing_scheme in waterfilling smoothWaterfilling priceScheme
    do
      pids=""
      # if you add more choices for the number of paths you might run out of cores/memory
      for numPathChoices in 4
      do
        if [[ " ${routing_scheme_list[*]} " == *"$routing_scheme"* ]]; then

            output_file=outputs/${prefix[i]}_circ_${routing_scheme}
            inifile=${PATH_NAME}${prefix[i]}_circ_${routing_scheme}.ini

            # create the ini file with specified parameters
            python scripts/create_ini_file.py \
                    --network-name ${network}\
                    --topo-filename ${topofile}\
                    --workload-filename ${workload}_workload.txt\
                    --ini-filename ${inifile}\
                    --signals-enabled $signalsEnabled\
                    --logging-enabled $loggingEnabled\
                    --simulation-length $simulationLength\
                    --stat-collection-rate $statCollectionRate\
                    --timeout-clear-rate $timeoutClearRate\
                    --timeout-enabled $timeoutEnabled\
                    --routing-scheme ${routing_scheme}\
                    --num-path-choices ${numPathChoices}\
                    --zeta $zeta\
                    --alpha $alpha\
                    --eta $eta\
                    --kappa $kappa\
                    --rho $rho\
                    --update-query-time $updateQueryTime\
                    --min-rate $minPriceRate\
                    --tau $tau\
                    --normalizer $normalizer


            # run the omnetexecutable with the right parameters
            # in the background
            ./spiderNet -u Cmdenv -f ${inifile}\
                -c ${network}_${routing_scheme}_${numPathChoices} -n ${PATH_NAME}\
                > $output_file 
            pids+=($!)i
        fi
      done 
    done
done
wait
