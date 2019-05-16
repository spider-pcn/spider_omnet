#!/bin/bash
PATH_NAME="/home/ubuntu/omnetpp-5.4.1/samples/spider_omnet/benchmarks/circulations/"
GRAPH_PATH="/home/ubuntu/omnetpp-5.4.1/samples/spider_omnet/scripts/figures/"

num_nodes=("2" "2" "3" "4" "5" "5" "5" "0" "0" "10" "20" "50" "60" "80" "100" "200" "400" "600" "800" "1000" \
    "10" "20" "50" "60" "80" "100" "200" "400" "600" "800" "1000" "40" "10" "20" "30" "40")

balance=100

prefix=("two_node_imbalance" "two_node_capacity" "three_node" "four_node" "five_node_hardcoded" \
    "hotnets" "five_line" "lnd_dec4_2018" "lnd_dec28_2018" \
    "sw_10_routers" "sw_20_routers" "sw_50_routers" "sw_60_routers" "sw_80_routers"  \
    "sw_100_routers" "sw_200_routers" "sw_400_routers" "sw_600_routers" \
    "sw_800_routers" "sw_1000_routers"\
    "sf_10_routers" "sf_20_routers" \
    "sf_50_routers" "sf_60_routers" "sf_80_routers"  \
    "sf_100_routers" "sf_200_routers" "sf_400_routers" "sf_600_routers" \
    "sf_800_routers" "sf_1000_routers" "tree_40_routers" "random_10_routers" "random_20_routers"\
    "random_30_routers" "sw_sparse_40_routers")

demand_scale=("1") # "60" "90")
path_choices_capacity_factor_dep_list=("priceSchemeWindow") # "landmarkRouting")  # "smoothWaterfilling")
path_choices_indep_list=()
random_init_bal=false
random_capacity=false


#general parameters that do not affect config names
simulationLength=5000
statCollectionRate=25
timeoutClearRate=1
timeoutEnabled=true
signalsEnabled=true
loggingEnabled=false

# scheme specific parameters
eta=0.025
alpha=0.2
kappa=0.025
updateQueryTime=1.5
minPriceRate=0.25
zeta=0.01
rho=0.04
tau=10
normalizer=100
xi=1
routerQueueDrainTime=5
serviceArrivalWindow=300
capacityFactor=0.9
capacityFactorTimes10=9

for suffix in "Base" "Waterfilling" "LndBaseline" "PriceScheme"
do
    cp hostNode${suffix}.ned ${PATH_NAME}
    cp routerNode${suffix}.ned ${PATH_NAME}
done
cp hostNodeLandmarkRouting.ned ${PATH_NAME}

