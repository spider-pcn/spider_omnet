#!/bin/bash
# NOTE: while a single OMNET experiment uses a single core,
# the memory consumption of each one of these runs can be on the order of 
# 100 GB for LND topology
# this script assumes you can run the schemes in parallel, but precisely one 
# experiment per scheme 
# (in total there are 5 balances * 5 circulations = 25 experiments per scheme)
# Do not use this script as is, without first judging the capabilities of
# your system to run experiments in parallel

# LND topo, kaggle data 

# LND balances
# make sure widest paths are generated
# Figures 9-11 of https://people.csail.mit.edu/vibhaa/files/spider_nsdi.pdf
for scheme in "DCTCPQ" "waterfilling" "lndBaseline" "landmarkRouting" "shortestPath"
do
    ./run_experiment_set.sh \
        --prefix=lnd_july15_2019 \
        --workload-prefix=lnd_july15_2019 \
        --exp-type=circ \
        --routing-scheme=${scheme} \
        --num_start=0 --num_end=4 \
        --balance-list="10 20 40 80 160" \
        --path-choice=widest \
        --num-paths=4 \
        --demand-scale="3" \
        --scheduling-alg=LIFO &
done
wait

# uniform balances
# Figures 22-23 of https://people.csail.mit.edu/vibhaa/files/spider_nsdi.pdf
for scheme in "DCTCPQ" "waterfilling" "lndBaseline" "landmarkRouting" "shortestPath"
do
    ./run_experiment_set.sh \
        --prefix=lnd_uniform \
        --workload-prefix=lnd_july15_2019 \
        --exp-type=circ \
        --routing-scheme=${scheme} \
        --num_start=0 --num_end=4 \
        --balance-list="10 20 40 80 160" \
        --path-choice=shortest \
        --num-paths=4 \
        --demand-scale="3" \
        --scheduling-alg=LIFO &
done
wait
