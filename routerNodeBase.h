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
        double amtSuccessfulSoFar = 0;

        // helper methods
        virtual int myIndex();
        virtual void checkQueuedTransUnits(vector<tuple<int, double, routerMsg*,  Id, simtime_t >> 
                queuedTransUnits, int node);
        virtual void printNodeToPaymentChannel();
        virtual simsignal_t registerSignalPerChannel(string signalStart, int id);
        virtual simsignal_t registerSignalPerDest(string signalStart, int id, string suffix);
        virtual simsignal_t registerSignalPerChannelPerDest(string signalStart,
              int pathIdx, int destNode);
        virtual double getTotalAmount(int x);
        virtual double getTotalAmountIncomingInflight(int x);
        virtual double getTotalAmountOutgoingInflight(int x);
        virtual void performRebalancing();
        virtual void setPaymentChannelBalanceByNode(int node, double balance);
        virtual void deleteTransaction(routerMsg* ttmsg);
        virtual void addFunds(map<int, double> pcsNeedingFunds);

        // core simulator functions 
        virtual void initialize() override;
        virtual void finish() override;
        virtual bool processTransUnits(int dest, vector<tuple<int, double , routerMsg *, Id, simtime_t>>& q);
        virtual void deleteMessagesInQueues();

        // generators for standard messages
        virtual routerMsg *generateUpdateMessage(int transId, int receiver, double amount, int htlcIndex);
        virtual routerMsg *generateStatMessage();
        virtual routerMsg *generateComputeMinBalanceMessage();
        virtual routerMsg *generateTriggerRebalancingMessage();
        virtual routerMsg *generateAddFundsMessage(map<int,double> fundsToBeAdded);
        virtual routerMsg *generateClearStateMessage();
        virtual routerMsg *generateAckMessage(routerMsg *msg, bool isSuccess = true);

        // message forwarders
        virtual void forwardMessage(routerMsg *msg);
        virtual int forwardTransactionMessage(routerMsg *msg, int dest, simtime_t);

        // message handlers
        virtual void handleMessage(cMessage *msg) override;
        virtual void handleTransactionMessage(routerMsg *msg);
        virtual void handleComputeMinAvailableBalanceMessage(routerMsg* ttmsg);
        virtual bool handleTransactionMessageTimeOut(routerMsg *msg);
        virtual void handleTimeOutMessage(routerMsg *msg);
        virtual void handleAckMessage(routerMsg *msg);
        virtual void handleAckMessageTimeOut(routerMsg *msg);
        virtual void handleUpdateMessage(routerMsg *msg);
        virtual void handleStatMessage(routerMsg *msg);
        virtual void handleClearStateMessage(routerMsg *msg);
};
#endif
