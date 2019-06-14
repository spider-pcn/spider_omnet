#ifndef ROUTERNODE_DCTCP_H
#define ROUTERNODE_DCTCP_H

#include "routerNodeBase.h"

using namespace std;
using namespace omnetpp;

class routerNodeDCTCP : public routerNodeBase {
    protected:
        // messsage helpers
        virtual void handleTransactionMessage(routerMsg *msg) override;
        virtual void handleStatMessage(routerMsg* ttmsg) override;
};
#endif
