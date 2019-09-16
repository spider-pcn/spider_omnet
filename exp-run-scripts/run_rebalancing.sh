#!/bin/bash
PATH_PREFIX="/home/ubuntu/omnetpp-5.4.1/samples/spider_omnet/benchmarks/"
GRAPH_PATH="/home/ubuntu/omnetpp-5.4.1/samples/spider_omnet/scripts/figures/"

routing_scheme=$1
echo $routing_scheme
random_init_bal=false
random_capacity=false
pathChoice=$2
prefix=$3 
workload_prefix=$4 
schedulingAlgorithm="LIFO"

if [ -z "$pathChoice" ]; then
    pathChoice="shortest"
fi

#general parameters that do not affect config names
simulationLength=1010
statCollectionRate=10
timeoutClearRate=1
timeoutEnabled=true
signalsEnabled=true
loggingEnabled=false
transStatStart=800
transStatEnd=1000
mtu=1.0
echo $transStatStart
echo $transStatEnd
echo $signalsEnabled

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

#DCTCP parameters
windowBeta=0.1
windowAlpha=10
queueThreshold=160
queueDelayThreshold=75
balanceThreshold=0.1
minDCTCPWindow=1
rateDecreaseFrequency=3.0

# dynamic paths
changingPathsEnabled=true
pathMonitorRate=10


PYTHON="/usr/bin/python"
mkdir -p ${PATH_PREFIX}

