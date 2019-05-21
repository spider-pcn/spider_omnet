#ifndef ROUTERNODE_LNDB_H
#define ROUTERNODE_LNDB_H

#include "routerNodeBase.h"

using namespace std;
using namespace omnetpp;

class routerNodeLndBaseline : public routerNodeBase {
    protected:
        // messsage helpers
        virtual routerMsg *generateAckMessage(routerMsg *msg, bool isSuccess = true) override;
};
#endif
