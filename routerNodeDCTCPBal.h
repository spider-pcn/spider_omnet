#ifndef ROUTERNODE_DCTCP_BAL_H
#define ROUTERNODE_DCTCP_BAL_H

#include "routerNodeBase.h"

using namespace std;
using namespace omnetpp;

class routerNodeDCTCPBal : public routerNodeBase {
    protected:
        // messsage helpers
        virtual void handleTransactionMessage(routerMsg *msg) override;
};
#endif
