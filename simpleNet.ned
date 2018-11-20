simple routerNode
{
	parameters:
		@signal[numInQueue](type="long"); 
		@statistic[numInQueue](title="total num in queue"; source="numInQueue"; record=vector,stats; interpolationmode=none); 

		@signal[numProcessed](type="long"); 
		@statistic[numProcessed](title="num processing events in statRate time"; source="numProcessed"; record=vector,stats; interpolationmode=none); 

		@signal[completionTime](type="long"); 
		@statistic[completionTime](title="completionTime"; source="completionTime"; record=vector,stats; interpolationmode=none); 

		@display("i=block/routing"); 
		
		@signal[numInQueuePerChannel*](type="long"); // note an asterisk and the type of emitted values
		@statisticTemplate[numInQueuePerChannelTemplate](record=vector,stats);

		@signal[numProcessedPerChannel*](type="long"); // note an asterisk and the type of emitted values
		@statisticTemplate[numProcessedPerChannelTemplate](record=vector, stats);
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
		node[5]: routerNode {} 
 connections: 
		node[0].out++ --> {delay = 30.0ms; } --> node[4].in++;  
		node[0].in++ <-- {delay = 30.0ms; } <-- node[4].out++;  
		node[0].out++ --> {delay = 30.0ms; } --> node[3].in++;  
		node[0].in++ <-- {delay = 30.0ms; } <-- node[3].out++;  
		node[0].out++ --> {delay = 30.0ms; } --> node[1].in++;  
		node[0].in++ <-- {delay = 30.0ms; } <-- node[1].out++;  
		node[2].out++ --> {delay = 30.0ms; } --> node[3].in++;  
		node[2].in++ <-- {delay = 30.0ms; } <-- node[3].out++;  
		node[2].out++ --> {delay = 30.0ms; } --> node[4].in++;  
		node[2].in++ <-- {delay = 30.0ms; } <-- node[4].out++;  
		node[2].out++ --> {delay = 30.0ms; } --> node[1].in++;  
		node[2].in++ <-- {delay = 30.0ms; } <-- node[1].out++;  
		node[1].out++ --> {delay = 30.0ms; } --> node[3].in++;  
		node[1].in++ <-- {delay = 30.0ms; } <-- node[3].out++;  
}
