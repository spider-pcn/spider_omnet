
using namespace std;
using namespace omnetpp;
#include "routerMsg_m.h"

class DestInfo{
public:
    deque<routerMsg *> transWaitingToBeSent;
    

    // units are txns per second
    double demand = 0;

    // number of transactions since the last interval
    double transSinceLastInterval = 0;

    // path and balance of the path with highest current
    // bottleneck balance - amt Inflight
    int highestBottleneckPathIndex;
    double highestBottleneckBalance;
};
