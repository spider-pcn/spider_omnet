#ifndef ROUTERNODE_CELER_H
#define ROUTERNODE_CELER_H

#include "routerNodeBase.h"

using namespace std;
using namespace omnetpp;

class routerNodeCeler : public routerNodeBase {
    private:
        unordered_map<int, DestNodeStruct> nodeToDestNodeStruct;
        unordered_map<int, deque<int>> transToNextHop; 
        unordered_set<int> adjacentHostNodes = {};

    protected:
        virtual void initialize() override; //initialize global debtQueues
        virtual void finish() override; // remove messages from the dest queues
        
        // message handlers
        virtual void handleStatMessage(routerMsg *msg) override;
        virtual void handleTimeOutMessage(routerMsg *msg) override;
        virtual void handleClearStateMessage(routerMsg *msg) override; 
        virtual void handleTransactionMessage(routerMsg *msg) override; 
        virtual void handleAckMessage(routerMsg *msg) override;

        // helper functions
        virtual void appendNextHopToTimeOutMessage(routerMsg* ttmsg, int nextNode);
        virtual void celerProcessTransactions(int endLinkNode = -1);
        virtual int findKStar(int endLinkNode, unordered_set<int> exclude);
        virtual double calculateCPI(int destNode, int neighborNode);
        virtual void setPaymentChannelBalanceByNode(int node, double balance) override;
        virtual int forwardTransactionMessage(routerMsg *msg, int dest, simtime_t arrivalTime) override; 
};
#endif
