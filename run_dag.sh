#!/bin/bash
PATH_PREFIX="benchmarks/"

prefix=("sw_40_routers" "sf_40_routers")
    #"sw_400_routers" "sf_400_routers")
    #"sw_1000_routers" "sf_1000_routers")

arraylength=${#prefix[@]}
dag_percentage=("1" "5" "25")

#general parameters that do not affect config names
simulationLength=30.0
statCollectionRate=0.2
timeoutClearRate=0.5
timeoutEnabled=true
signalsEnabled=false
loggingEnabled=false


for (( i=0; i<${arraylength}; i++));
do 
    for p in "${dag_percentage[@]}";
    do
        PATH_NAME=${PATH_PREFIX}dag$p/
        cp hostNode.ned ${PATH_NAME}
        cp routerNode.ned ${PATH_NAME}


        inifile=${PATH_NAME}${prefix[i]}_dag${p}.ini
        network=${prefix[i]}_dag${p}_net
        workloadname="${prefix[i]}_dag$p"
        topofile="${PATH_NAME}${prefix[i]}_topo.txt"
        workload="${PATH_NAME}$workloadname"

        #routing schemes where number of path choices doesn't matter
        for routing_scheme in shortestPath #silentWhispers
        do
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
        done

        #routing schemes where number of path choices matter
        for routing_scheme in waterfilling #LP
        do
          for numPathChoices in 4
          do
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
                    --num-path-choices ${numPathChoices}


            # run the omnetexecutable with the right parameters
            ./spiderNet -u Cmdenv -f $inifile -c ${network}_${routing_scheme}_${numPathChoices} -n ${PATH_NAME}
          done
      done
    done
done
