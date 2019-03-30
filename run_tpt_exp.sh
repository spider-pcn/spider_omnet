#!/bin/bash
PATH_NAME="/home/ubuntu/omnetpp-5.4.1/samples/spider_omnet/benchmarks/circulations/"
GRAPH_PATH="/home/ubuntu/omnetpp-5.4.1/samples/spider_omnet/scripts/figures/"


routing_scheme=$1
pathChoice=$2
echo $routing_scheme
random_init_bal=false
random_capacity=false

#general parameters that do not affect config names
simulationLength=3100
statCollectionRate=100
timeoutClearRate=1
timeoutEnabled=true
signalsEnabled=true
loggingEnabled=false
transStatStart=2000
transStatEnd=3000

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

cp hostNodeBase.ned ${PATH_NAME}
cp hostNodeWaterfilling.ned ${PATH_NAME}
cp hostNodeLandmarkRouting.ned ${PATH_NAME}
cp hostNodePriceScheme.ned ${PATH_NAME}
cp hostNodeLndBaseline.ned ${PATH_NAME}
cp routerNode.ned ${PATH_NAME}

arraylength=${#prefix[@]}
PYTHON="/usr/bin/python"
mkdir -p ${PATH_NAME}

if [ -z "$pathChoice" ]; then
    pathChoice="shortest"
fi

echo $pathChoice


capacity_list=("20" "40" "80" "160" "320")
demand_list=("10" "20" "40" "80" "160")
length=${#capacity_list[@]}

for (( i=0; i<$length; i++ ));
do
    prefix="lnd_uniform"
    capacity=${capacity_list[i]}
    scale=${demand_list[i]}

    echo $capacity
    echo $scale

    #prefix="five_node_hardcoded"
    network="${prefix}_cap${capacity}_circ_net"
    topofile="${PATH_NAME}${prefix}_cap${capacity}_topo.txt"
    echo ${capacity}

    # identify graph type for topology
    if [ ${prefix:0:2} == "sw" ]; then
        graph_type="small_world"
    elif [ ${prefix:0:2} == "sf" ]; then
        graph_type="scale_free"
    elif [ ${prefix:0:4} == "tree" ]; then
        graph_type="tree"
    elif [ ${prefix:0:3} == "lnd" ]; then
        graph_type=${prefix}
    elif [ ${prefix} == "hotnets" ]; then
        graph_type="hotnets_topo"
    elif [ ${prefix:0:6} == "random" ]; then
        graph_type="random"
    else
        graph_type="simple_topologies"
    fi
    
    # set delay amount
    if [[ (${prefix:0:3} == "two") || (${prefix:0:5} == "three") ]]; then
        delay="120"
    else
        delay="30"
    fi


    # workload info for infinite workload


    # generate the graph first to ned file
    workloadname="${prefix}_circ_demand${scale}"
    workload="${PATH_NAME}$workloadname"
    inifile="${PATH_NAME}${workloadname}_default.ini"
    payment_graph_topo="custom"
    
    echo $network
    echo $topofile
    echo $inifile
    echo $graph_type
    echo $workloadname


    # STEP 3: run the experiment
    # routing schemes where number of path choices doesn't matter
    if [ ${routing_scheme} ==  "shortestPath" ]; then 
      output_file=outputs/${prefix}_circ_${routing_scheme}_demand${scale}0_${pathChoice}_cap${capacity}
      inifile=${PATH_NAME}${prefix}_circ_${routing_scheme}_demand${scale}_${pathChoice}.ini

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
              --capacity ${capacity}


      # run the omnetexecutable with the right parameters
      ./spiderNet -u Cmdenv -f $inifile -c ${network}_${routing_scheme}_demand${scale}_${pathChoice}_cap${capacity} -n ${PATH_NAME}\
            > ${output_file}.txt & 
    
    else
      pids=""
      # if you add more choices for the number of paths you might run out of cores/memory
      for numPathChoices in 4
      do
        output_file=outputs/${prefix}_circ_${routing_scheme}_demand${scale}0_${pathChoice}_cap${capacity}
        inifile=${PATH_NAME}${prefix}_circ_${routing_scheme}_demand${scale}_${pathChoice}.ini

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
                --capacity ${capacity}

            # run the omnetexecutable with the right parameters
            # in the background
            ./spiderNet -u Cmdenv -f ${inifile}\
                -c ${network}_${routing_scheme}_demand${scale}_${pathChoice}_cap${capacity}_${numPathChoices} -n ${PATH_NAME}\
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
    if [ "$random_capacity" = true ]; then suffix="${suffix}randomCapacity_"; fi
    suffix=${suffix}cap${capacity}_
    echo $suffix
    graph_op_prefix=${GRAPH_PATH}${timeout}/${prefix}_delay${delay}_demand${scale}0_${suffix}
    vec_file_prefix=${PATH_NAME}results/${prefix}_cap${capacity}_${payment_graph_type}_net_
        
    #routing schemes where number of path choices doesn't matter
    if [ ${routing_scheme} ==  "shortestPath" ]; then 
        vec_file_path=${vec_file_prefix}${routing_scheme}_demand${scale}_${pathChoice}_cap${capacity}-#0.vec
        sca_file_path=${vec_file_prefix}${routing_scheme}_demand${scale}_${pathChoice}_cap${capacity}-#0.sca


        python scripts/generate_analysis_plots_for_single_run.py \
          --detail $signalsEnabled \
          --vec_file ${vec_file_path} \
          --sca_file ${sca_file_path} \
          --save ${graph_op_prefix}${routing_scheme}_${pathChoice} \
          --balance \
          --queue_info --timeouts --frac_completed \
          --inflight --timeouts_sender \
          --waiting --bottlenecks
        

        #routing schemes where number of path choices matter
    else
      for numPathChoices in 4
        do
            vec_file_path=${vec_file_prefix}${routing_scheme}_demand${scale}_${pathChoice}_cap${capacity}_${numPathChoices}-#0.vec
            sca_file_path=${vec_file_prefix}${routing_scheme}_demand${scale}_${pathChoice}_cap${capacity}_${numPathChoices}-#0.sca


            python scripts/generate_analysis_plots_for_single_run.py \
              --detail $signalsEnabled \
              --vec_file ${vec_file_path} \
              --sca_file ${sca_file_path} \
              --save ${graph_op_prefix}${routing_scheme}_${pathChoice}_lowstep \
              --balance \
              --queue_info --timeouts --frac_completed \
              --frac_completed_window \
              --inflight --timeouts_sender \
              --waiting --bottlenecks --probabilities \
              --mu_local --lambda --n_local --service_arrival_ratio --inflight_outgoing \
              --inflight_incoming --rate_to_send --price --mu_remote --demand \
              --rate_sent --amt_inflight_per_path
          done
      fi

    # STEP 5: cleanup        
    #rm ${PATH_NAME}${prefix[i]}_circ*_demand${scale}.ini
    #rm ${workload}_workload.txt
    #rm ${workload}.json
done
    #rm $topofile
