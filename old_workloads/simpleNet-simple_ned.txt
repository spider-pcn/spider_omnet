simple routerNode
{
	parameters:
		@signal[numInQueue](type="long"); 
		@statistic[numInQueue](title="total num in queue"; source="numInQueue"; record=vector,stats; interpolationmode=none); 

		@signal[numProcessed](type="long"); 
		@statistic[numProcessed](title="num processing events in statRate"; source="numProcessed"; record=vector,stats; interpolationmode=none); 

		@signal[completionTime](type="long"); 
		@statistic[completionTime](title="completionTime"; source="completionTime"; record=vector,stats; interpolationmode=none); 

		@display("i=block/routing"); 
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
		node[4]: routerNode {} 
 connections: 
		node[0].out++ --> {delay = 30.0ms; } --> node[1].in++;  
		node[0].in++ <-- {delay = 30.0ms; } <-- node[1].out++;  
		node[1].out++ --> {delay = 30.0ms; } --> node[2].in++;  
		node[1].in++ <-- {delay = 30.0ms; } <-- node[2].out++;  
		node[2].out++ --> {delay = 30.0ms; } --> node[3].in++;  
		node[2].in++ <-- {delay = 30.0ms; } <-- node[3].out++;  
}
