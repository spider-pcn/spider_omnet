#ifndef ROUTERNODE_CELER_H
#define ROUTERNODE_CELER_H

#include "routerNodeBase.h"

using namespace std;
using namespace omnetpp;

class routerNodeCeler : public routerNodeBase {
    private:
        unordered_map<int, DestNodeStruct> nodeToDestNodeStruct;
        unordered_map<int, int> transToNextHop; 

    protected:
        virtual void initialize() override; //initialize global debtQueues
        virtual void handleTransactionMessage(routerMsg *msg) override; // find k* for each paymen channel
        virtual void handleAckMessage(routerMsg *msg) override;
        virtual bool forwardTransactionMessage(routerMsg *msg, int dest, simtime_t arrivalTime) override; 
        virtual void handleTimeOutMessage(routerMsg *msg) override;
        //decrement debt queues

        // helper functions
        virtual void appendNextHopToTimeOutMessage(routerMsg* ttmsg, int nextNode);
        virtual double calculateCPI(int destNode, int neighborNode);
        virtual int findKStar(int endLinkNode);
        virtual void celerProcessTransactions(int endLinkNode = -1);
        virtual void setPaymentChannelBalanceByNode(int node, double balance) override;

        virtual void handleStatMessage(routerMsg *msg) override;

};
#endif
