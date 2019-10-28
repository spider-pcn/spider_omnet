#ifndef ROUTERNODE_CELER_H
#define ROUTERNODE_CELER_H

//#include "probeMsg_m.h"
#include "hostNodeBase.h"

using namespace std;
using namespace omnetpp;

class hostNodeCeler : public hostNodeBase {

    private:
        unordered_map<int, DestNodeStruct> nodeToDestNodeStruct;

    protected:
        virtual void initialize() override; //initialize global debtQueues
        virtual void handleTransactionMessageSpecialized(routerMsg *msg) override; // find k* for each paymen channel
        virtual bool forwardTransactionMessage(routerMsg *msg, int dest, simtime_t arrivalTime) override; //decrement debt queues

        virtual double calculateCPI(int destNode, int neighborNode);
        virtual int findKStar(int endLinkNode);
        virtual void celerProcessTransactions(int endLinkNode = -1);
        virtual void setPaymentChannelBalanceByNode(int node, double balance) override;
};
#endif
