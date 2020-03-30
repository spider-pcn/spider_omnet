#!/bin/bash
source parameters.sh
source setup.sh

# plot results given vector, scalar file inputs and output graph prefix
function plot {
    python ${DEFAULT_PATH}/scripts/generate_analysis_plots_for_single_run.py \
      --detail $signalsEnabled \
      --vec_file $1 \
      --sca_file $2 \
      --save $3 \
      --balance \
      --queue_info --timeouts --frac_completed \
      --frac_completed_window \
      --inflight --timeouts_sender --time_inflight \
      --waiting --bottlenecks --probabilities \
      --mu_local --lambda --n_local --service_arrival_ratio --inflight_outgoing \
      --inflight_incoming --rate_to_send --price --mu_remote --demand \
      --rate_sent --amt_inflight_per_path --rate_acked --fraction_marked --queue_delay \
      --cpi --perDestQueue --kStar
}

# find the vec/scalar files for a circulation experiment
function process_single_circ_experiment_results {
    prefix=$1
    balance=$2
    num=$3
    routing_scheme=$4
    path_choice=$5
    num_paths=$6
    scheduling_alg=$7
    demand_scale=$8

    network="${prefix}_circ_net"
    config_name=${network}_${balance}_${routing_scheme}
    config_name=${config_name}_circ${num}_demand${demand_scale}_${path_choice}
    config_name=${config_name}_${num_paths}_${scheduling_alg} 

    vec_file_path=${PATH_NAME}results/${config_name}-#0.vec
    sca_file_path=${PATH_NAME}results/${config_name}-#0.sca
    graph_op_prefix=${GRAPH_PATH}/${prefix}${balance}_circ${num}_delay${delay}_demand${demand_scale}0
    graph_name=${graph_op_prefix}${routing_scheme}_${path_choice}_${num_paths}_${scheduling_alg}
    
    plot $vec_file_path $sca_file_path $graph_name        
}

# find vector/scalar files for a dag experiment
function process_single_dag_experiment_results {
    prefix=$1
    balance=$2
    num=$3
    routing_scheme=$4
    path_choice=$5
    num_paths=$6
    scheduling_alg=$7
    demand_scale=$8
    dag_amt=$9

    DAG_PATH_NAME="${PATH_PREFIX}dag${dag_amt}/"
    network="${prefix}_dag${dag_amt}_net"
    config_name=${network}_${balance}_${routing_scheme}
    config_name=${config_name}_dag${num}_demand${demand_scale}_${path_choice}
    config_name=${config_name}_${num_paths}_${scheduling_alg} 

    vec_file_path=${DAG_PATH_NAME}results/${config_name}-#0.vec
    sca_file_path=${DAG_PATH_NAME}results/${config_name}-#0.sca
    graph_op_prefix=${GRAPH_PATH}/${prefix}_dag${dag_amt}_${balance}_num${num}_delay${delay}_demand${demand_scale}0
    graph_name=${graph_op_prefix}${routing_scheme}_${path_choice}_${num_paths}_${scheduling_alg}

    plot $vec_file_path $sca_file_path $graph_name
}
