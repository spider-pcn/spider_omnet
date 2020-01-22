#!/bin/bash
PATH_NAME="/home/ubuntu/omnetpp-5.4.1/samples/spider_omnet/benchmarks/circulations/"
GRAPH_PATH="/home/ubuntu/omnetpp-5.4.1/samples/spider_omnet/scripts/figures/"

balance=100

routing_scheme=$1
capacity_type=$2
pathChoice=$3
schedulingAlgorithm=$4
echo $routing_scheme
random_init_bal=false
random_capacity=false
lnd_capacity=false

widestPathsEnabled=false
obliviousRoutingEnabled=false
kspYenEnabled=false

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
queueDelayThreshold=300
balanceThreshold=0.1
minDCTCPWindow=1
rateDecreaseFrequency=3.0

for suffix in "Base" "Waterfilling" "LndBaseline" "PriceScheme" "DCTCP" "Celer"
do
    cp hostNode${suffix}.ned ${PATH_NAME}
    cp routerNode${suffix}.ned ${PATH_NAME}
done
cp hostNodeLandmarkRouting.ned ${PATH_NAME}
cp routerNodeDCTCPBal.ned ${PATH_NAME}

PYTHON="/usr/bin/python"
mkdir -p ${PATH_NAME}

if [ -z "$pathChoice" ]; then
    pathChoice="shortest"
fi

if [ -z "$schedulingAlgorithm" ]; then
    schedulingAlgorithm="LIFO"
fi

prefix="sf_50_routers"
workload_prefix="sf_50_routers"
if [ -z "$capacity_type" ]; then
    random_capacity=false
    lnd_capacity=false
    echo "constant capacity"
elif [ $capacity_type == "random" ]; then
    random_capacity=true
    prefix="sf_50_routers_randomCap"
    echo "normal dist"
elif [ $capacity_type == "lnd" ]; then
    lnd_capacity=true
    prefix="sf_50_routers_lndCap"
    echo "lnd_dist"
fi

echo $pathChoice

