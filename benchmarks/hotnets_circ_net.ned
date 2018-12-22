import routerNode;

network ../benchmarks/hotnets_circ_net
{
	parameters:
		double linkDelay @unit("s") = default(100us);
		double linkDataRate @unit("Gbps") = default(1Gbps);
	submodules:
		node[10]: routerNode {} 
 connections: 
		node[5].out++ --> {delay = 30.0ms; } --> node[6].in++;  
		node[5].in++ <-- {delay = 30.0ms; } <-- node[6].out++;  
		node[5].out++ --> {delay = 30.0ms; } --> node[9].in++;  
		node[5].in++ <-- {delay = 30.0ms; } <-- node[9].out++;  
		node[6].out++ --> {delay = 30.0ms; } --> node[7].in++;  
		node[6].in++ <-- {delay = 30.0ms; } <-- node[7].out++;  
		node[6].out++ --> {delay = 30.0ms; } --> node[8].in++;  
		node[6].in++ <-- {delay = 30.0ms; } <-- node[8].out++;  
		node[7].out++ --> {delay = 30.0ms; } --> node[8].in++;  
		node[7].in++ <-- {delay = 30.0ms; } <-- node[8].out++;  
		node[8].out++ --> {delay = 30.0ms; } --> node[9].in++;  
		node[8].in++ <-- {delay = 30.0ms; } <-- node[9].out++;  
		node[0].out++ --> {delay = 30.0ms; } --> node[5].in++;  
		node[0].in++ <-- {delay = 30.0ms; } <-- node[5].out++;  
		node[1].out++ --> {delay = 30.0ms; } --> node[6].in++;  
		node[1].in++ <-- {delay = 30.0ms; } <-- node[6].out++;  
		node[2].out++ --> {delay = 30.0ms; } --> node[7].in++;  
		node[2].in++ <-- {delay = 30.0ms; } <-- node[7].out++;  
		node[3].out++ --> {delay = 30.0ms; } --> node[8].in++;  
		node[3].in++ <-- {delay = 30.0ms; } <-- node[8].out++;  
		node[4].out++ --> {delay = 30.0ms; } --> node[9].in++;  
		node[4].in++ <-- {delay = 30.0ms; } <-- node[9].out++;  
}
