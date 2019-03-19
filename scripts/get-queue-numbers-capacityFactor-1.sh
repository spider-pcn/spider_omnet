for demand in 1 3 6
do
    python parse_vec_files_queueLines.py /home/ubuntu/omnetpp-5.4.1/samples/spider_omnet/benchmarks/circulations/results/capacity-factor-1-queue-1000/sw_50_routers_circ_net_priceScheme_demand${demand}_shortest_4-#0.vec $demand woWin 3000 5000 figures/timeouts/capacity-factor-1-queue-1000/queue-sw50-kaggle-demand${demand}-woWin-capacityFactor1.txt
    #python parse_vec_files_queueLines.py /home/ubuntu/omnetpp-5.4.1/samples/spider_omnet/benchmarks/circulations/results/capacity-factor-1-queue-1000/sw_50_routers_circ_net_priceSchemeWindow_demand${demand}_shortest_4-#0.vec $demand regular 3000 5000 figures/timeouts/capacity-factor-1-queue-1000/queue-sw50-kaggle-demand${demand}-regular-capacityFactor1.txt
    #python parse_vec_files_queueLines.py /home/ubuntu/omnetpp-5.4.1/samples/spider_omnet/benchmarks/circulations/results/capacity-factor-1-queue-1000/sw_50_routers_circ_net_priceSchemeWindowNoQueue_demand${demand}_shortest_4-#0.vec $demand woQ 3000 5000 figures/timeouts/capacity-factor-1-queue-1000/queue-sw50-kaggle-demand${demand}-woQ-capacityFactor1.txt
done

for demand in 9
do
    #python parse_vec_files_queueLines.py /home/ubuntu/omnetpp-5.4.1/samples/spider_omnet/benchmarks/circulations/results/capacity-factor-1-queue-1000/sw_50_routers_circ_net_priceScheme_demand${demand}_shortest_4-#0.vec $demand woWin 3000 5000 figures/timeouts/capacity-factor-1-queue-1000/queue-sw50-kaggle-demand${demand}-woWin-capacityFactor1.txt
    #python parse_vec_files_queueLines.py /home/ubuntu/omnetpp-5.4.1/samples/spider_omnet/benchmarks/circulations/results/capacity-factor-1-queue-1000/sw_50_routers_circ_net_priceSchemeWindow_demand${demand}_shortest_4-#0.vec $demand regular 3000 5000 figures/timeouts/capacity-factor-1-queue-1000/queue-sw50-kaggle-demand${demand}-regular-capacityFactor1.txt
    python parse_vec_files_queueLines.py /home/ubuntu/omnetpp-5.4.1/samples/spider_omnet/benchmarks/circulations/results/capacity-factor-1-queue-1000/sw_50_routers_circ_net_priceSchemeWindowNoQueue_demand${demand}_shortest_4-#0.vec $demand woQ 3000 5000 figures/timeouts/capacity-factor-1-queue-1000/queue-sw50-kaggle-demand${demand}-woQ-capacityFactor1.txt
done