dag_percent=("45") # "45" "65")
balance=4000
scale=3 # "60" "90")
rebalancingDelay=1
for num in {0..4}
do
    for rebalancingRate in 10 100 1000 10000 100000
    do
        echo "doing run $num"
        # create woirkload files and run different demand levels
        for dag_amt in "${dag_percent[@]}"
        do
            # generate the graph first to ned file
            PATH_NAME="${PATH_PREFIX}dag${dag_amt}/"
            mkdir -p ${PATH_NAME}

            for suffix in "Base" "Waterfilling" "LndBaseline" "PriceScheme"
            do
                cp hostNode${suffix}.ned ${PATH_NAME}
                cp routerNode${suffix}.ned ${PATH_NAME}
            done
            cp hostNodeLandmarkRouting.ned ${PATH_NAME}
            
            network="${prefix}_dag${dag_amt}_net"
            topofile="${PATH_NAME}${prefix}_topo${balance}_rb.txt"
            graph_type="lnd"
            delay="30"

            workloadname="${workload_prefix}_demand${scale}_dag${dag_amt}_num${num}"
            workload="${PATH_NAME}$workloadname"
            inifile="${PATH_NAME}${workloadname}_default.ini"
            payment_graph_topo="custom"
            
            echo $network
            echo $topofile
            echo $inifile
            echo $graph_type
           
            # STEP 3: run the experiment
            # routing schemes where number of path choices doesn't matter
            if [ ${routing_scheme} ==  "shortestPath" ]; then 
              configname=${balance}_${routing_scheme}_dag${num}_demand${scale}_
              configname=${configname}${pathChoice}_${schedulingAlgorithm}_rebalancing${rebalancingDelay}_
              configname=${configname}rate${rebalancingRate}
              output_file=outputs/${prefix}_dag${dag_amt}_${configname}
              inifile=${PATH_NAME}${prefix}_dag${dag_amt}_${configname}.ini


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
                      --demand-scale ${scale}\
                      --transStatStart $transStatStart\
                      --transStatEnd $transStatEnd\
                      --path-choice $pathChoice \
                      --balance $balance\
                      --dag-num $num \
                      --rebalancing-delay $rebalancingDelay \
                      --rebalancing-rate $rebalancingRate \
                      --rebalancing-enabled \


              # run the omnetexecutable with the right parameters
              ./spiderNet -u Cmdenv -f $inifile -c ${network}_${configname} -n ${PATH_NAME} > ${output_file}.txt & 

            #routing schemes where number of path choices matter
            else
              pids=""
              # if you add more choices for the number of paths you might run out of cores/memory
              for numPathChoices in 4
              do
                output_file=outputs/${prefix}_dag${dag_amt}_${balance}_dag${num}_${routing_scheme}_demand${scale}0_${pathChoice}_${schedulingAlgorithm}_rebalancing${rebalancingDelay}_rate${rebalancingRate}
                inifile=${PATH_NAME}${prefix}_dag${dag_amt}_${balance}_dag${num}_${routing_scheme}_demand${scale}_${pathChoice}_${schedulingAlgorithm}_rebalancing${rebalancingDelay}_rate${rebalancingRate}
                configname=${network}_${balance}_${routing_scheme}_dag${num}_demand${scale}_${pathChoice}_${numPathChoices}_${schedulingAlgorithm}_rebalancing${rebalancingDelay}_rate${rebalancingRate}

                if [[ $routing_scheme =~ .*Window.* ]]; then
                    windowEnabled=true
                else 
                    windowEnabled=false
                fi

                if [ ${routing_scheme} ==  "DCTCPQ" ]; then 
                    output_file=${output_file}_qd${queueDelayThreshold}
                    inifile=${inifile}_qd${queueDelayThreshold}
                    configname=${configname}_qd${queueDelayThreshold} 
                fi

                echo "Creating ini file"
                # create the ini file with specified parameters
                python scripts/create_ini_file.py \
                        --network-name ${network}\
                        --topo-filename ${topofile}\
                        --workload-filename ${workload}_workload.txt\
                        --ini-filename ${inifile}.ini\
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
                        --window-enabled $windowEnabled\
                        --demand-scale $scale\
                        --xi $xi\
                        --router-queue-drain-time $routerQueueDrainTime\
                        --service-arrival-window $serviceArrivalWindow\
                        --transStatStart $transStatStart\
                        --transStatEnd $transStatEnd\
                        --path-choice $pathChoice \
                        --scheduling-algorithm $schedulingAlgorithm\
                        --balance $balance\
                        --dag-num $num \
                        --window-alpha $windowAlpha \
                        --window-beta $windowBeta \
                        --queue-threshold $queueThreshold \
                        --queue-delay-threshold $queueDelayThreshold \
                        --balance-ecn-threshold $balanceThreshold \
                        --mtu $mtu\
                        --min-dctcp-window $minDCTCPWindow\
                        --rate-decrease-frequency $rateDecreaseFrequency \
                        --rebalancing-delay $rebalancingDelay \
                        --rebalancing-rate $rebalancingRate \
                        --rebalancing-enabled \



                # run the omnetexecutable with the right parameters
                # in the background
                ./spiderNet -u Cmdenv -f ${inifile}.ini -c ${configname} -n ${PATH_NAME}\
                   > ${output_file}.txt &
               pids+=($!)
             done
            fi 
            wait # for all algorithms to complete for this demand

            # STEP 4: plot everything for this demand
            # TODO: add plotting script
            echo "Plotting"
            payment_graph_type="dag${dag_amt}" 
            if [ "$timeoutEnabled" = true ] ; then timeout="timeouts"; else timeout="no_timeouts"; fi
            if [ "$random_init_bal" = true ] ; then suffix="randomInitBal_"; else suffix=""; fi
            if [ "$random_capacity" = true ]; then suffix="${suffix}randomCapacity_"; fi
            echo $suffix
            graph_op_prefix=${GRAPH_PATH}${timeout}/${prefix}_dag${dag_amt}_${balance}_num${num}_delay${delay}_demand${scale}0_${suffix}
            vec_file_prefix=${PATH_NAME}results/${prefix}_dag${dag_amt}_net_${balance}_
            
            #routing schemes where number of path choices doesn't matter
            if [ ${routing_scheme} ==  "shortestPath" ]; then 
                vec_file_path=${PATH_NAME}results/${network}_${configname}-#0.vec
                sca_file_path=${PATH_NAME}results/${network}_${configname}-#0.sca

                python scripts/generate_analysis_plots_for_single_run.py \
                  --detail $signalsEnabled \
                  --vec_file ${vec_file_path} \
                  --sca_file ${sca_file_path} \
                  --save ${graph_op_prefix}${routing_scheme}_${pathChoice}_rebalancing${rebalancingDelay}_rate${rebalancingRate} \
                  --balance \
                  --queue_info --timeouts --frac_completed \
                  --inflight --timeouts_sender \
                  --waiting --bottlenecks --time_inflight --queue_delay \
                  --rebalancing-amt --capacity --bank --inflight_outgoing

            #routing schemes where number of path choices matter
            else
              for numPathChoices in 4
                do
                    vec_file_path=${PATH_NAME}results/${configname}-#0.vec
                    sca_file_path=${PATH_NAME}results/${configname}-#0.sca
                    graph_name=${graph_op_prefix}${routing_scheme}_${pathChoice}_${numPathChoices}_${schedulingAlgorithm}_rebalancing${rebalancingDelay}_rate${rebalancingRate}

                    if [ ${routing_scheme} ==  "DCTCPQ" ]; then 
                        graph_name=${graph_name}_qd${queueDelayThreshold}
                    fi

                    python scripts/generate_analysis_plots_for_single_run.py \
                      --detail $signalsEnabled \
                      --vec_file ${vec_file_path} \
                      --sca_file ${sca_file_path} \
                      --save ${graph_name}\
                      --balance \
                      --queue_info --timeouts --frac_completed \
                      --frac_completed_window \
                      --inflight --timeouts_sender --time_inflight \
                      --waiting --bottlenecks --probabilities \
                      --mu_local --lambda --n_local --service_arrival_ratio --inflight_outgoing \
                      --inflight_incoming --rate_to_send --price --mu_remote --demand \
                      --rate_sent --amt_inflight_per_path --rate_acked --fraction_marked --queue_delay \
                      --rebalancing-amt --capacity --bank --inflight_outgoing --waiting
                done
            fi

            # STEP 5: cleanup        
            #rm ${PATH_NAME}${prefix[i]}_circ*_demand${scale}.ini
            #rm ${workload}_workload.txt
            #rm ${workload}.json
        done
        #rm $topofile
    done
done
