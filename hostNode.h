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
#include "global.h"

using namespace std;
using namespace omnetpp;

class hostNode : public cSimpleModule
{
   private:

      string topologyFile_;
      string workloadFile_;
      map<int, PaymentChannel> nodeToPaymentChannel;
      vector<int> statNumCompleted;
      vector<int> statNumAttempted;
      vector<int> statNumTimedOut;
      map<int, vector<int>> destNodeToPath; //store shortest paths
      map<int, map<int, PathInfo>> nodeToShortestPathsMap = {}; //store k shortest paths in waterfilling
      simsignal_t completionTimeSignal;
      vector<simsignal_t> numCompletedPerDestSignals;
      vector<simsignal_t> numAttemptedPerDestSignals;
      vector<simsignal_t> numTimedOutPerDestSignals;
      vector<simsignal_t> pathPerTransPerDestSignals; //signal showing which path index was chosen for each transaction
      vector<simsignal_t> fracSuccessfulPerDestSignals;
      vector< TransUnit > myTransUnits; //list of TransUnits that have me as sender
      set<int> successfulDoNotSendTimeOut; //set of transaction units WITH timeouts, that we already received acks for
      set<CanceledTrans> canceledTransactions = {};
      map<tuple<int,int>,AckState> transPathToAckState = {}; //key is (transactionId, routeIndex)
      map<int, int> transactionIdToNumHtlc = {}; //allows us to calculate the htlcIndex number
      map<int, int> destNodeToNumTransPending;
      map<int, AckState> transToAmtLeftToComplete = {};
      int numCleared = 0;
      simsignal_t numClearedSignal;


   protected:
      virtual int myIndex(); //if host node, return getIndex(), if routerNode, return getIndex()+numHostNodes
      virtual routerMsg *generateWaterfillingTransactionMessage(double amt, vector<int> path, int pathIndex, transactionMsg * transMsg);
      virtual routerMsg *generateWaterfillingTimeOutMessage( vector<int> path, int transactionId, int receiver);
      virtual routerMsg *generateTransactionMessage(TransUnit TransUnit);
      virtual routerMsg *generateAckMessage(routerMsg *msg);
          //generates ack message and destroys input parameter msg
      //virtual routerMsg *generateAckMessageFromTimeOutMsg(routerMsg *msg);
      virtual routerMsg *generateUpdateMessage(int transId, int receiver, double amount, int htlcIndex);
          //just generates routerMsg with no encapsulated msg inside
      virtual routerMsg *generateStatMessage();
      virtual routerMsg *generateClearStateMessage();
      virtual routerMsg *generateTimeOutMessage(routerMsg *transMsg);
      virtual routerMsg *generateProbeMessage(int destNode, int pathIdx, vector<int> path);
      virtual void splitTransactionForWaterfilling(routerMsg * ttMsg);
      virtual void initialize() override;
      virtual void handleMessage(cMessage *msg) override;
      virtual void handleTransactionMessage(routerMsg *msg);
      virtual bool handleTransactionMessageTimeOut(routerMsg *msg); //returns true if message was deleted
      virtual void handleTransactionMessageWaterfilling(routerMsg *msg);
      virtual void handleTimeOutMessageShortestPath(routerMsg *msg);
      virtual void handleTimeOutMessageWaterfilling(routerMsg *msg);
      virtual void handleAckMessage(routerMsg *msg);
      virtual void handleAckMessageTimeOut(routerMsg *msg);
      virtual void handleAckMessageWaterfilling(routerMsg *msg);
      virtual void handleAckMessageShortestPath(routerMsg *msg);
      virtual void handleUpdateMessage(routerMsg *msg);
      virtual void handleStatMessage(routerMsg *msg);
      virtual void handleProbeMessage(routerMsg *msg);
      virtual void handleClearStateMessage(routerMsg *msg);
      virtual bool forwardTransactionMessage(routerMsg *msg);
      virtual void forwardAckMessage(routerMsg *msg);
      virtual void forwardTimeOutMessage(routerMsg *msg);
      virtual void forwardProbeMessage(routerMsg *msg);
      virtual void sendUpdateMessage(routerMsg *msg);
      virtual void processTransUnits(int dest, vector<tuple<int, double , routerMsg *, Id>>& q);
      virtual void initializeProbes(vector<vector<int>> kShortestPaths, int destNode); //takes a vector of routes, and puts them into the map
      virtual void restartProbes(int destNode);
      virtual void deleteMessagesInQueues();

      //helper
      virtual void printNodeToPaymentChannel();
};

#endif
