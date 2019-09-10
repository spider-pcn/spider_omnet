simple routerNodeDCTCPBal
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

		@signal[numInQueuePerChannel*](type="double"); 
		@statisticTemplate[numInQueuePerChannelTemplate](record=vector,stats);   

		@signal[numProcessedPerChannel*](type="long");  
		@statisticTemplate[numProcessedPerChannelTemplate](record=vector, stats);   

		@signal[numSentPerChannel*](type="long");  
		@statisticTemplate[numSentPerChannelTemplate](record=vector, stats);   
		
		@signal[balancePerChannel*](type="double"); 
		@statisticTemplate[balancePerChannelTemplate](record=vector, stats);   
		
		@signal[timeInFlightPerChannel*](type="double"); 
		@statisticTemplate[timeInFlightPerChannelTemplate](record=vector, stats);   
		
		//price scheme payment channel specific signals
		@signal[nValuePerChannel*](type="double"); 
		@statisticTemplate[nValuePerChannelTemplate](record=vector, stats);
		
		@signal[fakeRebalanceQPerChannel*](type="double"); 
		@statisticTemplate[fakeRebalanceQPerChannelTemplate](record=vector, stats);
                
		@signal[queueDelayEWMAPerChannel*](type="double"); 
		@statisticTemplate[queueDelayEWMAPerChannelTemplate](record=vector, stats);
                
                @signal[serviceRatePerChannel*](type="double"); 
		@statisticTemplate[serviceRatePerChannelTemplate](record=vector, stats);
				
                @signal[arrivalRatePerChannel*](type="double"); 
		@statisticTemplate[arrivalRatePerChannelTemplate](record=vector, stats);
                
                @signal[inflightOutgoingPerChannel*](type="long"); 
		@statisticTemplate[inflightOutgoingPerChannelTemplate](record=vector, stats);

                @signal[inflightIncomingPerChannel*](type="long"); 
		@statisticTemplate[inflightIncomingPerChannelTemplate](record=vector, stats);
                
                @signal[xLocalPerChannel*](type="double"); 
		@statisticTemplate[xLocalPerChannelTemplate](record=vector, stats);

		@signal[lambdaPerChannel*](type="double"); 
		@statisticTemplate[lambdaPerChannelTemplate](record=vector, stats);
		
		@signal[muLocalPerChannel*](type="double"); 
		@statisticTemplate[muLocalPerChannelTemplate](record=vector, stats);
		
		@signal[muRemotePerChannel*](type="double"); 
		@statisticTemplate[muRemotePerChannelTemplate](record=vector, stats);
    		
                @signal[numInflightPerChannel*](type="double"); 
		@statisticTemplate[numInflightPerChannelTemplate](record=vector, stats);

		@signal[implicitRebalancingAmtPerChannel*](type="double"); 
		@statisticTemplate[implicitRebalancingAmtPerChannelTemplate](record=vector, stats);  
		@signal[explicitRebalancingAmtPerChannel*](type="double"); 
		@statisticTemplate[explicitRebalancingAmtPerChannelTemplate](record=vector, stats);  


	gates:
		input in[];
		output out[];
}