#balance_scale=("100" "200" "400" "800" "1600" "3200") #"1200" "2400" "4800" "9600") 
balance_scale=("2750" "4000" "5900" "8750" "12000") # #300 400 800 1600 3200 600 1200 2400 4800 9600
for num in {0..4}
do
    echo "doing run $num"
    for balance in "${balance_scale[@]}"
    do
        network="${prefix}_circ_net"
        topofile="${PATH_NAME}${prefix}_topo${balance}.txt"
        graph_type="scale_free"
        delay="30"
        scale="3"
        
        # generate the graph first to ned file
        workloadname="${workload_prefix}_circ${num}_demand${scale}"
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
          output_file=outputs/${prefix}_${balance}_circ${num}_${routing_scheme}_demand${scale}0_${pathChoice}
          inifile=${PATH_NAME}${prefix}_${balance}_circ${num}_${routing_scheme}_demand${scale}_${pathChoice}.ini

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
                  --path-choice $pathChoice\
                  --balance $balance\
                  --circ-num $num


          # run the omnetexecutable with the right parameters
          ./spiderNet -u Cmdenv -f $inifile -c \
          ${network}_${balance}_${routing_scheme}_circ${num}_demand${scale}_${pathChoice} -n ${PATH_NAME}\
                > ${output_file}.txt & 
        
      else
          pids=""
          # if you add more choices for the number of paths you might run out of cores/memory
          for numPathChoices in 4
          do
            output_file=outputs/${prefix}_${balance}_circ${num}_${routing_scheme}_demand${scale}0_${pathChoice}_${numPathChoices}_${schedulingAlgorithm}
            inifile=${PATH_NAME}${prefix}_${balance}_circ${num}_${routing_scheme}_demand${scale}_${pathChoice}_${numPathChoices}_${schedulingAlgorithm}.ini

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
                    --path-choice $pathChoice\
                    --scheduling-algorithm $schedulingAlgorithm\
                    --balance $balance\
                    --circ-num $num \
                    --window-alpha $windowAlpha \
                    --window-beta $windowBeta \
                    --queue-threshold $queueThreshold \
                    --queue-delay-threshold $queueDelayThreshold \
                    --balance-ecn-threshold $balanceThreshold \
                    --mtu $mtu\
                    --min-dctcp-window $minDCTCPWindow\
                    --rate-decrease-frequency $rateDecreaseFrequency

            # run the omnetexecutable with the right parameters
            # in the background
            ./spiderNet -u Cmdenv -f ${inifile} -c\
            ${network}_${balance}_${routing_scheme}_circ${num}_demand${scale}_${pathChoice}_${numPathChoices}_${schedulingAlgorithm}  -n ${PATH_NAME}\
                > ${output_file}.txt &
            pids+=($!)
            done
        fi
        wait # for all algorithms to complete for this demand

        # STEP 4: plot everything for this demand
        # TODO: add plotting script
        echo "Plotting"
        payment_graph_type='circ' 
        if [ "$timeoutEnabled" = true ] ; then timeout="timeouts"; else timeout="no_timeouts"; fi
        if [ "$random_init_bal" = true ] ; then suffix="randomInitBal_"; else suffix=""; fi
        echo $suffix
        graph_op_prefix=${GRAPH_PATH}${timeout}/${prefix}${balance}_circ${num}_delay${delay}_demand${scale}0_${suffix}
        vec_file_prefix=${PATH_NAME}results/${prefix}_${payment_graph_type}_net_${balance}_
        
        #routing schemes where number of path choices doesn't matter
        if [ ${routing_scheme} ==  "shortestPath" ]; then 
            vec_file_path=${vec_file_prefix}${routing_scheme}_circ${num}_demand${scale}_${pathChoice}-#0.vec
            sca_file_path=${vec_file_prefix}${routing_scheme}_circ${num}_demand${scale}_${pathChoice}-#0.sca


            python scripts/generate_analysis_plots_for_single_run.py \
              --detail $signalsEnabled \
              --vec_file ${vec_file_path} \
              --sca_file ${sca_file_path} \
              --save ${graph_op_prefix}${routing_scheme}_${pathChoice} \
              --balance \
              --queue_info --timeouts --frac_completed \
              --inflight --timeouts_sender \
              --waiting --bottlenecks --time_inflight --queue_delay 
        

        #routing schemes where number of path choices matter
        else
          for numPathChoices in 4
            do
                vec_file_path=${vec_file_prefix}${routing_scheme}_circ${num}_demand${scale}_${pathChoice}_${numPathChoices}_${schedulingAlgorithm}-#0.vec
                sca_file_path=${vec_file_prefix}${routing_scheme}_circ${num}_demand${scale}_${pathChoice}_${numPathChoices}_${schedulingAlgorithm}-#0.sca


                python scripts/generate_analysis_plots_for_single_run.py \
                  --detail $signalsEnabled \
                  --vec_file ${vec_file_path} \
                  --sca_file ${sca_file_path} \
                  --save ${graph_op_prefix}${routing_scheme}_${pathChoice}_${numPathChoices}_${schedulingAlgorithm}  \
                  --balance \
                  --queue_info --timeouts --frac_completed \
                  --frac_completed_window \
                  --inflight --timeouts_sender --time_inflight \
                  --waiting --bottlenecks --probabilities \
                  --mu_local --lambda --n_local --service_arrival_ratio --inflight_outgoing \
                  --inflight_incoming --rate_to_send --price --mu_remote --demand \
                  --rate_sent --amt_inflight_per_path --rate_acked --fraction_marked --queue_delay
              done
          fi

        # STEP 5: cleanup        
        #rm ${PATH_NAME}${prefix}_circ*_demand${scale}.ini
        #rm ${workload}_workload.txt
        #rm ${workload}.json
    done
    #rm $topofile
done

