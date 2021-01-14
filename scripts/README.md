#### Benchmark Generation
To generate the benchmarks, run one of the two commands depending on your needs.
```
    generate_circulation_benchmarks.sh
    generate_dag_benchmarks.sh
```
Make sure that the scheme that is run is "DCTCPQ" in order to reproduce Spider's algorithm. Some more examples of a cleaner experiment routine
can be found in the `refactor-scripts` branch.
These two basically go through all the types of graphs: really small ones to much larger ones and generate
the topology, workload, ini files and ned files for every one of them. The circulations are placed in 
`../benchmarks/circulations` and the dags are placed in `../benchmarks/dagx` where `x` denotes the dag percentage
which is also supplied in line 18 of the `generate_dag_benchmarks.sh`. If you want fewer topologies to be newly generated, tamper only with the `prefix` array.


#### Running benchmarks
To run a subset of these benchmarks, two sample scripts exist in the parent directory to this. It is important
you run them only from the parent directory since otherwise, the directory indexing gets messed with.
```
    run_circulations.sh
    run_dag.sh
```
They both ensure that the hostNed and routerNed files are supplied in the right directory and then run the 
spiderNet executable on appropriate workloads. The results generated go into the corresponding 
`benchmarks/x/results/` where `x` depends on whether it is a circulation or a dag. The filenames of the .vec files
are determined by the config that was run.

To run a particular topology, workload or config file (let's say its prefix is `sample` and it 
is a circulation workload), 
run this command from the parent directory to this directory,
```
./spiderNet -u Cmdenv -f benchmarks/circulations/sample.ini -c sample -n benchmarks/circulations
```
The results of this run will be in `benchmarks/circulations/results/sample-#0.vec`.


#### Visualizing CDFs
To visualize the CDFs and compare them across a few different runs, you can use the following command:
```
python generate_summary_cdf.py --vec_files ../benchmarks/circulations/results/wf/sample-#0.vec ../benchmarks/circulations/results/shortest/sample-#0.vec --labels "wf" "shortest"  --save 400_nodes_sw.pdf
```
The --vec_files specifies the vector files holding the results. Make sure to put the waterfilling run in a `wf` subfolder as of now, since that is how the plotting
script differnetiates between waterfilling and shortest paths. The --labels then mention the legend names and the
--save denotes the pdf file to plot to. Following this, the pdf file can be viewed/downloaded and so on.


#### Analyzing a single run similar to IDE
An example command to generate the analysis plots for the above described `sample` run would be:
```
python generate_analysis_plots_for_single_run.py --vec_file ../benchmarks/circulations/results/sample-#0.vec --balance --save sample --queue_info --timeouts --frac_completed
```
In particular, this generates two pdf files. The first will be called `sample_per_channel_info.pdf` which will contain the time series of the balances and the queue information across all payment channels. Every router has its own page and unique plot in this pdf with all of its payment channels on that plot. If you want more information other than the balances
and the queue, you can look at other flags supported or add your own and include support for parsing the right signals associated with it.

 The second will be called `sample_per_src_dest_info.pdf` which will contain the time series of the fraction of transactions completed and the number of transactions that timed out between every source and destination. Every host has its own page and unique plot in this pdf with all of its destinations on that plot. If you want more information other than the timeouts or fraction completed you can look at other flags supported or add your own and include support for parsing the right signals associated with it.




    
 
