#ifndef ROUTERNODE_WF_H
#define ROUTERNODE_WF_H

#include "probeMsg_m.h"
#include "routerNodeBase.h"

using namespace std;
using namespace omnetpp;

class routerNodeWaterfilling : public routerNodeBase {
    protected:
        // message handlers
        virtual void handleMessage(cMessage *msg) override;
        virtual void handleProbeMessage(routerMsg *msg);
      
        // messsage helpers
        virtual void forwardProbeMessage(routerMsg *msg);
};
#endif
