#!/bin/bash
# NOTE: while a single OMNET experiment uses a single core,
# the memory consumption of each one of these runs can be on the order of 
# 5 GB for these 10 node scale free graphs 
# Do not use this script as is, without first judging the capabilities of
# your system to run experiments in parallel

# scale free, kaggle size data

for scheme in "DCTCPQ" "celer"
do
    ./run_experiment_set.sh \
        --prefix=sf_10_routers \
        --workload-prefix=sf_10_routers \
        --exp-type=celer \
        --routing-scheme=${scheme} \
        --num_start=0 --num_end=4 \
        --balance-list="200 400 800 1600 3200 6400" \
        --path-choice=shortest \
        --num-paths=4 \
        --demand-scale=3 \
        --scheduling-alg=LIFO &
done
wait
