#ifndef ROUTERNODE_CELER_H
#define ROUTERNODE_CELER_H

#include "routerNodeBase.h"

using namespace std;
using namespace omnetpp;

class routerNodeCeler : public routerNodeBase {

    private:

    unordered_map<int, DestNodeStruct> nodeToDestNodeStruct;

        /*
        // time since the last measurement for balances was made
        // used to update path probabilities in smooth waterfilling
        unordered_map<int, double> destNodeToLastMeasurementTime = {};

        // waterfilling specific signals
        unordered_map<int, simsignal_t> probabilityPerDestSignals = {};
        unordered_map<int, simsignal_t> numWaitingPerDestSignals = {};
        */
    protected:
        virtual void initialize() override; //initialize global debtQueues
         //TODO: KATHY - start here
        virtual void handleTransactionMessage(routerMsg *msg) override; // find k* for each paymen channel
        virtual bool forwardTransactionMessage(routerMsg *msg, int dest, simtime_t arrivalTime) override; //decrement debt queues

        virtual double calculateCPI(int destNode, int neighborNode);
        virtual int findKStar(int endLinkNode);

            //need to determine where to send transaction message



        /*
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

        //takes a vector of routes for a destination, and puts them into the map and initiates
        // probes to them
        virtual void initializeProbes(vector<vector<int>> kShortestPaths, int destNode);

        // restarts probes onces they've been stopped
        virtual void restartProbes(int destNode);

        // forwards probes
        virtual void forwardProbeMessage(routerMsg *msg);

        // splits transactions and decides which paths they should use
        virtual void splitTransactionForWaterfilling(routerMsg * ttMsg, bool firstAttempt);

        // sends entirety of transaciton on path with highest bottleneck balance
        virtual void attemptTransactionOnBestPath(routerMsg * ttMsg, bool firstAttempt);

        // helper functions
        virtual int updatePathProbabilities(vector<double> bottleneckBalances, int destNode);
        */

};
#endif
