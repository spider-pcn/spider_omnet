import routerNode;

network ../benchmarks/hotnets_circ_net
{
	parameters:
		double linkDelay @unit("s") = default(100us);
		double linkDataRate @unit("Gbps") = default(1Gbps);
	submodules:
		node[10]: routerNode {} 
 connections: 
		node[0].out++ --> {delay = 30.0ms; } --> node[1].in++;  
		node[0].in++ <-- {delay = 30.0ms; } <-- node[1].out++;  
		node[0].out++ --> {delay = 30.0ms; } --> node[4].in++;  
		node[0].in++ <-- {delay = 30.0ms; } <-- node[4].out++;  
		node[1].out++ --> {delay = 30.0ms; } --> node[2].in++;  
		node[1].in++ <-- {delay = 30.0ms; } <-- node[2].out++;  
		node[1].out++ --> {delay = 30.0ms; } --> node[3].in++;  
		node[1].in++ <-- {delay = 30.0ms; } <-- node[3].out++;  
		node[2].out++ --> {delay = 30.0ms; } --> node[3].in++;  
		node[2].in++ <-- {delay = 30.0ms; } <-- node[3].out++;  
		node[3].out++ --> {delay = 30.0ms; } --> node[4].in++;  
		node[3].in++ <-- {delay = 30.0ms; } <-- node[4].out++;  
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
