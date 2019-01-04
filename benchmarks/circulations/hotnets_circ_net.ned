import routerNode;
import hostNode;

network hotnets_circ_net
{
	parameters:
		double linkDelay @unit("s") = default(100us);
		double linkDataRate @unit("Gbps") = default(1Gbps);
	submodules:
		host[5]: hostNode {} 
		router[5]: routerNode {} 
connections: 
		router[0].out++ --> {delay = 30.0ms; } --> router[1].in++;  
		router[0].in++ <-- {delay = 30.0ms; } <-- router[1].out++;  
		router[0].out++ --> {delay = 30.0ms; } --> router[4].in++;  
		router[0].in++ <-- {delay = 30.0ms; } <-- router[4].out++;  
		router[1].out++ --> {delay = 30.0ms; } --> router[2].in++;  
		router[1].in++ <-- {delay = 30.0ms; } <-- router[2].out++;  
		router[1].out++ --> {delay = 30.0ms; } --> router[3].in++;  
		router[1].in++ <-- {delay = 30.0ms; } <-- router[3].out++;  
		router[2].out++ --> {delay = 30.0ms; } --> router[3].in++;  
		router[2].in++ <-- {delay = 30.0ms; } <-- router[3].out++;  
		router[3].out++ --> {delay = 30.0ms; } --> router[4].in++;  
		router[3].in++ <-- {delay = 30.0ms; } <-- router[4].out++;  
		host[0].out++ --> {delay = 30.0ms; } --> router[0].in++;  
		host[0].in++ <-- {delay = 30.0ms; } <-- router[0].out++;  
		host[1].out++ --> {delay = 30.0ms; } --> router[1].in++;  
		host[1].in++ <-- {delay = 30.0ms; } <-- router[1].out++;  
		host[2].out++ --> {delay = 30.0ms; } --> router[2].in++;  
		host[2].in++ <-- {delay = 30.0ms; } <-- router[2].out++;  
		host[3].out++ --> {delay = 30.0ms; } --> router[3].in++;  
		host[3].in++ <-- {delay = 30.0ms; } <-- router[3].out++;  
		host[4].out++ --> {delay = 30.0ms; } --> router[4].in++;  
		host[4].in++ <-- {delay = 30.0ms; } <-- router[4].out++;  
}
