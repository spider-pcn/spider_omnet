import routerNode;
import hostNode;

network ../benchmarks/three_node_circ_net
{
	parameters:
		double linkDelay @unit("s") = default(100us);
		double linkDataRate @unit("Gbps") = default(1Gbps);
	submodules:
		host[3]: hostNode {} 
		router[3]: routerNode {} 
connections: 
		router[0].out++ --> {delay = 30.0ms; } --> router[1].in++;  
		router[0].in++ <-- {delay = 30.0ms; } <-- router[1].out++;  
		router[0].out++ --> {delay = 30.0ms; } --> router[2].in++;  
		router[0].in++ <-- {delay = 30.0ms; } <-- router[2].out++;  
		router[1].out++ --> {delay = 30.0ms; } --> router[2].in++;  
		router[1].in++ <-- {delay = 30.0ms; } <-- router[2].out++;  
		host[0].out++ --> {delay = 30.0ms; } --> router[0].in++;  
		host[0].in++ <-- {delay = 30.0ms; } <-- router[0].out++;  
		host[1].out++ --> {delay = 30.0ms; } --> router[1].in++;  
		host[1].in++ <-- {delay = 30.0ms; } <-- router[1].out++;  
		host[2].out++ --> {delay = 30.0ms; } --> router[2].in++;  
		host[2].in++ <-- {delay = 30.0ms; } <-- router[2].out++;  
}
