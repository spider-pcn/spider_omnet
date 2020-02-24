#ifndef ROUTERNODE_CELER_H
#define ROUTERNODE_CELER_H

#include "hostNodeBase.h"

using namespace std;
using namespace omnetpp;

class hostNodeCeler : public hostNodeBase {

    private:
        unordered_map<int, DestNodeStruct> nodeToDestNodeStruct;
        unordered_map<int, int> transToNextHop; 

    protected:
        virtual void initialize() override; //initialize global debtQueues
        virtual void finish() override; // remove messages from the dest queues

        // message handlers
        virtual void handleStatMessage(routerMsg *msg) override;
        virtual void handleClearStateMessage(routerMsg *msg) override;
        virtual void handleTimeOutMessage(routerMsg *msg) override;
        virtual void handleTransactionMessageSpecialized(routerMsg *msg) override; 
        virtual void handleAckMessageSpecialized(routerMsg *msg) override; 

        // helper methods
        virtual void pushIntoPerDestQueue(routerMsg* rMsg, int destNode);
        virtual void celerProcessTransactions(int endLinkNode = -1);
        virtual double calculateCPI(int destNode, int neighborNode);
        virtual int findKStar(int endLinkNode, unordered_set<int> exclude); // find k* for each paymen channel
        virtual int forwardTransactionMessage(routerMsg *msg, int dest, simtime_t arrivalTime) override; 
        virtual void setPaymentChannelBalanceByNode(int node, double balance) override;


};
#endif
