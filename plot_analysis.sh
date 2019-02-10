#!/bin/bash
PATH_NAME="benchmarks/"
GRAPH_PATH="scripts/figures/"

#prefix=("two_node" "three_node" "four_node" "five_node") 
prefix=("two_node_imbalance" "two_node_capacity") #"sw_sparse_40_routers" "sf_40_routers")
    #"sw_400_routers" "sf_400_routers")
    #"sw_1000_routers" "sf_1000_routers")

arraylength=${#prefix[@]}

#general parameters that do not affect config names
simulationLength=1000
statCollectionRate=25
timeoutClearRate=1
timeoutEnabled=true

payment_graph_type=circ # or dagx where x is the percentage of dag
if [ "$payment_graph_type" = "circ" ]; then 
    PATH_NAME=${PATH_NAME}circulations/ 
else 
    PATH_NAME=${PATH_NAME}${payment_graph_type}
fi

delay=30ms
routing_scheme_list=("smoothWaterfilling" "waterfilling" "shortestPath")


for (( i=0; i<${arraylength}; i++));
do 

    if [ "$timeoutEnabled" = true ] ; then timeout="timeouts"; else timeout="no_timeouts"; fi
    graph_op_prefix=${GRAPH_PATH}${timeout}/${prefix[i]}_${delay}_${simulationLength}_
    vec_file_prefix=${PATH_NAME}results/${prefix[i]}_${payment_graph_type}_net_

    #routing schemes where number of path choices doesn't matter
    for routing_scheme in shortestPath #silentWhispers
    do
        if [[ " ${routing_scheme_list[*]} " == *"$routing_scheme"* ]]; then
            vec_file_path=${vec_file_prefix}${routing_scheme}-#0.vec
            sca_file_path=${vec_file_prefix}${routing_scheme}-#0.sca


            python scripts/generate_analysis_plots_for_single_run.py \
              --vec_file ${vec_file_path} \
              --sca_file ${sca_file_path} \
              --save ${graph_op_prefix}${routing_scheme} \
              --balance \
              --queue_info --timeouts --frac_completed \
              --inflight --path --timeouts_sender \
              --waiting --bottlenecks
        fi
    done

   #routing schemes where number of path choices matter
    for routing_scheme in waterfilling smoothWaterfilling priceScheme
    do
      for numPathChoices in 4
        do
            if [[ " ${routing_scheme_list[*]} " == *"$routing_scheme"* ]]; then
                vec_file_path=${vec_file_prefix}${routing_scheme}_${numPathChoices}-#0.vec
                sca_file_path=${vec_file_prefix}${routing_scheme}_${numPathChoices}-#0.sca


                python scripts/generate_analysis_plots_for_single_run.py \
                  --vec_file ${vec_file_path} \
                  --sca_file ${sca_file_path} \
                  --save ${graph_op_prefix}${routing_scheme}_final \
                  --balance \
                  --queue_info --timeouts --frac_completed \
                  --frac_completed_window \
                  --inflight --path --timeouts_sender \
                  --waiting --bottlenecks --probabilities \
                  --mu_local --lambda --x_local \
                  --rate_to_send --price --mu_remote --demand
            fi
          done
    done
done

