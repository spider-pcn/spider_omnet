This is a simulator that models the flow of transactions through a payment channel network. 

## Installation and Setup
To setup, first follow the omnet installation instructions from [Omnet Installation Link](https://omnetpp.org/doc/omnetpp/InstallGuide.pdf).
Following this, clone this repo in the `samples` directory of the omnet installation.

Then, run the following commands:
```
cd samples/spider_omnet/
make
omnetpp
```

This should open the Omnet IDE and you can click on the `simpleNet.ned` file and run the OmnetSimulation from there.

Once you have completed installation, navigate to the folder with setenv (should be first level inside the omnet simulation folder) and run 
```
. setenv
omnetpp
```
to save the time in rebuilding the omnet setup.

## Running the Simulation
Open the simpleNet.ned file and click the green run button to run the simulation.

## Collecting Statistics and Plotting Graphs
Click the button to run the simulation. For just the purpose of collection statistics, you can use the EXPRESS setting (triple arrow) to run through the simulation. Once the simulation has terminated, close the simulation window. 

Inside the omnet IDE, open the results folder. The statistics of the most recent run will be saved in the "simpleNet-#0.vec" file. To view the file, rename the .vec file to something unique, for example "figure4b-time10.vec". Double click the renamed file, which will prompt you to generate a .anf file. Click Finish.

In the .anf file, click Browse Data on the bottom toolbar. The individual nodes are in the expanded view under the folder "simpleNet: #0". Expand a node to see the statistics collected for that node. Right click on a node, folder, or statistic to plot it.

### Tips
- The third white drop down menu next to the blue arrow can filter by statistic.
- If you want to filter by more than one statistic, use the funnel button next to the blue arrow, and query for example:
```
name("numAttemptedPerDest_Total(dest *") name("numCompletedPerDest_Total(dest *")
``` 
- For aesthetic graphs, you need to right click -> properties, add titles and axis under "Titles", check display legend under "Legend"

