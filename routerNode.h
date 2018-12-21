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

class routerNode : public cSimpleModule
{
   private:

      string topologyFile_;
      string workloadFile_;
      map<int, PaymentChannel> nodeToPaymentChannel;
      //vector<int> statNumCompleted;
      //vector<int> statNumAttempted;
      //map<int, vector<int>> destNodeToPath; //store shortest paths
      //map<int, map<int, PathInfo>> nodeToShortestPathsMap;
      //simsignal_t completionTimeSignal;
      //vector<simsignal_t> numCompletedPerDestSignals;
      //vector<simsignal_t> numAttemptedPerDestSignals;
      //vector< TransUnit > myTransUnits; //list of TransUnits that have me as sender
      //set<int> successfulDoNotSendTimeOut; //set of transaction units WITH timeouts, that we already received acks for

   protected:
      //virtual routerMsg *generateWaterfillingTransactionMessage(double amt, vector<int> path, transactionMsg * transMsg);
      //virtual routerMsg *generateTransactionMessage(TransUnit TransUnit);
      //virtual routerMsg *generateAckMessage(routerMsg *msg, bool isTimeOutMsg = false);
          //generates ack message and destroys input parameter msg
      //virtual routerMsg *generateAckMessageFromTimeOutMsg(routerMsg *msg);
      virtual routerMsg *generateUpdateMessage(int transId, int receiver, double amount);
          //just generates routerMsg with no encapsulated msg inside
      virtual routerMsg *generateStatMessage();
      //virtual routerMsg *generateTimeOutMessage(routerMsg *transMsg);
      //virtual routerMsg *generateProbeMessage(int destNode, int pathIdx, vector<int> path);
      //virtual void splitTransactionForWaterfilling(routerMsg * ttMsg);
      virtual int myIndex();
      virtual void initialize() override;
      virtual void handleMessage(cMessage *msg) override;
      virtual void handleTransactionMessage(routerMsg *msg);
      virtual void handleTimeOutMessage(routerMsg *msg);
      virtual void handleAckMessage(routerMsg *msg);
      virtual void handleUpdateMessage(routerMsg *msg);
      virtual void handleStatMessage(routerMsg *msg);
      virtual void handleProbeMessage(routerMsg *msg);
      virtual bool forwardTransactionMessage(routerMsg *msg);
      virtual void forwardAckMessage(routerMsg *msg);
      virtual void forwardTimeOutMessage(routerMsg *msg);
      virtual void forwardProbeMessage(routerMsg *msg);
      virtual void sendUpdateMessage(routerMsg *msg);
      virtual void processTransUnits(int dest, vector<tuple<int, double , routerMsg *>>& q);
      //virtual void initializeProbes(vector<vector<int>> kShortestPaths, int destNode);
      virtual void deleteMessagesInQueues();


};



#endif
