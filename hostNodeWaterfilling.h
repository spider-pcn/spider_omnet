#ifndef ROUTERNODE_WF_H
#define ROUTERNODE_WF_H

#include "probeMsg_m.h"

using namespace std;
using namespace omnetpp;

class hostNodeWaterfilling : public hostNodeBase {

    private:
        // time since the last measurement for balances was made 
        // used to update path probabilities in smooth waterfilling
        map<int, double> destNodeToLastMeasurementTime = {};

        // waterfilling specific signals
        vector<simsignal_t> probabilityPerDestSignals = {};
        vector<simsignal_t> pathPerTransPerDestSignals = {};    

    protected:
        // message generating functions
        virtual routerMsg *generateWaterfillingTransactionMessage(double amt,
                vector<int> path, int pathIndex, transactionMsg * transMsg);
        virtual routerMsg *generateTimeOutMessage(vector<int> path, 
                int transactionId, int receiver) override;
        virtual routerMsg *generateProbeMessage(int destNode, int pathIdx, vector<int> path);

        // message handlers
        virtual void handleMessage(routerMsg *msg);
        virtual void handleTransactionMessageSpecialized(routerMsg *msg) override;
        virtual void handleTimeOutMessage(routerMsg *msg) override;
        virtual void handleProbeMessage(routerMsg *msg);
        virtual void handleClearStateMessage(routerMsg *msg) override;
        virtual void handleAckMessageTimeOut(routerMsg *msg) override;
        virtual void handleAckMessageSpecialized(routerMsg *msg) override;
        

        /**** CORE LOGIC ****/
        //takes a vector of routes for a destination, and puts them into the map and initiates
        // probes to them
        virtual void initializeProbes(vector<vector<int>> kShortestPaths, int destNode);
        
        // restarts probes onces they've been stopped
        virtual void restartProbes(int destNode);

        // forwards messages including probes
        virtual void forwardProbeMessage(routerMsg *msg);

        // splits transactions and decides which paths they should use
        virtual void splitTransactionForWaterfilling(routerMsg * ttMsg);

        // helper functions
        virtual int updatePathProbabilities(vector<double> bottleneckBalances, int destNode);


}
#endif
