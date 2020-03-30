#!/bin/bash
# NOTE: while a single OMNET experiment uses a single core,
# the memory consumption of each one of these runs can be on the order of 
# 50 GB for small world/ scale free and 100 GB for the LND topology
# Do not use this script as is, without first judging the capabilities of
# your system to run experiments in parallel

# small world topology, kaggle data for transactions
# make sure widest paths are avilable first for all topologues
for alg in "FIFO" "SPF" "EDF" #"LIFO"
do
    ./run_experiment_set.sh \
        --prefix=sw_50_routers_lndCap \
        --workload-prefix=sw_50_routers \
        --exp-type=circ \
        --routing-scheme=DCTCPQ \
        --num_start=0 --num_end=4 \
        --balance-list="900 1350 2750 4000 8750" \
        --path-choice=widest \
        --num-paths=4 \
        --demand-scale="3" \
        --scheduling-alg=${alg} &
done
wait

# scale free topology, kaggle transaction data
for alg in "FIFO" "SPF" "EDF" #"LIFO"
do
    ./run_experiment_set.sh \
        --prefix=sf_50_routers_lndCap \
        --workload-prefix=sf_50_routers \
        --exp-type=circ \
        --routing-scheme=DCTCPQ \
        --num_start=0 --num_end=4 \
        --balance-list="900 1350 2750 4000 8750" \
        --path-choice=widest \
        --num-paths=4 \
        --demand-scale="3" \
        --scheduling-alg=${alg} &
done
wait

# lnd topology, kaggle transaction data
for alg in "FIFO" "SPF" "EDF" #"LIFO"
do
    ./run_experiment_set.sh \
        --prefix=lndnewtopo \
        --workload-prefix=lndnewtopo \
        --exp-type=circ \
        --routing-scheme=DCTCPQ \
        --num_start=0 --num_end=4 \
        --balance-list="10 20 40 80 160" \
        --path-choice=widest \
        --num-paths=4 \
        --demand-scale="3" \
        --scheduling-alg=${alg} &
done
wait
