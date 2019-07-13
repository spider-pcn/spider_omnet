#ifndef ROUTERNODE_H
#define ROUTERNODE_H

#include <stdio.h>
#include <string.h>
#include <omnetpp.h>
#include <vector>
#include "routerMsg_m.h"
#include "transactionMsg_m.h"
#include "ackMsg_m.h"
#include "updateMsg_m.h"
#include "timeOutMsg_m.h"
#include "probeMsg_m.h"
#include <iostream>
#include <algorithm>
#include <string>
#include <sstream>
#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <fstream>
#include "global.h"
#include "hostInitialize.h"


using namespace std;
using namespace omnetpp;

class routerNodeBase : public cSimpleModule
{
    protected:
        unordered_map<int, PaymentChannel> nodeToPaymentChannel = {};
        unordered_set<CanceledTrans, hashCanceledTrans, equalCanceledTrans> canceledTransactions = {};

        // helper methods
        virtual int myIndex();
        virtual void checkQueuedTransUnits(vector<tuple<int, double, routerMsg*,  Id, simtime_t >> queuedTransUnits, int node);
        virtual void printNodeToPaymentChannel();
        virtual simsignal_t registerSignalPerChannel(string signalStart, int id);
        virtual double getTotalAmount(int x);
        virtual double getTotalAmountIncomingInflight(int x);
        virtual double getTotalAmountOutgoingInflight(int x);

        // core simulator functions 
        virtual void initialize() override;
        virtual void finish() override;
        virtual void processTransUnits(int dest, vector<tuple<int, double , routerMsg *, Id, simtime_t>>& q);
        virtual void deleteMessagesInQueues();

        // generators for standard messages
        virtual routerMsg *generateUpdateMessage(int transId, int receiver, double amount, int htlcIndex);
        //just generates routerMsg with no encapsulated msg inside
        virtual routerMsg *generateStatMessage();
        virtual routerMsg *generateClearStateMessage();
        virtual routerMsg *generateAckMessage(routerMsg *msg, bool isSuccess = true);

        // message forwarders
        virtual void forwardMessage(routerMsg *msg);
        virtual bool forwardTransactionMessage(routerMsg *msg, int dest, simtime_t);

        // message handlers
        virtual void handleMessage(cMessage *msg) override;
        virtual void handleTransactionMessage(routerMsg *msg);
        // returns true if message has timed out
        virtual bool handleTransactionMessageTimeOut(routerMsg *msg);

        virtual void handleTimeOutMessage(routerMsg *msg);
        virtual void handleAckMessage(routerMsg *msg);
        virtual void handleAckMessageTimeOut(routerMsg *msg);
        virtual void handleUpdateMessage(routerMsg *msg);
        virtual void handleStatMessage(routerMsg *msg);
        virtual void handleClearStateMessage(routerMsg *msg);
};
#endif
