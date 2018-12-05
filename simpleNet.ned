import routerNode;

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
