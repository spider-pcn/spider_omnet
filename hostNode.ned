simple hostNode
{
	parameters:
	    bool isHost = true;
	    
		string topologyFile = default("sample-topology.txt");
		string workloadFile = default("sample-workload.txt");

		double simulationLength = default(30.0);
		double statRate = default(0.2);
		bool signalsEnabled = default(false);
		bool loggingEnabled = default(false);

		bool timeoutEnabled = default(true);
		double timeoutClearRate = default(0.5);

		bool waterfillingEnabled = default(false);
		bool priceSchemeEnabled = default(false);
		bool smoothWaterfillingEnabled = default(false);
		int numPathChoices = default(4);

		
		@signal[completionTime](type="long"); 
		@statistic[completionTime](title="completionTime"; source="completionTime"; record=vector,stats; interpolationmode=none); 
		
		@signal[numCleared](type="long"); 
		@statistic[numCleared](title="numCleared"; source="numCleared"; record=vector,stats; interpolationmode=none); 
		
		@display("i=block/routing"); 

		@signal[numInQueuePerChannel*](type="long"); // note an asterisk and the type of emitted values  
		@statisticTemplate[numInQueuePerChannelTemplate](record=vector,stats);   

		@signal[numProcessedPerChannel*](type="long");  
		@statisticTemplate[numProcessedPerChannelTemplate](record=vector, stats);  

 		@signal[probabilityPerDest*](type="double");  
		@statisticTemplate[probabilityPerDestTemplate](record=vector, stats);  

		@signal[numSentPerChannel*](type="long");  
		@statisticTemplate[numSentPerChannelTemplate](record=vector, stats);   

		@signal[rateCompletedPerDest*](type="long");  
		@statisticTemplate[rateCompletedPerDestTemplate](record=vector, stats);   
		
		@signal[numCompletedPerDest*](type="long");  
		@statisticTemplate[numCompletedPerDestTemplate](record=vector, stats);   

		@signal[numPendingPerDest*](type="long");  
		@statisticTemplate[numPendingPerDestTemplate](record=vector, stats);  
	
		@signal[fracSuccessfulPerDest*](type="long");  
		@statisticTemplate[fracSuccessfulPerDestTemplate](record=vector, stats);   
		
		@signal[pathPerTransPerDest*](type="long");  
		@statisticTemplate[pathPerTransPerDestTemplate](record=vector, stats);   
		
		@signal[numTimedOutPerDest*](type="long");  
		@statisticTemplate[numTimedOutPerDestTemplate](record=vector, stats);

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
		
		@signal[bottleneckPerDestPerPath*](type="long"); 
		@statisticTemplate[bottleneckPerDestPerPathTemplate](record=vector, stats);
		
		@signal[probeBackPerDestPerPath*](type="long"); 
		@statisticTemplate[probeBackPerDestPerPathTemplate](record=vector, stats);
		
		@signal[rateCompletedPerDestPerPath*](type="long"); 
		@statisticTemplate[rateCompletedPerDestPerPathTemplate](record=vector, stats);
		
		@signal[rateAttemptedPerDestPerPath*](type="long"); 
		@statisticTemplate[rateAttemptedPerDestPerPathTemplate](record=vector, stats);
		
		//price scheme payment channel specific signals
		@signal[nValuePerChannel*](type="long"); 
		@statisticTemplate[nValuePerChannelTemplate](record=vector, stats);
		
		@signal[xLocalPerChannel*](type="long"); 
		@statisticTemplate[xLocalPerChannelTemplate](record=vector, stats);

		@signal[lambdaPerChannel*](type="long"); 
		@statisticTemplate[lambdaPerChannelTemplate](record=vector, stats);
		
		@signal[muLocalPerChannel*](type="long"); 
		@statisticTemplate[muLocalPerChannelTemplate](record=vector, stats);
		
		@signal[muRemotePerChannel*](type="long"); 
		@statisticTemplate[muRemotePerChannelTemplate](record=vector, stats);
		
		//price scheme path specific signals
		@signal[rateToSendTransPerPath*](type="long"); 
		@statisticTemplate[rateToSendTransPerPathTemplate](record=vector, stats);
		
		@signal[timeToNextSendPerPath*](type="long"); 
		@statisticTemplate[timeToNextSendPerPathTemplate](record=vector, stats);
		
		@signal[sumOfTransUnitsInFlightPerPath*](type="long"); 
		@statisticTemplate[sumOfTransUnitsInFlightPerPathTemplate](record=vector, stats);
		
		@signal[priceLastUpdatedPerPath*](type="long"); 
		@statisticTemplate[priceLastUpdatedPerPathTemplate](record=vector, stats);
		
		@signal[isSendTimerSetPerPath*](type="long"); 
		@statisticTemplate[isSendTimerSetPerPathTemplate](record=vector, stats);
		
		
		

	gates:
		input in[];
		output out[];
}


