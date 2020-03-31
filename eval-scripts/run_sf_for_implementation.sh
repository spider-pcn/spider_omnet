#!/bin/bash
# Figure 8 of https://people.csail.mit.edu/vibhaa/files/spider_nsdi.pdf
# NOTE: while a single OMNET experiment uses a single core,
# the memory consumption of each one of these runs can be on the order of 
# 5 GB for these 10 node scale free graphs 
# Do not use this script as is, without first judging the capabilities of
# your system to run experiments in parallel

# scale free
# no kaggle sizes for the data - just use unit size transactions to 
# parallel pre-split transactions for the implementation

for scheme in "DCTCPQ" "lndBaseline"
do
    ./run_experiment_set.sh \
        --prefix=sf_10_routers \
        --workload-prefix=sf_10_routers \
        --exp-type=impl \
        --routing-scheme=${scheme} \
        --num_start=0 --num_end=4 \
        --balance-list="25 50 100 200 400" \
        --path-choice=shortest \
        --num-paths=4 \
        --demand-scale=10 \
        --scheduling-alg=FIFO &
done
wait
