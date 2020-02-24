 #ifndef ROUTERNODE_LR_H
#define ROUTERNODE_LR_H

#include "probeMsg_m.h"
#include "hostNodeBase.h"

using namespace std;
using namespace omnetpp;

class hostNodeLandmarkRouting : public hostNodeBase {
    private:
        map<int, ProbeInfo> transactionIdToProbeInfoMap = {}; //used only for landmark routing

    protected:
        virtual routerMsg *generateProbeMessage(int destNode, int pathIdx, vector<int> path);
        virtual void forwardProbeMessage(routerMsg *msg);

        // message handlers
        virtual void handleMessage(cMessage* msg) override;
        virtual void handleTransactionMessageSpecialized(routerMsg *msg) override;
        virtual void handleTimeOutMessage(routerMsg *msg) override;
        virtual void handleAckMessageTimeOut(routerMsg *msg) override;
        virtual void handleAckMessageSpecialized(routerMsg *msg) override;
        virtual void handleClearStateMessage(routerMsg *msg) override;
        virtual void handleProbeMessage(routerMsg *msg);
      
        // core logic
        virtual bool randomSplit(double totalAmt, vector<double> bottlenecks, vector<double> &amtPerPath);
        virtual void initializePathInfoLandmarkRouting(vector<vector<int>> kShortestRoutes, 
               int  destNode);
        virtual void initializeLandmarkRoutingProbes(routerMsg * msg, 
               int transactionId, int destNode);

        virtual void finish() override;
};
#endif
