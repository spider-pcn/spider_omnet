#!/bin/bash
# NOTE: while a single OMNET experiment uses a single core,
# the memory consumption of each one of these runs can be on the order o
# 50 GB for a small world run
# this script assumes you can run the schemes - DCTCP and waterfilling
# in parallel, but precisely one 
# experiment per scheme 
# (in total there are 5 balances * 5 circulations = 25 experiments per scheme)
# Do not use this script as is, without first judging the capabilities of
# your system to run experiments in parallel

# Small world topology, kaggle data for transactions

# LND balances
# make sure widest paths are generated
for scheme in "DCTCPQ" "waterfilling" "lndBaseline" "landmarkRouting" "shortestPath"
do
    ./run_experiment_set.sh \
        --prefix=sw_50_routers_lndCap \
        --workload-prefix=sw_50_routers \
        --exp-type=circ \
        --routing-scheme=${scheme} \
        --num_start=0 --num_end=4 \
        --balance-list="900 1350 2750 4000 8750" \
        --path-choice=widest \
        --num-paths=4 \
        --demand-scale="3" \
        --scheduling-alg=LIFO &
done
wait

# uniform balances
for scheme in "DCTCPQ" "waterfilling" "lndBaseline" "landmarkRouting" "shortestPath"
do
    ./run_experiment_set.sh \
        --prefix=sw_50_routers \
        --workload-prefix=sw_50_routers \
        --exp-type=circ \
        --routing-scheme=${scheme} \
        --num_start=0 --num_end=4 \
        --balance-list="100 200 400 800 1600 3200" \
        --path-choice=shortest \
        --num-paths=4 \
        --demand-scale="3" \
        --scheduling-alg=LIFO &
done
wait
