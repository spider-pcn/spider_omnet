#ifndef ROUTERNODE_CELER_H
#define ROUTERNODE_CELER_H

//#include "probeMsg_m.h"
#include "hostNodeBase.h"

using namespace std;
using namespace omnetpp;

class hostNodeCeler : public hostNodeBase {

    private:
        unordered_map<int, DestNodeStruct> nodeToDestNodeStruct;
        unordered_map<int, int> transToNextHop; 

    protected:
        virtual void initialize() override; //initialize global debtQueues
        // find k* for each paymen channel
        virtual void handleTransactionMessageSpecialized(routerMsg *msg) override; 
        virtual void handleAckMessageSpecialized(routerMsg *msg) override; 
        //decrement debt queues
        virtual bool forwardTransactionMessage(routerMsg *msg, int dest, simtime_t arrivalTime) override; 
        virtual void handleTimeOutMessage(routerMsg *msg) override;

        virtual double calculateCPI(int destNode, int neighborNode);
        virtual int findKStar(int endLinkNode);
        virtual void celerProcessTransactions(int endLinkNode = -1);
        virtual void setPaymentChannelBalanceByNode(int node, double balance) override;

        virtual void handleStatMessage(routerMsg *msg) override;

};
#endif
