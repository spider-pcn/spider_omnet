import routerNodeCeler;
import hostNodeCeler;

network simpleNet
{
        parameters:
                double linkDelay @unit("s") = default(100us);
                double linkDataRate @unit("Gbps") = default(1Gbps);
        submodules:
                host[2]: hostNodeCeler {}
                router[2]: routerNodeCeler {}
connections:
    			host[0].out++ --> {delay = 300.0ms; } --> router[0].in++;
    			host[0].in++ <-- {delay = 300.0ms; } <-- router[0].out++;
                router[0].out++ --> {delay = 300.0ms; } --> router[1].in++;
                router[0].in++ <-- {delay = 300.0ms; } <-- router[1].out++;
                host[1].out++ --> {delay = 300.0ms; } --> router[1].in++;
    			host[1].in++ <-- {delay = 300.0ms; } <-- router[1].out++;
}                                                                    