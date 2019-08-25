
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

    // number of transactions waiting at the sender over the last x seconds
    // of a monitor paths message
    double sumTxnsWaiting;

    // maximum path Id in use right now
    int maxPathId;
};
