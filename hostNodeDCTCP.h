#ifndef ROUTERNODE_DCTCP_H
#define ROUTERNODE_DCTCP_H

#include "priceQueryMsg_m.h"
#include "priceUpdateMsg_m.h"
#include "hostNodeBase.h"

using namespace std;
using namespace omnetpp;

class hostNodeDCTCP : public hostNodeBase {
    private:
        // DCTCP signals
        unordered_map<int, simsignal_t> numWaitingPerDestSignals = {};
        unordered_map<int, simsignal_t> demandEstimatePerDestSignals = {};

    protected:
        virtual void initialize() override;
        virtual void handleMessage(cMessage *msg) override;
        virtual void handleAckMessageSpecialized(routerMsg *msg) override;
        virtual void handleTimeOutMessage(routerMsg *msg) override;
        virtual void handleTransactionMessageSpecialized(routerMsg *msg) override;
        virtual void handleStatMessage(routerMsg *msg) override;
        virtual void handleMonitorPathsMessage(routerMsg *msg);
        virtual void handleClearStateMessage(routerMsg *msg) override;

        // helper method
        virtual double getMaxWindowSize(unordered_map<int, PathInfo> pathList); 
        virtual void initializePathInfo(vector<vector<int>> kShortestPaths, int destNode);
        virtual void initializeThisPath(vector<int> thisPath, int pathIdx, int destNode);
        virtual void sendMoreTransactionsOnPath(int destNode, int pathIndex);
        virtual routerMsg *generateMonitorPathsMessage();
        virtual int forwardTransactionMessage(routerMsg *msg, int dest, simtime_t arrivalTime) override;
};
#endif
