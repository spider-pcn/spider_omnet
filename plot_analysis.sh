#!/bin/bash
PATH_NAME="benchmarks/"
GRAPH_PATH="scripts/figures/"

#prefix=("two_node" "three_node" "four_node" "five_node") 
prefix=( "hotnets" "sf_10_routers")  #"sw_sparse_40_routers" "sf_40_routers")
    #"sw_400_routers" "sf_400_routers")
    #"sw_1000_routers" "sf_1000_routers")

arraylength=${#prefix[@]}

#general parameters that do not affect config names
simulationLength=5000
statCollectionRate=25
timeoutClearRate=1
timeoutEnabled=true
demand=400

payment_graph_type=circ # or dagx where x is the percentage of dag
if [ "$payment_graph_type" = "circ" ]; then 
    PATH_NAME=${PATH_NAME}circulations/ 
else 
    PATH_NAME=${PATH_NAME}${payment_graph_type}
fi

delay=30ms
path_choices_dep_list=( "waterfilling" "smoothWaterfilling" "priceScheme" "priceSchemeWindow" )
path_choices_indep_list=( "shortestPath" )


for (( i=0; i<${arraylength}; i++));
do 

    if [ "$timeoutEnabled" = true ] ; then timeout="timeouts"; else timeout="no_timeouts"; fi
    graph_op_prefix=${GRAPH_PATH}${timeout}/${prefix[i]}_delay${delay}_demand${demand}_
    vec_file_prefix=${PATH_NAME}results/${prefix[i]}_${payment_graph_type}_net_

    #routing schemes where number of path choices doesn't matter
    for routing_scheme in "${path_choices_indep_list[@]}"  #silentWhispers
    do
        vec_file_path=${vec_file_prefix}${routing_scheme}-#0.vec
        sca_file_path=${vec_file_prefix}${routing_scheme}-#0.sca


        python scripts/generate_analysis_plots_for_single_run.py \
          --vec_file ${vec_file_path} \
          --sca_file ${sca_file_path} \
          --save ${graph_op_prefix}${routing_scheme} \
          --balance \
          --queue_info --timeouts --frac_completed \
          --inflight --timeouts_sender \
          --waiting --bottlenecks
    done

   #routing schemes where number of path choices matter
    for routing_scheme in "${path_choices_dep_list[@]}"     
    do
      for numPathChoices in 4
        do
            vec_file_path=${vec_file_prefix}${routing_scheme}_${numPathChoices}-#0.vec
            sca_file_path=${vec_file_prefix}${routing_scheme}_${numPathChoices}-#0.sca


            python scripts/generate_analysis_plots_for_single_run.py \
              --vec_file ${vec_file_path} \
              --sca_file ${sca_file_path} \
              --save ${graph_op_prefix}${routing_scheme}_final \
              --balance \
              --queue_info --timeouts --frac_completed \
              --frac_completed_window \
              --inflight --timeouts_sender \
              --waiting --bottlenecks --probabilities \
              --mu_local --lambda --n_local --bal_sum --inflight_sum \
              --rate_to_send --price --mu_remote --demand \
              --rate_sent --amt_inflight_per_path
          done
    done
done

