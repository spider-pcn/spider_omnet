
using namespace std;
using namespace omnetpp;
#include "routerMsg_m.h"

class DestInfo{
public:
    stack<routerMsg *> transWaitingToBeSent;
    

    // units are txns per second
    double demand = 0;

    // number of transactions since the last interval
    double transSinceLastInterval = 0;
};
