
using namespace std;
using namespace omnetpp;
#include "routerMsg_m.h"

class DestInfo{
public:
    deque<routerMsg *> transWaitingToBeSent = {};
};
