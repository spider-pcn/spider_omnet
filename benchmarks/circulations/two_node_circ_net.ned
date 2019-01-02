import routerNode;
import hostNode;

network ../benchmarks/circulations/two_node_circ_net
{
	parameters:
		double linkDelay @unit("s") = default(100us);
		double linkDataRate @unit("Gbps") = default(1Gbps);
	submodules:
		host[2]: hostNode {} 
		router[2]: routerNode {} 
connections: 
		router[0].out++ --> {delay = 30.0ms; } --> router[1].in++;  
		router[0].in++ <-- {delay = 30.0ms; } <-- router[1].out++;  
		host[0].out++ --> {delay = 30.0ms; } --> router[0].in++;  
		host[0].in++ <-- {delay = 30.0ms; } <-- router[0].out++;  
		host[1].out++ --> {delay = 30.0ms; } --> router[1].in++;  
		host[1].in++ <-- {delay = 30.0ms; } <-- router[1].out++;  
}
