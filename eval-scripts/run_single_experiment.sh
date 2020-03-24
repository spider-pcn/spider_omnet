#!/bin/bash
source parameters.sh
source setup.sh

function run_single_experiment {
    prefix=$1
    balance=$2
    num=$3
    routing_scheme=$4
    path_choice=$5
    num_paths=$6
    scheduling_alg=$7
    
    network="${prefix}_circ_net"
    topofile="${PATH_NAME}${prefix}_topo${balance}.txt"
    
    output_file=${DEFAULT_PATH}/outputs/${prefix}_${balance}_circ${num}
    output_file=${output_file}_${routing_scheme}_demand${demand_scale}0
    output_file=${output_file}_${path_choice}_${num_paths}_${scheduling_alg}
    
    inifile=${PATH_NAME}${prefix}_${balance}
    inifile=${inifile}_circ${num}_${routing_scheme}_demand${demand_scale}
    inifile=${inifile}_${path_choice}_${num_paths}_${scheduling_alg}.ini

    windowEnabled=true
    config_name=${network}_${balance}_${routing_scheme}
    config_name=${config_name}_circ${num}_demand${demand_scale}_${path_choice}
    config_name=${config_name}_${num_paths}_${scheduling_alg} 

    echo "Creating ini file"
    # create the ini file with specified parameters
    python ${DEFAULT_PATH}/scripts/create_ini_file.py \
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
            --num-path-choices ${num_paths}\
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
            --demand-scale $demand_scale\
            --xi $xi\
            --router-queue-drain-time $routerQueueDrainTime\
            --service-arrival-window $serviceArrivalWindow\
            --transStatStart $transStatStart\
            --transStatEnd $transStatEnd\
            --path-choice $path_choice\
            --scheduling-algorithm $scheduling_alg\
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
    ${DEFAULT_PATH}/spiderNet -u Cmdenv -f ${inifile} -c ${config_name} -n ${PATH_NAME}\
        > ${output_file}.txt
}


