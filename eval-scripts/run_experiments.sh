#!/bin/bash
source run_single_experiment.sh
source plot_results.sh

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

# help message
function usage {
    echo "Usage: "
    echo "./run_experiments.sh"
    echo -e "\t-h --help"
    echo -e "\t--prefix=$prefix"
    echo -e "\t--workload-prefix=$workload_prefix"
    echo -e "\t--balance-list=\"100 200 400 800\""
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
        --path-choice)
            path_choice=$VALUE
            ;;
        --num-paths)
            num_paths=$VALUE
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

# run basic setup
./setup.sh

#iterate through balance list and between the start and end circulations
for balance in $balance_list
do
    for ((num=$num_start;num<=$num_end;num+=1));
    do
    # generate the graph first to ned file
    echo $prefix
    echo $balance
    echo $num

    workloadname="${workload_prefix}_circ${num}_demand${scale}"
    workload="${PATH_NAME}$workloadname"
    inifile="${PATH_NAME}${workloadname}_default.ini"
    network="${prefix}_circ_net"
    topofile="${PATH_NAME}${prefix}_topo${balance}.txt"
     
    # run this one experiment
    run_single_experiment $prefix $balance $num $routing_scheme \
        $path_choice $num_paths $scheduling_alg

    # plot results
    process_single_experiment_results $prefix $balance $num $routing_scheme \
        $path_choice $num_paths $scheduling_alg 
    done
done

