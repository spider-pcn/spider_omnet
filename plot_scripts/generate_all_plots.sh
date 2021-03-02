#!/bin/bash
dir="../nsdi/figures/"

#### design section plots
./plotRateQueueTimeSeries.R parallel_graph_DCTCPQ_timeseries_data parallel_graph_DCTCPQ
./plotRateQueueTimeSeries.R parallel_graph_TCP_timeseries_data parallel_graph_TCP
cp parallel_graph_*RateQueue.pdf ../nsdi/figures

# toy examples for DCTCP
./rateQueueFacet.R capacity
./rateQueueFacet.R balance
cp balanceRateQueue.pdf $dir
cp capacityRateQueue.pdf $dir

# distribution of balances and transaction data
echo "eval data distribution"
./plotLNDCapacityCDF.R
cp lndCapacityCDFJuly15min25.pdf $dir

./plotKaggleData.R
cp kaggleCDF.pdf $dir

### eval section
# split up performance by size of transactions
echo "size succ probability graphs"
./plotSizeSuccRatioFacet.R all_topo_all_credit_succ_prob_data all_topo_lnd_credit_succ_probability.pdf lnd
./plotSizeSuccRatioFacet.R all_topo_all_credit_succ_prob_data all_topo_uniform_credit_succ_probability.pdf uniform
cp all_topo_lnd_credit_succ_probability.pdf $dir
cp all_topo_uniform_credit_succ_probability.pdf $dir

# simulator fidelity
./plotImplSimulationGraphs.R impl_sf_10_data simulatorFidelity.pdf
cp simulatorFidelity.pdf $dir

# credit graphs
echo "Credit Graphs"
./plotThroughputFacet.R all_topo_all_credit_data all_topo_all_credit_facet
cp all_topo_all_credit_facetSuccRatio.pdf $dir
cp all_topo_all_credit_facet_lnd_credit_SuccRatio.pdf $dir
cp all_topo_all_credit_facet_uniform_credit_SuccRatio.pdf $dir

# latency plot
./plotSizeCompletionTime.R

# flow-level fairness graph
./plotFlowFairnessCDF.R all_topo_lnd_credit_fairness_data flowLevelFairness.pdf
cp flowLevelFairness.pdf $dir

# celer plot
./plotGraphsAtCredit.R sf_10_routers_celer_uniform_credit_data sf_10_routers_celer_comp
cp sf_10_routers_celer_compSummary.pdf $dir

# DAG graph
echo "DAG"
./plotGraphsAtDagPercent.R lndnewtopo_lnd_credit_dag_data lndnewtopo_lnd_credit_dag 42
cp lndnewtopo_lnd_credit_dagSummary.pdf $dir
./plotDagFacet.R all_topo_dag_data synthetic_topo_dag_throughput.pdf 42
cp synthetic_topo_dag_throughput.pdf $dir

# deadlock example
./tptTimeSeriesFacet.R
cp dagThroughputTimeseries.pdf $dir

# rebalancing graphs
./plotRebalancingFacet.R synthetic_topo_lnd_credit_rebalancing_data synthetic_topo_rebalancing_throughput.pdf
cp synthetic_topo_rebalancing_throughput.pdf $dir

./plotRebalancingGraphs.R lndnewtopo_lnd_credit_rebalancing_data lndnewtopo_lnd_credit_rebalancing lndnewtopo_lnd_credit_rebalancing_offload_data 
cp lndnewtopo_lnd_credit_rebalancingSummary.pdf ../nsdi/figures

# path choices effect
echo "type of paths"
./barPlotPathTypes.R typesOfPathsData typeOfPathsEffect.pdf
cp typeOfPathsEffect.pdf $dir

echo "scheduling algorithms"
./barPlotSchedulingAlg.R schedulingAlgData schedulingAlgEffect.pdf
cp schedulingAlgEffect.pdf $dir

# num paths effect
echo "number of paths"
./barPlotNumPaths.R numPathsData numberOfPathsEffect.pdf
cp numberOfPathsEffect.pdf $dir
