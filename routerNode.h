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
#include <map>
#include <fstream>
#include "transUnit.h"


using namespace std;
using namespace omnetpp;


struct paymentChannel{
    //channel information
    cGate* gate;
    double balance;
    vector<tuple<int, double, routerMsg*>> queuedTransUnits; //make_heap in initialization
    map<int, double> incomingTransUnits; //(key,value) := (transactionId, amount)
    map<int, double> outgoingTransUnits;

    //statistics - ones for per payment channel
    int statNumProcessed;
    int statNumSent;
    simsignal_t numInQueuePerChannelSignal;
    simsignal_t numProcessedPerChannelSignal;
    simsignal_t balancePerChannelSignal;
    simsignal_t numSentPerChannelSignal;

};

struct PathInfo{
    vector<int> path;
    simtime_t lastUpdated;
    double bottleneck;
    vector<double> pathBalances;
};



class routerNode : public cSimpleModule
{
   private:
      string topologyFile_;
      string workloadFile_;
      map<int, paymentChannel> nodeToPaymentChannel;
      vector<int> statNumCompleted;
      vector<int> statNumAttempted;
      map<int, vector<int>> destNodeToPath;

      map<int, map<int, PathInfo>> nodeToShortestPathsMap;


      simsignal_t completionTimeSignal;
      vector<simsignal_t> numCompletedPerDestSignals;
      vector<simsignal_t> numAttemptedPerDestSignals;
      vector< transUnit > myTransUnits; //list of transUnits that have me as sender
      set<int> successfulDoNotSendTimeOut; //set of transaction units WITH timeouts, that we already received acks for



   protected:
      virtual routerMsg *generateTransactionMessage(transUnit transUnit);
      virtual routerMsg *generateAckMessage(routerMsg *msg, bool isTimeOutMsg = false);
          //generates ack message and destroys input parameter msg
      //virtual routerMsg *generateAckMessageFromTimeOutMsg(routerMsg *msg);
      virtual routerMsg *generateUpdateMessage(int transId, int receiver, double amount);
          //just generates routerMsg with no encapsulated msg inside
      virtual routerMsg *generateStatMessage();
      virtual routerMsg *generateTimeOutMessage(routerMsg *transMsg);


      virtual void initialize() override;
      virtual void handleMessage(cMessage *msg) override;
      virtual void handleTransactionMessage(routerMsg *msg);
      virtual void handleTimeOutMessage(routerMsg *msg);
      virtual void handleAckMessage(routerMsg *msg);
      virtual void handleUpdateMessage(routerMsg *msg);
      virtual void handleStatMessage(routerMsg *msg);
      virtual bool forwardTransactionMessage(routerMsg *msg);
      virtual void forwardAckMessage(routerMsg *msg);
      virtual void forwardTimeOutMessage(routerMsg *msg);
      virtual void sendUpdateMessage(routerMsg *msg);
      virtual void processTransUnits(int dest, vector<tuple<int, double , routerMsg *>>& q);
      virtual void deleteMessagesInQueues();


};


//global parameters
extern vector<transUnit> transUnitList;
extern int numNodes;
//number of nodes in network
extern map<int, vector<pair<int,int>>> channels; //adjacency list format of graph edges of network
extern map<tuple<int,int>,double> balances;
extern double statRate;
//map of balances for each edge; key = <int,int> is <source, destination>
//extern bool withFailures;
extern bool useWaterfilling;
extern int kValue; //for k shortest paths

#endif
