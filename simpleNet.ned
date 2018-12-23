import routerNode;
import hostNode;

network simpleNet
{
	parameters:
		double linkDelay @unit("s") = default(100us);
		double linkDataRate @unit("Gbps") = default(1Gbps);
	submodules:
		host[20]: hostNode {} 
		router[20]: routerNode {} 
connections: 
		router[0].out++ --> {delay = 30.0ms; } --> router[1].in++;  
		router[0].in++ <-- {delay = 30.0ms; } <-- router[1].out++;  
		router[0].out++ --> {delay = 30.0ms; } --> router[19].in++;  
		router[0].in++ <-- {delay = 30.0ms; } <-- router[19].out++;  
		router[0].out++ --> {delay = 30.0ms; } --> router[7].in++;  
		router[0].in++ <-- {delay = 30.0ms; } <-- router[7].out++;  
		router[1].out++ --> {delay = 30.0ms; } --> router[2].in++;  
		router[1].in++ <-- {delay = 30.0ms; } <-- router[2].out++;  
		router[2].out++ --> {delay = 30.0ms; } --> router[3].in++;  
		router[2].in++ <-- {delay = 30.0ms; } <-- router[3].out++;  
		router[3].out++ --> {delay = 30.0ms; } --> router[17].in++;  
		router[3].in++ <-- {delay = 30.0ms; } <-- router[17].out++;  
		router[3].out++ --> {delay = 30.0ms; } --> router[4].in++;  
		router[3].in++ <-- {delay = 30.0ms; } <-- router[4].out++;  
		router[4].out++ --> {delay = 30.0ms; } --> router[5].in++;  
		router[4].in++ <-- {delay = 30.0ms; } <-- router[5].out++;  
		router[5].out++ --> {delay = 30.0ms; } --> router[6].in++;  
		router[5].in++ <-- {delay = 30.0ms; } <-- router[6].out++;  
		router[6].out++ --> {delay = 30.0ms; } --> router[7].in++;  
		router[6].in++ <-- {delay = 30.0ms; } <-- router[7].out++;  
		router[8].out++ --> {delay = 30.0ms; } --> router[9].in++;  
		router[8].in++ <-- {delay = 30.0ms; } <-- router[9].out++;  
		router[9].out++ --> {delay = 30.0ms; } --> router[10].in++;  
		router[9].in++ <-- {delay = 30.0ms; } <-- router[10].out++;  
		router[10].out++ --> {delay = 30.0ms; } --> router[11].in++;  
		router[10].in++ <-- {delay = 30.0ms; } <-- router[11].out++;  
		router[11].out++ --> {delay = 30.0ms; } --> router[12].in++;  
		router[11].in++ <-- {delay = 30.0ms; } <-- router[12].out++;  
		router[12].out++ --> {delay = 30.0ms; } --> router[13].in++;  
		router[12].in++ <-- {delay = 30.0ms; } <-- router[13].out++;  
		router[13].out++ --> {delay = 30.0ms; } --> router[14].in++;  
		router[13].in++ <-- {delay = 30.0ms; } <-- router[14].out++;  
		router[14].out++ --> {delay = 30.0ms; } --> router[15].in++;  
		router[14].in++ <-- {delay = 30.0ms; } <-- router[15].out++;  
		router[15].out++ --> {delay = 30.0ms; } --> router[16].in++;  
		router[15].in++ <-- {delay = 30.0ms; } <-- router[16].out++;  
		router[16].out++ --> {delay = 30.0ms; } --> router[17].in++;  
		router[16].in++ <-- {delay = 30.0ms; } <-- router[17].out++;  
		router[18].out++ --> {delay = 30.0ms; } --> router[19].in++;  
		router[18].in++ <-- {delay = 30.0ms; } <-- router[19].out++;  
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
		host[5].out++ --> {delay = 30.0ms; } --> router[5].in++;  
		host[5].in++ <-- {delay = 30.0ms; } <-- router[5].out++;  
		host[6].out++ --> {delay = 30.0ms; } --> router[6].in++;  
		host[6].in++ <-- {delay = 30.0ms; } <-- router[6].out++;  
		host[7].out++ --> {delay = 30.0ms; } --> router[7].in++;  
		host[7].in++ <-- {delay = 30.0ms; } <-- router[7].out++;  
		host[8].out++ --> {delay = 30.0ms; } --> router[8].in++;  
		host[8].in++ <-- {delay = 30.0ms; } <-- router[8].out++;  
		host[9].out++ --> {delay = 30.0ms; } --> router[9].in++;  
		host[9].in++ <-- {delay = 30.0ms; } <-- router[9].out++;  
		host[10].out++ --> {delay = 30.0ms; } --> router[10].in++;  
		host[10].in++ <-- {delay = 30.0ms; } <-- router[10].out++;  
		host[11].out++ --> {delay = 30.0ms; } --> router[11].in++;  
		host[11].in++ <-- {delay = 30.0ms; } <-- router[11].out++;  
		host[12].out++ --> {delay = 30.0ms; } --> router[12].in++;  
		host[12].in++ <-- {delay = 30.0ms; } <-- router[12].out++;  
		host[13].out++ --> {delay = 30.0ms; } --> router[13].in++;  
		host[13].in++ <-- {delay = 30.0ms; } <-- router[13].out++;  
		host[14].out++ --> {delay = 30.0ms; } --> router[14].in++;  
		host[14].in++ <-- {delay = 30.0ms; } <-- router[14].out++;  
		host[15].out++ --> {delay = 30.0ms; } --> router[15].in++;  
		host[15].in++ <-- {delay = 30.0ms; } <-- router[15].out++;  
		host[16].out++ --> {delay = 30.0ms; } --> router[16].in++;  
		host[16].in++ <-- {delay = 30.0ms; } <-- router[16].out++;  
		host[17].out++ --> {delay = 30.0ms; } --> router[17].in++;  
		host[17].in++ <-- {delay = 30.0ms; } <-- router[17].out++;  
		host[18].out++ --> {delay = 30.0ms; } --> router[18].in++;  
		host[18].in++ <-- {delay = 30.0ms; } <-- router[18].out++;  
		host[19].out++ --> {delay = 30.0ms; } --> router[19].in++;  
		host[19].in++ <-- {delay = 30.0ms; } <-- router[19].out++;  
}
