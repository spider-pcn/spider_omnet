simple hostNode
{
	parameters:
	    bool isHost = true;
	    
		string topologyFile = default("sample-topology.txt");
		string workloadFile = default("sample-workload.txt");
<<<<<<< HEAD
		
		@signal[completionTime](type="long"); 
		@statistic[completionTime](title="completionTime"; source="completionTime"; record=vector,stats; interpolationmode=none); 
=======
		bool timeoutEnabled = default(true);
		bool waterfillingEnabled = default(true);
		
		@signal[completionTime](type="long"); 
		@statistic[completionTime](title="completionTime"; source="completionTime"; record=vector,stats; interpolationmode=none); 
		
		@signal[numCleared](type="long"); 
		@statistic[numCleared](title="numCleared"; source="numCleared"; record=vector,stats; interpolationmode=none); 
		
>>>>>>> water-filling-timeouts

		@display("i=block/routing"); 

		@signal[numInQueuePerChannel*](type="long"); // note an asterisk and the type of emitted values  
		@statisticTemplate[numInQueuePerChannelTemplate](record=vector,stats);   

		@signal[numProcessedPerChannel*](type="long");  
		@statisticTemplate[numProcessedPerChannelTemplate](record=vector, stats);   

		@signal[numSentPerChannel*](type="long");  
		@statisticTemplate[numSentPerChannelTemplate](record=vector, stats);   

		@signal[numCompletedPerDest*](type="long");  
		@statisticTemplate[numCompletedPerDestTemplate](record=vector, stats);   
<<<<<<< HEAD

=======
		
		@signal[fracSuccessfulPerDest*](type="long");  
		@statisticTemplate[fracSuccessfulPerDestTemplate](record=vector, stats);   
		
		@signal[pathPerTransPerDest*](type="long");  
		@statisticTemplate[pathPerTransPerDestTemplate](record=vector, stats);   
		
		@signal[numTimedOutPerDest*](type="long");  
		@statisticTemplate[numTimedOutPerDestTemplate](record=vector, stats);
		
>>>>>>> water-filling-timeouts
		@signal[numAttemptedPerDest*](type="long"); 
		@statisticTemplate[numAttemptedPerDestTemplate](record=vector, stats);   

		@signal[balancePerChannel*](type="long"); 
		@statisticTemplate[balancePerChannelTemplate](record=vector, stats);   
<<<<<<< HEAD
=======
		
		@signal[bottleneckPerDestPerPath*](type="long"); 
		@statisticTemplate[bottleneckPerDestPerPathTemplate](record=vector, stats);
		
		@signal[probeBackPerDestPerPath*](type="long"); 
		@statisticTemplate[probeBackPerDestPerPathTemplate](record=vector, stats);
		
		@signal[rateCompletedPerDestPerPath*](type="long"); 
		@statisticTemplate[rateCompletedPerDestPerPathTemplate](record=vector, stats);
		
		@signal[rateAttemptedPerDestPerPath*](type="long"); 
		@statisticTemplate[rateAttemptedPerDestPerPathTemplate](record=vector, stats);
>>>>>>> water-filling-timeouts

	gates:
		input in[];
		output out[];
}


