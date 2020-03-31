#!/bin/bash
source run_single_experiment.sh
source plot_results.sh
source setup.sh

# default arguments
prefix="sw_50_routers"
workload_prefix="sw_50_routers"
routing_scheme="DCTCPQ"
num_start=0
num_end=4
balance_list=("100" "200" "400" "800" "1600") 
path_choice="widest"
scheduling_alg="LIFO"
num_paths=4
exp_type="circ"
demand_scale="3"
dag_percent_list=("20" "45" "65")
rebalancing_rate_list=("10" "100" "1000" "10000" "100000")

# help message
function usage {
    echo "Usage: "
    echo "./run_experiments.sh"
    echo -e "\t-h --help"
    echo -e "\t--prefix=$prefix"
    echo -e "\t--workload-prefix=$workload_prefix"
    echo -e "\t--exp-type=$exp_type"
    echo -e "\t--balance-list=\"100 200 400 800\""
    echo -e "\t--dag-percent-list=\"25 45 65\""
    echo -e "\t--rebalancing-rate-list=\"10 100 1000 10000 100000\""
    echo -e "\t--routing-scheme=$routing_scheme"
    echo -e "\t--num-start=$num_start"
    echo -e "\t--num-end=$num_end"
    echo -e "\t--path-choice=$path_choice"
    echo -e "\t--num-paths=$num_paths"
    echo -e "\t--scheduling-alg=$scheduling_alg"
    echo -e ""
}


# parse the arguments
while [ "$1" != "" ]; do
    PARAM=`echo $1 | awk -F= '{print $1}'`
    VALUE=`echo $1 | awk -F= '{print $2}'`
    case $PARAM in
        -h | --help)
            usage
            exit
            ;;
        --prefix)
            prefix=$VALUE
            ;;
        --workload-prefix)
            workload_prefix=$VALUE
            ;;
        --routing-scheme)
            routing_scheme=$VALUE
            echo $routing_scheme
            ;;
        --num-start)
            num_start=$VALUE
            ;;
        --num-end)
            num_end=$VALUE
            ;;
        --balance-list)
            balance_list=$(echo $VALUE | tr " " "\n")
            ;;
        --dag-percent-list)
            dag_percent_list=$(echo $VALUE | tr " " "\n")
            ;;
        --rebalancing-rate-list)
            rebalancing_rate_list=$(echo $VALUE | tr " " "\n")
            ;;
        --path-choice)
            path_choice=$VALUE
            ;;
        --num-paths)
            num_paths=$VALUE
            ;;
        --exp-type)
            exp_type=$VALUE
            ;;
        --scheduling-alg)
            scheduling_alg=$VALUE
            ;;
        *)
            echo "ERROR: unknown parameter \"$PARAM\""
            usage
            exit 1
            ;;
    esac
    shift
done


#iterate through balance list and between the start and end circulations
for ((num=$num_start;num<=$num_end;num+=1));
do
    for balance in $balance_list
    do
    # generate the graph first to ned file
    echo $prefix
    echo $balance
    echo $num

    if [[ "$exp_type" == "rebalance"]]
    then
    elif [[ "$exp_type" == "dag" ]]
    then
        for dag_amt in $dag_percent_list
        do
            DAG_PATH_NAME="${PATH_PREFIX}dag${dag_amt}/"
            setup $DAG_PATH_NAME

            workloadname="${workload_prefix}_demand${scale}_dag${dag_amt}_num${num}"
            workload="${DAG_PATH_NAME}$workloadname"
            inifile="${DAG_PATH_NAME}${workloadname}_default.ini"
            network="${prefix}_dag${dag_amt}_net"
            topofile="${DAG_PATH_NAME}${prefix}_topo${balance}.txt"
    
            # run this one circulation experiment
            run_single_dag_experiment $prefix $balance $num $routing_scheme \
                $path_choice $num_paths $scheduling_alg $demand_scale $dag_amt $exp_type

            # plot results
            process_single_dag_experiment_results $prefix $balance $num $routing_scheme \
                $path_choice $num_paths $scheduling_alg $demand_scale $dag_amt
        done
    else 
        setup ${CIRC_PATH_NAME}
        workloadname="${workload_prefix}_circ${num}_demand${demand_scale}"
        workload="${CIRC_PATH_NAME}$workloadname"
        inifile="${CIRC_PATH_NAME}${workloadname}_default.ini"
        network="${prefix}_circ_net"
        topofile="${CIRC_PATH_NAME}${prefix}_topo${balance}.txt"
        
        # run this one circulation experiment
        run_single_circ_experiment $prefix $balance $num $routing_scheme \
            $path_choice $num_paths $scheduling_alg $demand_scale $exp_type

        # plot results
        process_single_circ_experiment_results $prefix $balance $num $routing_scheme \
            $path_choice $num_paths $scheduling_alg $demand_scale
    fi
    done
done



