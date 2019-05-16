#!/bin/bash
PATH_NAME="benchmarks/circulations/"

prefix=( "hotnets" ) #five_node_hardcoded" )
#"two_node_imbalance" "two_node_capacity" ) #"sw_sparse_40_routers") # "sw_40_routers" "sf_40_routers")
    #"sw_400_routers" "sf_400_routers")
    #"sw_1000_routers" "sf_1000_routers")

arraylength=${#prefix[@]}

#general parameters that do not affect config names
simulationLength=5000
statCollectionRate=25
timeoutClearRate=1
timeoutEnabled=true
signalsEnabled=true
loggingEnabled=false
#path_choices_dep_list=( "priceSchemeWindow" "waterfilling" "smoothWaterfilling")
#path_choices_indep_list=( "shortestPath" )
path_choices_dep_list=( "priceSchemeWindow")
path_choices_indep_list=(  )
scale=10

eta=0.025
alpha=0.05
kappa=0.025
updateQueryTime=1
minPriceRate=0.25
zeta=0.01
rho=0

tau=10
normalizer=100

cp hostNodeBase.ned ${PATH_NAME}
cp hostNodeWaterfilling.ned ${PATH_NAME}
cp hostNodeLandmarkRouting.ned ${PATH_NAME}
cp hostNodePriceScheme.ned ${PATH_NAME}
cp routerNode.ned ${PATH_NAME}

for (( i=0; i<${arraylength}; i++));
do 
    workloadname="${prefix[i]}_circ_demand${scale}"
    topofile="${PATH_NAME}${prefix[i]}_topo.txt"
    workload="${PATH_NAME}$workloadname"
    network=${prefix[i]}_circ_net

    #routing schemes where number of path choices doesn't matter
    for routing_scheme in "${path_choices_indep_list[@]}" 
    do
      output_file=outputs/${prefix[i]}_circ_${routing_scheme}_demand${scale}0
      inifile=${PATH_NAME}${prefix[i]}_circ_${routing_scheme}_demand${scale}.ini

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
              --routing-scheme ${routing_scheme}\
              --demand-scale ${scale}


      # run the omnetexecutable with the right parameters
      ./spiderNet -u Cmdenv -f $inifile -c ${network}_${routing_scheme}_demand${scale}  -n ${PATH_NAME}\
            > ${output_file}.txt & 
    done

  #routing schemes where number of path choices matter
    for routing_scheme in  "${path_choices_dep_list[@]}" 
    do
      pids=""
      # if you add more choices for the number of paths you might run out of cores/memory
      for numPathChoices in 4
      do
        output_file=outputs/${prefix[i]}_circ_${routing_scheme}_demand${scale}0
        inifile=${PATH_NAME}${prefix[i]}_circ_${routing_scheme}_demand${scale}0.ini

        if [[ $routing_scheme =~ .*Window.* ]]; then
            windowEnabled=true
        else 
            windowEnabled=false
        fi


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
                --normalizer $normalizer \
                --window-enabled $windowEnabled \
                --demand-scale ${scale}
                --xi $xi\
                --routerQueueDrainTime $routerQueueDrainTime\
                --serviceArrivalWindow $serviceArrivalWindow

        # run the omnetexecutable with the right parameters
        # in the background
        ./spiderNet -u Cmdenv -f ${inifile}\
            -c ${network}_${routing_scheme}_demand${scale}_${numPathChoices} -n ${PATH_NAME}\
            > ${output_file}.txt &
        pids+=($!)
      done 
    done
done
wait
