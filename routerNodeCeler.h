#ifndef ROUTERNODE_CELER_H
#define ROUTERNODE_CELER_H

#include "routerNodeBase.h"

using namespace std;
using namespace omnetpp;

class routerNodeWaterfilling : public routerNodeBase {
    protected:

        unordered_map<int, vector<tuple<int, double, routerMsg*,  Id, simtime_t >>> destNodeToQueuedTransUnits;

        unordered_map<int, double> destNodeToQueueSizeSum = 0;
        unordered_map<int, double> destNodeToTotalAmtInQueue = 0; // total amount in the queue

       /*
        // message handlers
        virtual void handleMessage(cMessage *msg) override;
        virtual void handleProbeMessage(routerMsg *msg);
      
        // messsage helpers
        virtual void forwardProbeMessage(routerMsg *msg);
        */
};
#endif
