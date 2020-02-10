#ifndef ROUTERNODE_DCTCP_H
#define ROUTERNODE_DCTCP_H

#include "routerNodeBase.h"

using namespace std;
using namespace omnetpp;

class routerNodeDCTCP : public routerNodeBase {
    protected:
        // messsage helpers
        virtual void handleStatMessage(routerMsg* ttmsg) override;
        virtual int forwardTransactionMessage(routerMsg *msg, int dest, simtime_t arrivalTime) override;
};
#endif
