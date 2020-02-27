#ifndef ROUTERNODE_WF_H
#define ROUTERNODE_WF_H

#include "probeMsg_m.h"
#include "hostNodeBase.h"

using namespace std;
using namespace omnetpp;

class hostNodeWaterfilling : public hostNodeBase {

    private:
        // time since the last measurement for balances was made 
        // used to update path probabilities in smooth waterfilling
        unordered_map<int, double> destNodeToLastMeasurementTime = {};

        // waterfilling specific signals
        unordered_map<int, simsignal_t> probabilityPerDestSignals = {};
        unordered_map<int, simsignal_t> numWaitingPerDestSignals = {};

    protected:
        virtual void initialize() override;
        // message generating functions
        virtual routerMsg *generateProbeMessage(int destNode, int pathIdx, vector<int> path);

        // message handlers
        virtual void handleMessage(cMessage *msg) override;
        virtual void handleTransactionMessageSpecialized(routerMsg *msg) override;
        virtual void handleTimeOutMessage(routerMsg *msg) override;
        virtual void handleProbeMessage(routerMsg *msg);
        virtual void handleClearStateMessage(routerMsg *msg) override;
        virtual void handleAckMessageTimeOut(routerMsg *msg) override;
        virtual void handleAckMessageSpecialized(routerMsg *msg) override;
        virtual void handleStatMessage(routerMsg *msg) override;
        
        /**** CORE LOGIC ****/
        virtual void initializeProbes(vector<vector<int>> kShortestPaths, int destNode);
        virtual void restartProbes(int destNode);
        virtual void forwardProbeMessage(routerMsg *msg);
        virtual void splitTransactionForWaterfilling(routerMsg * ttMsg, bool firstAttempt);
        virtual void attemptTransactionOnBestPath(routerMsg * ttMsg, bool firstAttempt);
        virtual int updatePathProbabilities(vector<double> bottleneckBalances, int destNode);


};
#endif
