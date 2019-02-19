 #ifndef ROUTERNODE_LR_H
#define ROUTERNODE_LR_H

#include "probeMsg_m.h"

using namespace std;
using namespace omnetpp;

class hostNodeLandmarkRouting : public hostNodeBase {
    private:
        map<int, ProbeInfo> transactionIdToProbeInfoMap = {}; //used only for landmark routing

    protected:
       // message handlers 
       virtual void handleTransactionMessage(routerMsg *msg) override;
       virtual void handleProbeMessageLandmarkRouting(routerMsg *msg);
      
       // core logic
       virtual void initializePathInfoLandmarkRouting(vector<vector<int>> kShortestRoutes, 
               int  destNode);
       virtual void initializeLandmarkRoutingProbes(routerMsg * msg, 
               int transactionId, int destNode);
}
#endif
