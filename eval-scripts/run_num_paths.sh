#!/bin/bash
# Figure 16 of https://people.csail.mit.edu/vibhaa/files/spider_nsdi.pdf
# NOTE: while a single OMNET experiment uses a single core,
# the memory consumption of each one of these runs can be on the order of 
# 50 GB for small world/ scale free and 100 GB for the LND topology
# Do not use this script as is, without first judging the capabilities of
# your system to run experiments in parallel

# small world topology, kaggle data for transactions
# make sure widest paths are avilable first for all topologies
for num_paths in 1 2 4 8
do
    ./run_experiment_set.sh \
        --prefix=sw_50_routers_lndCap \
        --workload-prefix=sw_50_routers \
        --exp-type=circ \
        --routing-scheme=DCTCPQ \
        --num_start=0 --num_end=4 \
        --balance-list="4000" \
        --path-choice=widest \
        --num-paths=${num_paths} \
        --demand-scale="3" \
        --scheduling-alg=LIFO &
done
wait

# scale free topology, kaggle transaction data
for num_paths in 1 2 4 8
do
    ./run_experiment_set.sh \
        --prefix=sf_50_routers_lndCap \
        --workload-prefix=sf_50_routers \
        --exp-type=circ \
        --routing-scheme=DCTCPQ \
        --num_start=0 --num_end=4 \
        --balance-list="4000" \
        --path-choice=widest \
        --num-paths=${num_paths} \
        --demand-scale="3" \
        --scheduling-alg=LIFO &
done
wait

# lnd topology, kaggle transaction data
for num_paths in 1 2 4 8
do
    ./run_experiment_set.sh \
        --prefix=lndnewtopo \
        --workload-prefix=lndnewtopo \
        --exp-type=circ \
        --routing-scheme=DCTCPQ \
        --num_start=0 --num_end=4 \
        --balance-list="40" \
        --path-choice=widest \
        --num-paths=${num_paths} \
        --demand-scale="3" \
        --scheduling-alg=LIFO &
done
wait
