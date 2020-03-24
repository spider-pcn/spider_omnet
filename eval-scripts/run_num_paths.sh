#!/bin/bash

# small world 
for num_paths in 1 2 4 8
do
    ./run_experiments.sh \
        --prefix=sw_50_routers_lndCap \
        --workload-prefix=sw_50_routers \
        --routing-scheme=DCTCPQ \
        --num_start=0 --num_end=4 \
        --balance-list="900 1350 2750 4000 8750" \
        --path-choice=widest \
        --num-paths=${num_paths} \
        --scheduling-alg=LIFO &
done
wait

# scale free 
for num_paths in 1 2 4 8
do
    ./run_experiments.sh \
        --prefix=sf_50_routers_lndCap \
        --workload-prefix=sf_50_routers \
        --routing-scheme=DCTCPQ \
        --num_start=0 --num_end=4 \
        --balance-list="900 1350 2750 4000 8750" \
        --path-choice=widest \
        --num-paths=${num_paths} \
        --scheduling-alg=LIFO &
done
wait

# lnd 
for num_paths in 1 2 4 8
do
    ./run_experiments.sh \
        --prefix=lndnewtopo \
        --workload-prefix=lndnewtopo \
        --routing-scheme=DCTCPQ \
        --num_start=0 --num_end=4 \
        --balance-list="10 20 40 80 160" \
        --path-choice=widest \
        --num-paths=${num_paths} \
        --scheduling-alg=LIFO &
done
wait
