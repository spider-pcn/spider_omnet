#!/bin/bash
# Figures 12, 25 of https://people.csail.mit.edu/vibhaa/files/spider_nsdi.pdf
# NOTE: while a single OMNET experiment uses a single core,
# the memory consumption of each one of these runs can be on the order of 
# 50 GB for small world/ scale free and 100 GB for the LND topology
# Do not use this script as is, without first judging the capabilities of
# your system to run experiments in parallel

# LND topology, kaggle data
# make sure widest paths are generated for the 40 point for LND, 4000 for synthetic
for scheme in "DCTCPQ" "waterfilling" "lndBaseline" "landmarkRouting" "shortestPath"
do
    ./run_experiment_set.sh \
        --prefix=lnd_july15_2019 \
        --workload-prefix=lnd_july15_2019 \
        --exp-type=dag \
        --routing-scheme=${scheme} \
        --num_start=0 --num_end=4 \
        --balance-list="40" \
        --dag-percent-list="20 45 65" \
        --path-choice=widest \
        --num-paths=4 \
        --demand-scale="3" \
        --scheduling-alg=LIFO &
done
wait

# small world topology, kaggle data for transactions
for scheme in "DCTCPQ" "waterfilling" "lndBaseline" "landmarkRouting" "shortestPath"
do
    ./run_experiment_set.sh \
        --prefix=sw_50_routers_lndCap \
        --workload-prefix=sw_50_routers \
        --exp-type=dag \
        --routing-scheme=${scheme} \
        --num_start=0 --num_end=4 \
        --balance-list="4000" \
        --dag-percent-list="20 45 65" \
        --path-choice=widest \
        --num-paths=4 \
        --demand-scale="3" \
        --scheduling-alg=LIFO &
done
wait

# scale free topology, kaggle data for transactions
for scheme in "DCTCPQ" "waterfilling" "lndBaseline" "landmarkRouting" "shortestPath"
do
    ./run_experiment_set.sh \
        --prefix=sf_50_routers_lndCap \
        --workload-prefix=sf_50_routers \
        --exp-type=dag \
        --routing-scheme=${scheme} \
        --num_start=0 --num_end=4 \
        --balance-list="4000" \
        --dag-percent-list="20 45 65" \
        --path-choice=widest \
        --num-paths=4 \
        --demand-scale="3" \
        --scheduling-alg=LIFO &
done
wait
