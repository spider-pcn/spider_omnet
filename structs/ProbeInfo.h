using namespace std;
using namespace omnetpp;
#include "routerMsg_m.h"

class ProbeInfo{
public:
    routerMsg* messageToSend; //message to send out once all probes return
    vector<simtime_t> probeReturnTimes; //probeReturnTimes[0] is return time of first probe
	vector<double> probeBottlenecks; //probeBottlenecks[0] is the bottleneck of the first probe/route
	int numProbesWaiting; //number of probes waiting to be received (can send transaction message once this number of 0)
};
