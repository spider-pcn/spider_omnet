simple routerNode
{
	parameters:
	    bool isHost = false;
		string topologyFile = default("sample-topology.txt");
		string workloadFile = default("sample-workload.txt");
		bool timeoutEnabled = default(true);
		bool waterfillingEnabled = default(true);
		
		@signal[completionTime](type="long"); 
		@statistic[completionTime](title="completionTime"; source="completionTime"; record=vector,stats; interpolationmode=none); 

		@display("i=block/routing"); 

		@signal[numInQueuePerChannel*](type="long"); // note an asterisk and the type of emitted values  
		@statisticTemplate[numInQueuePerChannelTemplate](record=vector,stats);   

		@signal[numProcessedPerChannel*](type="long");  
		@statisticTemplate[numProcessedPerChannelTemplate](record=vector, stats);   

		@signal[numSentPerChannel*](type="long");  
		@statisticTemplate[numSentPerChannelTemplate](record=vector, stats);   
		
		@signal[balancePerChannel*](type="long"); 
		@statisticTemplate[balancePerChannelTemplate](record=vector, stats);   
		
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

	gates:
		input in[];
		output out[];
}