arraylength=${#prefix[@]}
PYTHON="/usr/bin/python"
mkdir -p ${PATH_NAME}

# TODO: find the indices in prefix of the topologies you want to run on and then specify them in array
# adjust experiment time as needed
#array=( 0 1 4 5 8 19 32)
array=( 9 ) #10 11 13 22 24)
for i in "${array[@]}" 
do
    network="${prefix[i]}_circ_net"
    topofile="${PATH_NAME}${prefix[i]}_topo.txt"

    # identify graph type for topology
    if [ ${prefix[i]:0:2} == "sw" ]; then
        graph_type="small_world"
    elif [ ${prefix[i]:0:2} == "sf" ]; then
        graph_type="scale_free"
    elif [ ${prefix[i]:0:4} == "tree" ]; then
        graph_type="tree"
    elif [ ${prefix[i]:0:3} == "lnd" ]; then
        graph_type=${prefix[i]}
    elif [ ${prefix[i]} == "hotnets" ]; then
        graph_type="hotnets_topo"
    elif [ ${prefix[i]:0:6} == "random" ]; then
        graph_type="random"
    else
        graph_type="simple_topologies"
    fi
    
    # set delay amount
    if [[ (${prefix[i]:0:3} == "two") || (${prefix[i]:0:5} == "three") ]]; then
        delay="120"
    else
        delay="30"
    fi
    
    # STEP 1: create topology
    $PYTHON scripts/create_topo_ned_file.py $graph_type\
            --network-name ${PATH_NAME}$network\
            --topo-filename $topofile\
            --num-nodes ${num_nodes[i]}\
            --balance-per-channel $balance\
            --separate-end-hosts \
            --delay-per-channel $delay\
            --randomize-start-bal $random_init_bal\
            --random-channel-capacity $random_capacity 


    # create workload files and run different demand levels
    for scale in "${demand_scale[@]}"
    do

        # generate the graph first to ned file
        workloadname="${prefix[i]}_circ_demand${scale}"
        workload="${PATH_NAME}$workloadname"
        inifile="${PATH_NAME}${workloadname}_default.ini"
        payment_graph_topo="custom"
        
        # figure out payment graph/workload topology
        if [ ${prefix[i]:0:9} == "five_line" ]; then
            payment_graph_topo="simple_line"
        elif [ ${prefix[i]:0:5} == "three" ]; then
            payment_graph_topo="simple_line"
        elif [ ${prefix[i]:0:4} == "five" ]; then
            payment_graph_topo="hardcoded_circ"
        elif [ ${prefix[i]:0:7} == "hotnets" ]; then
            payment_graph_topo="hotnets_topo"
        fi

        echo $network
        echo $topofile
        echo $inifile
        echo $graph_type

        # STEP 2: create transactions corresponding to this experiment run
        $PYTHON scripts/create_workload.py $workload uniform \
                --graph-topo $payment_graph_topo \
                --payment-graph-dag-percentage 0\
                --topo-filename $topofile\
                --experiment-time $simulationLength \
                --balance-per-channel $balance\
                --generate-json-also \
                --timeout-value 5 \
                --scale-amount $scale \
                --txn-size-mean 1


        # STEP 3: run the experiment
        # routing schemes where number of path choices doesn't matter
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
          ./spiderNet -u Cmdenv -f $inifile -c ${network}_${routing_scheme}_demand${scale} -n ${PATH_NAME}\
                > ${output_file}.txt & 
        done

        #routing schemes where number of path choices matter
        for routing_scheme in  "${path_choices_capacity_factor_dep_list[@]}" 
        do
          pids=""
          # if you add more choices for the number of paths you might run out of cores/memory
          for numPathChoices in 4
          do
            output_file=outputs/${prefix[i]}_circ_${routing_scheme}_demand${scale}0_capacityFactorTimes10_${capacityFactorTimes10}
            inifile=${PATH_NAME}${prefix[i]}_circ_${routing_scheme}_demand${scale}.ini

            if [[ $routing_scheme =~ .*Window.* ]]; then
                windowEnabled=true
            else 
                windowEnabled=false
            fi


            echo "Creating ini file"
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
										--capacityFactor $capacityFactor\
                    --update-query-time $updateQueryTime\
                    --min-rate $minPriceRate\
                    --tau $tau\
                    --normalizer $normalizer \
                    --window-enabled $windowEnabled\
                    --demand-scale $scale\
                    --xi $xi\
                    --router-queue-drain-time $routerQueueDrainTime\
                    --service-arrival-window $serviceArrivalWindow


            # run the omnetexecutable with the right parameters
            # in the background
            ./spiderNet -u Cmdenv -f ${inifile}\
                -c ${network}_${routing_scheme}_demand${scale}_${numPathChoices}_capacityFactorTimes10_${capacityFactorTimes10} -n ${PATH_NAME}\
                > ${output_file}.txt &
            pids+=($!)
         done 
        done
        wait # for all algorithms to complete for this demand

        # STEP 4: plot everything for this demand
        # TODO: add plotting script
        echo "Plotting"
        payment_graph_type='circ' 
        if [ "$timeoutEnabled" = true ] ; then timeout="timeouts"; else timeout="no_timeouts"; fi
        if [ "$random_init_bal" = true ] ; then suffix="randomInitBal_"; else suffix=""; fi
        if [ "$random_capacity" = true ]; then suffix="${suffix}randomCapacity_"; fi
        echo $suffix
        graph_op_prefix=${GRAPH_PATH}${timeout}/${prefix[i]}_delay${delay}_demand${scale}0_${suffix}_capacityFactorTimes10_${capacityFactorTimes10}
        vec_file_prefix=${PATH_NAME}results/${prefix[i]}_${payment_graph_type}_net_
        
        #routing schemes where number of path choices doesn't matter
        for routing_scheme in "${path_choices_indep_list[@]}"  #silentWhispers
        do
            vec_file_path=${vec_file_prefix}${routing_scheme}_demand${scale}-#0.vec
            sca_file_path=${vec_file_prefix}${routing_scheme}_demand${scale}-#0.sca


            python scripts/generate_analysis_plots_for_single_run.py \
              --detail $signalsEnabled \
              --vec_file ${vec_file_path} \
              --sca_file ${sca_file_path} \
              --save ${graph_op_prefix}${routing_scheme} \
              --balance \
              --queue_info --timeouts --frac_completed \
              --inflight --timeouts_sender \
              --waiting --bottlenecks
        done

        #routing schemes where number of path choices matter
        for routing_scheme in "${path_choices_capacity_factor_dep_list[@]}"     
        do
          for numPathChoices in 4
            do
                vec_file_path=${vec_file_prefix}${routing_scheme}_demand${scale}_${numPathChoices}_capacityFactorTimes10_${capacityFactorTimes10}-#0.vec
                sca_file_path=${vec_file_prefix}${routing_scheme}_demand${scale}_${numPathChoices}_capacityFactorTimes10_${capacityFactorTimes10}-#0.sca


                python scripts/generate_analysis_plots_for_single_run.py \
                  --detail $signalsEnabled \
                  --vec_file ${vec_file_path} \
                  --sca_file ${sca_file_path} \
                  --save ${graph_op_prefix}${routing_scheme}_final \
                  --balance \
                  --queue_info --timeouts --frac_completed \
                  --frac_completed_window \
                  --inflight --timeouts_sender \
                  --waiting --bottlenecks --probabilities \
                  --mu_local --lambda --n_local --service_arrival_ratio --inflight_outgoing \
                  --inflight_incoming --rate_to_send --price --mu_remote --demand \
                  --rate_sent --amt_inflight_per_path
              done
        done

        # STEP 5: cleanup        
        #rm ${PATH_NAME}${prefix[i]}_circ*_demand${scale}.ini
        #rm ${workload}_workload.txt
        #rm ${workload}.json
    done
    #rm $topofile
done

