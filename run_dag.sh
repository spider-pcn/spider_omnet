#!/bin/bash
PATH_PREFIX="benchmarks/"

prefix=("sw_40_routers" "sf_40_routers")
    #"sw_400_routers" "sf_400_routers")
    #"sw_1000_routers" "sf_1000_routers")

arraylength=${#prefix[@]}
dag_percentage=("0" "5" "25")

#general parameters that do not affect config names
simulationLength=1000
statCollectionRate=25
timeoutClearRate=1
timeoutEnabled=true
signalsEnabled=true
loggingEnabled=false
# routing_scheme_list=("smoothWaterfilling" "waterfilling" "shortestPath")
routing_scheme_list=("shortestPath")


eta=0.02
alpha=0.05
kappa=0.02
updateQueryTime=0.8
minPriceRate=0.25
zeta=0.01
rho=0.05

tau=10
normalizer=100

for (( i=0; i<${arraylength}; i++));
do 
    for p in "${dag_percentage[@]}";
    do
        PATH_NAME=${PATH_PREFIX}dag$p/
        cp hostNode.ned ${PATH_NAME}
        cp routerNode.ned ${PATH_NAME}


        network=${prefix[i]}_dag${p}_net
        workloadname="${prefix[i]}_dag$p"
        topofile="${PATH_NAME}${prefix[i]}_topo.txt"
        workload="${PATH_NAME}$workloadname"

        #routing schemes where number of path choices doesn't matter
        for routing_scheme in shortestPath #silentWhispers
        do
         if [[ " ${routing_scheme_list[*]} " == *"$routing_scheme"* ]]; then
            inifile=${PATH_NAME}${prefix[i]}_dag${p}_${routing_scheme}.ini

              # create the ini file with specified parameters
              python scripts/create_ini_file.py \
                      --network-name $network\
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
          for numPathChoices in 4
          do
            if [[ " ${routing_scheme_list[*]} " == *"$routing_scheme"* ]]; then
                output_file=outputs/${prefix[i]}_dag${p}_${routing_scheme}
                inifile=${PATH_NAME}${prefix[i]}_dag${p}_${routing_scheme}.ini

                # create the ini file with specified parameters
                python scripts/create_ini_file.py \
                        --network-name $network\
                        --topo-filename ${topofile}\
                        --workload-filename ${workload}_workload.txt\
                        --ini-filename $inifile\
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
                ./spiderNet -u Cmdenv -f $inifile \
                    -c ${network}_${routing_scheme}_${numPathChoices} -n ${PATH_NAME} \
                    > $output_file &
            fi
          done
      done
    done
done
wait
