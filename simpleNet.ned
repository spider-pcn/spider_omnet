simple routerNode
{
	parameters:
		@signal[completionTime](type="long"); 
		@statistic[completionTime](title="completionTime"; source="completionTime"; record=vector,stats; interpolationmode=none); 

		@display("i=block/routing"); 

		@signal[numInQueuePerChannel*](type="long"); // note an asterisk and the type of emitted values  
		@statisticTemplate[numInQueuePerChannelTemplate](record=vector,stats);   

		@signal[numProcessedPerChannel*](type="long");  
		@statisticTemplate[numProcessedPerChannelTemplate](record=vector, stats);   

		@signal[numSentPerChannel*](type="long");  
		@statisticTemplate[numSentPerChannelTemplate](record=vector, stats);   

		@signal[numCompletedPerDest*](type="long");  
		@statisticTemplate[numCompletedPerDestTemplate](record=vector, stats);   

		@signal[numAttemptedPerDest*](type="long"); 
		@statisticTemplate[numAttemptedPerDestTemplate](record=vector, stats);   

		@signal[balancePerChannel*](type="long"); 
		@statisticTemplate[balancePerChannelTemplate](record=vector, stats);   

	gates:
		input in[];
		output out[];
}

network simpleNet
{
	parameters:
		double linkDelay @unit("s") = default(100us);
		double linkDataRate @unit("Gbps") = default(1Gbps);
	submodules:
		node[3]: routerNode {} 
 connections: 
		node[0].out++ --> {delay = 30.0ms; } --> node[1].in++;  
		node[0].in++ <-- {delay = 30.0ms; } <-- node[1].out++;  
		node[1].out++ --> {delay = 30.0ms; } --> node[2].in++;  
		node[1].in++ <-- {delay = 30.0ms; } <-- node[2].out++;  
}
