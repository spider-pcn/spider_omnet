#!/bin/bash
source parameters.sh
source setup.sh

# runs a single circulation experiment either for circulation
# regular experiments or celer or implementation comparisons
function run_single_circ_experiment {
    prefix=$1
    balance=$2
    num=$3
    routing_scheme=$4
    path_choice=$5
    num_paths=$6
    scheduling_alg=$7
    demand_scale=$8
    exp_type=$9

    # implementation comparison parameters
    if [[ "$exp_type" == "impl" ]]
    then
        queueDelayThreshold=80
        simulationLength=610
        statCollectionRate=2
        transStatStart=300
        transStatEnd=500
        demand_scale=10
    elif [[ "$exp_type" == "celer" ]]
    then
        # shorter experiments to compare with celer
        # because celer can't run for that long without 
        # blowing up memory
        simulationLength=610
        transStatStart=400
        transStatEnd=600
    fi
    
    echo "transstatstart"
    echo $transStatStart
    
    network="${prefix}_circ_net"
    topofile="${CIRC_PATH_NAME}${prefix}_topo${balance}.txt"
    
    output_file=${DEFAULT_PATH}/outputs/${prefix}_${balance}_circ${num}
    output_file=${output_file}_${routing_scheme}_demand${demand_scale}0
    output_file=${output_file}_${path_choice}_${num_paths}_${scheduling_alg}
    
    inifile=${CIRC_PATH_NAME}${prefix}_${balance}
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
    ${DEFAULT_PATH}/spiderNet -u Cmdenv -f ${inifile} -c ${config_name} -n ${CIRC_PATH_NAME}\
        > ${output_file}.txt
}

# runs a single dag experiment using parameters default for it
function run_single_dag_experiment {
    prefix=$1
    balance=$2
    num=$3
    routing_scheme=$4
    path_choice=$5
    num_paths=$6
    scheduling_alg=$7
    demand_scale=$8
    dag_amt=$9
    queueDelayThreshold=300
    windowEnabled=true

    DAG_PATH_NAME="${PATH_PREFIX}dag${dag_amt}/"
    network="${prefix}_dag${dag_amt}_net"
    topofile="${DAG_PATH_NAME}${prefix}_topo${balance}.txt"
    
    output_file=${DEFAULT_PATH}/outputs/${prefix}_dag${dag_amt}_${balance}_dag${num}
    output_file=${output_file}_${routing_scheme}_demand${demand_scale}0
    output_file=${output_file}_${path_choice}_${num_paths}_${scheduling_alg}
    
    inifile=${DAG_PATH_NAME}${prefix}_dag${dag_amt}_${balance}
    inifile=${inifile}_dag${num}_${routing_scheme}_demand${demand_scale}
    inifile=${inifile}_${path_choice}_${num_paths}_${scheduling_alg}.ini

    config_name=${network}_${balance}_${routing_scheme}
    config_name=${config_name}_dag${num}_demand${demand_scale}_${path_choice}
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
            --dag-num $num \
            --window-alpha $windowAlpha \
            --window-beta $windowBeta \
            --queue-threshold $queueThreshold \
            --queue-delay-threshold $queueDelayThreshold \
            --balance-ecn-threshold $balanceThreshold \
            --mtu $mtu\
            --min-dctcp-window $minDCTCPWindow\
            --rate-decrease-frequency $rateDecreaseFrequency

    # run the omnetexecutable with the right parameters
    ${DEFAULT_PATH}/spiderNet -u Cmdenv -f ${inifile} -c ${config_name} -n ${DAG_PATH_NAME}\
        > ${output_file}.txt
}

# runs a single rebalancing experiment using parameters default for it
function run_single_rebalancing_experiment {
    prefix=$1
    balance=$2
    num=$3
    routing_scheme=$4
    path_choice=$5
    num_paths=$6
    scheduling_alg=$7
    demand_scale=$8
    dag_amt=$9
    rebalancing_rate=${10}

    queueDelayThreshold=75
    windowEnabled=true

    DAG_PATH_NAME="${PATH_PREFIX}dag${dag_amt}/"
    network="${prefix}_dag${dag_amt}_net"
    topofile="${DAG_PATH_NAME}${prefix}_topo${balance}_rb.txt"
    
    output_file=${DEFAULT_PATH}/outputs/${prefix}_dag${dag_amt}_${balance}_dag${num}
    output_file=${output_file}_${routing_scheme}_demand${demand_scale}0
    output_file=${output_file}_${path_choice}_${num_paths}_${scheduling_alg}
    output_file=${output_file}_rebalancing${rebalancing_rate}
    
    inifile=${DAG_PATH_NAME}${prefix}_dag${dag_amt}_${balance}
    inifile=${inifile}_dag${num}_${routing_scheme}_demand${demand_scale}
    inifile=${inifile}_${path_choice}_${num_paths}_${scheduling_alg}
    inifile=${inifile}_rebalancing${rebalancing_rate}.ini

    config_name=${network}_${balance}_${routing_scheme}
    config_name=${config_name}_dag${num}_demand${demand_scale}_${path_choice}
    config_name=${config_name}_${num_paths}_${scheduling_alg}
    config_name=${config_name}_rebalancing${rebalancing_rate}

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
            --dag-num $num \
            --window-alpha $windowAlpha \
            --window-beta $windowBeta \
            --queue-threshold $queueThreshold \
            --queue-delay-threshold $queueDelayThreshold \
            --balance-ecn-threshold $balanceThreshold \
            --mtu $mtu\
            --min-dctcp-window $minDCTCPWindow\
            --rate-decrease-frequency $rateDecreaseFrequency \
            --rebalancing-enabled \
            --rebalancing-rate $rebalancing_rate 

    # run the omnetexecutable with the right parameters
    ${DEFAULT_PATH}/spiderNet -u Cmdenv -f ${inifile} -c ${config_name} -n ${DAG_PATH_NAME}\
        > ${output_file}.txt
}

