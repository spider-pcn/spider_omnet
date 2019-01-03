simple hostNode
{
	parameters:
	    bool isHost = true;
	    
		string topologyFile = default("sample-topology.txt");
		string workloadFile = default("sample-workload.txt");
		
		@signal[completionTime](type="long"); 
		@statistic[completionTime](title="completionTime"; source="completionTime"; record=vector,stats; interpolationmode=none); 
		
		@signal[numCleared](type="long"); 
		@statistic[numCleared](title="numCleared"; source="numCleared"; record=vector,stats; interpolationmode=none); 
		

		@display("i=block/routing"); 

		@signal[numInQueuePerChannel*](type="long"); // note an asterisk and the type of emitted values  
		@statisticTemplate[numInQueuePerChannelTemplate](record=vector,stats);   

		@signal[numProcessedPerChannel*](type="long");  
		@statisticTemplate[numProcessedPerChannelTemplate](record=vector, stats);   

		@signal[numSentPerChannel*](type="long");  
		@statisticTemplate[numSentPerChannelTemplate](record=vector, stats);   

		@signal[numCompletedPerDest*](type="long");  
		@statisticTemplate[numCompletedPerDestTemplate](record=vector, stats);   
		
		@signal[fracSuccessfulPerDest*](type="long");  
		@statisticTemplate[fracSuccessfulPerDestTemplate](record=vector, stats);   
		
		@signal[numTimedOutPerDest*](type="long");  
		@statisticTemplate[numTimedOutPerDestTemplate](record=vector, stats);
		
		@signal[numAttemptedPerDest*](type="long"); 
		@statisticTemplate[numAttemptedPerDestTemplate](record=vector, stats);   

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

	gates:
		input in[];
		output out[];
}


