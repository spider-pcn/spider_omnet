simple hostNodeLndBaseline
{
	parameters:
	    bool isHost = true;
	    
		string topologyFile = default("sample-topology.txt");
		string workloadFile = default("sample-workload.txt");
                string resultPrefix = default("");

		double simulationLength = default(30.0);
		double statRate = default(0.2);
		bool signalsEnabled = default(false);
		bool loggingEnabled = default(false);
                double splitSize = default(5.0);

		bool timeoutEnabled = default(false);
		double timeoutClearRate = default(0.5);

		bool celerEnabled = default(false);
		bool waterfillingEnabled = default(false);
		bool priceSchemeEnabled = default(false);
		bool smoothWaterfillingEnabled = default(false);
                bool windowEnabled = default(false);
                bool landmarkRoutingEnabled = default(false);
                bool lndBaselineEnabled = default(false);
                bool dctcpEnabled = default(false);
		int numPathChoices = default(4);

                bool widestPathsEnabled = default(false);
                bool heuristicPathsEnabled = default(false);
                bool obliviousRoutingEnabled = default(false);
                bool kspYenEnabled = default(false);
                
                bool LIFOEnabled = default(false);
                bool FIFOEnabled = default(false);
                bool SPFEnabled = default(false);
                bool RREnabled = default(false);
                bool EDFEnabled = default(false);
                
                bool changingPathsEnabled = default(false);
                int maxPathsToConsider = default(100);
                double windowThresholdForChange = default(1);
                double pathMonitorRate = default(5);

                bool rebalancingEnabled = default(false);
                double queueDelayThreshold = default(3.0);
                double gamma = default(1.0);
                double gammaImbalanceQueueSize = default(5.0);
                double rebalancingRate = default(10.0);
                double rebalancingDelayForAddingFunds = default(0.1);
                double minBalanceComputeRate = default(0.1);

                double alpha = default(0.01);
                double kappa = default(0.01);
                double eta = default(0.01);
                double zeta = default(0.01);
                double rhoValue = default(0.05);
                double minRate = default(0.25);
                double updateQueryTime = default(0.8);
                bool splittingEnabled = default(false);
                double xi = default(1);
                int serviceArrivalWindow = default(100);
                double routerQueueDrainTime = default(1);

                double transStatStart = default(3000);
                double transStatEnd = default(5000);
                
                double tau = default(10);
                double normalizer = default(100);

		
		@signal[completionTime](type="long"); 
		@statistic[completionTime](title="completionTime"; source="completionTime"; record=vector,stats; interpolationmode=none); 
		
		@signal[numCleared](type="long"); 
		@statistic[numCleared](title="numCleared"; source="numCleared"; record=vector,stats; interpolationmode=none); 
		
		@display("i=block/routing"); 

		@signal[numInQueuePerChannel*](type="long"); // note an asterisk and the type of emitted values  
                @signal[bankPerChannel*](type="double"); 
		@statisticTemplate[bankPerChannelTemplate](record=vector, stats); 
		@statisticTemplate[numInQueuePerChannelTemplate](record=vector,stats);   

		@signal[numProcessedPerChannel*](type="long");  
		@statisticTemplate[numProcessedPerChannelTemplate](record=vector, stats);  

 		@signal[probabilityPerDestPerPath*](type="double");  
		@statisticTemplate[probabilityPerDestPerPathTemplate](record=vector, stats);  

		@signal[numSentPerChannel*](type="long");  
		@statisticTemplate[numSentPerChannelTemplate](record=vector, stats);   

		@signal[rateCompletedPerDest*](type="long");  
		@statisticTemplate[rateCompletedPerDestTemplate](record=vector, stats);   
		
		@signal[numCompletedPerDest*](type="long");  
		@statisticTemplate[numCompletedPerDestTemplate](record=vector, stats);   

		@signal[numPendingPerDest*](type="long");  
		@statisticTemplate[numPendingPerDestTemplate](record=vector, stats); 

 		@signal[numWaitingPerDest*](type="long");  
		@statisticTemplate[numWaitingPerDestTemplate](record=vector, stats);  
	
		@signal[fracSuccessfulPerDest*](type="double");  
		@statisticTemplate[fracSuccessfulPerDestTemplate](record=vector, stats);   
		
		@signal[pathPerTransPerDest*](type="long");  
		@statisticTemplate[pathPerTransPerDestTemplate](record=vector, stats);   
		
		@signal[numTimedOutPerDest*](type="long");  
		@statisticTemplate[numTimedOutPerDestTemplate](record=vector, stats);

		@signal[demandEstimatePerDest*](type="double");  
		@statisticTemplate[demandEstimatePerDestTemplate](record=vector, stats);

		@signal[numTimedOutAtSenderPerDest*](type="long");  
		@statisticTemplate[numTimedOutAtSenderPerDestTemplate](record=vector, stats);
	
        	@signal[rateAttemptedPerDest*](type="long"); 
		@statisticTemplate[rateAttemptedPerDestTemplate](record=vector, stats);   
		
                @signal[rateArrivedPerDest*](type="long"); 
		@statisticTemplate[rateArrivedPerDestTemplate](record=vector, stats);  
	
		@signal[numArrivedPerDest*](type="long"); 
		@statisticTemplate[numArrivedPerDestTemplate](record=vector, stats);   

		@signal[balancePerChannel*](type="long"); 
		@statisticTemplate[balancePerChannelTemplate](record=vector, stats);   
		
		@signal[capacityPerChannel*](type="double"); 
		@statisticTemplate[capacityPerChannelTemplate](record=vector, stats);   
		
		@signal[implicitRebalancingAmtPerChannel*](type="double"); 
		@statisticTemplate[implicitRebalancingAmtPerChannelTemplate](record=vector, stats);  
		@signal[explicitRebalancingAmtPerChannel*](type="double"); 
		@statisticTemplate[explicitRebalancingAmtPerChannelTemplate](record=vector, stats);  
		
		@signal[timeInFlightPerChannel*](type="double"); 
		@statisticTemplate[timeInFlightPerChannelTemplate](record=vector, stats);   
		
		@signal[bottleneckPerDestPerPath*](type="long"); 
		@statisticTemplate[bottleneckPerDestPerPathTemplate](record=vector, stats);
		
                @signal[numInflightPerChannel*](type="unsigned long"); 
		@statisticTemplate[numInflightPerChannelTemplate](record=vector, stats);
		
                @signal[probeBackPerDestPerPath*](type="long"); 
		@statisticTemplate[probeBackPerDestPerPathTemplate](record=vector, stats);
		
		@signal[rateCompletedPerDestPerPath*](type="long"); 
		@statisticTemplate[rateCompletedPerDestPerPathTemplate](record=vector, stats);
		
		@signal[rateAttemptedPerDestPerPath*](type="long"); 
		@statisticTemplate[rateAttemptedPerDestPerPathTemplate](record=vector, stats);
		
		//has queue capacity specific signals
		@signal[numFailedPerDest*](type="long");  
		@statisticTemplate[numFailedPerDestTemplate](record=vector, stats);
		
		@signal[rateFailedPerDest*](type="long");  
		@statisticTemplate[rateFailedPerDestTemplate](record=vector, stats);
		
		//price scheme payment channel specific signals
		@signal[nValuePerChannel*](type="long"); 
		@statisticTemplate[nValuePerChannelTemplate](record=vector, stats);
		
		@signal[xLocalPerChannel*](type="double"); 
		@statisticTemplate[xLocalPerChannelTemplate](record=vector, stats);

		@signal[lambdaPerChannel*](type="double"); 
		@statisticTemplate[lambdaPerChannelTemplate](record=vector, stats);
		
		@signal[muLocalPerChannel*](type="double"); 
		@statisticTemplate[muLocalPerChannelTemplate](record=vector, stats);
		
		@signal[muRemotePerChannel*](type="double"); 
		@statisticTemplate[muRemotePerChannelTemplate](record=vector, stats);
		
		//price scheme path specific signals
		@signal[rateToSendTransPerDestPerPath*](type="double"); 
		@statisticTemplate[rateToSendTransPerDestPerPathTemplate](record=vector, stats);
		
		@signal[timeToNextSendPerDestPerPath*](type="simtime_t"); 
		@statisticTemplate[timeToNextSendPerDestPerPathTemplate](record=vector, stats);
		
		@signal[sumOfTransUnitsInFlightPerDestPerPath*](type="double"); 
		@statisticTemplate[sumOfTransUnitsInFlightPerDestPerPathTemplate](record=vector, stats);
		
		@signal[priceLastSeenPerDestPerPath*](type="double"); 
		@statisticTemplate[priceLastSeenPerDestPerPathTemplate](record=vector, stats);
		
		@signal[isSendTimerSetPerDestPerPath*](type="long"); 
		@statisticTemplate[isSendTimerSetPerDestPerPathTemplate](record=vector, stats);
		
		@signal[numPathsPerTransPerDest*](type="long");  
		@statisticTemplate[numPathsPerTransPerDestTemplate](record=vector, stats);   
	
        gates:
		input in[];
		output out[];
}


