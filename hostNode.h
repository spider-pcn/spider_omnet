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
        map<int, PaymentChannel> nodeToPaymentChannel;
        map<int, DestInfo> nodeToDestInfo; //one structure per destination;
              //TODO: incorporate the signals into nodeToDestInfo
      vector<int> statNumFailed = {};
      vector<int> statRateFailed = {};
 
        vector<int> statNumCompleted = {};
        vector<int> statNumArrived = {};
        vector<int> statRateCompleted = {};
        vector<int> statRateAttempted = {};
        vector<int> statNumTimedOut = {};
        vector<int> statNumTimedOutAtSender = {};
        vector<int> statRateArrived = {};
        vector<double> statProbabilities = {};

        //store shortest paths
        map<int, vector<int>> destNodeToPath = {};
        map<int, double> destNodeToLastMeasurementTime = {};

        //store k shortest paths in waterfilling
        map<int, map<int, PathInfo>> nodeToShortestPathsMap = {};       
        simsignal_t completionTimeSignal;
        vector<simsignal_t> rateCompletedPerDestSignals = {};
        vector<simsignal_t> rateAttemptedPerDestSignals = {};
        vector<simsignal_t> rateArrivedPerDestSignals = {};
        vector<simsignal_t> numCompletedPerDestSignals = {};
        vector<simsignal_t> numArrivedPerDestSignals = {};
        vector<simsignal_t> numTimedOutPerDestSignals = {};
        vector<simsignal_t> numPendingPerDestSignals = {};       
        vector<simsignal_t> numWaitingPerDestSignals = {};
        vector<simsignal_t> numTimedOutAtSenderSignals = {};
        vector<simsignal_t> probabilityPerDestSignals = {};


   vector<simsignal_t> rateFailedPerDestSignals = {};
      vector<simsignal_t> numFailedPerDestSignals = {};

        //signal showing which path index was chosen for each transaction
        vector<simsignal_t> pathPerTransPerDestSignals = {};       
        vector<simsignal_t> fracSuccessfulPerDestSignals = {};

        //set of transaction units WITH timeouts, that we already received acks for
        set<int> successfulDoNotSendTimeOut = {};       
        set<CanceledTrans> canceledTransactions = {};

        map<tuple<int,int>,AckState> transPathToAckState = {}; //key is (transactionId, routeIndex)
        map<int, int> transactionIdToNumHtlc = {}; //allows us to calculate the htlcIndex number
        map<int, int> destNodeToNumTransPending = {};
        map<int, AckState> transToAmtLeftToComplete = {};
        int numCleared = 0;
        simsignal_t numClearedSignal;

   protected:
      virtual int myIndex(); //if host node, return getIndex(), if routerNode, return getIndex()+numHostNodes
      virtual routerMsg *generateWaterfillingTransactionMessage(double amt, vector<int> path, int pathIndex, transactionMsg * transMsg);
      virtual routerMsg *generateWaterfillingTimeOutMessage( vector<int> path, int transactionId, int receiver);
      virtual routerMsg *generateTransactionMessage(TransUnit TransUnit);
      virtual routerMsg *generateAckMessage(routerMsg *msg, bool isSuccess = true);
          //generates ack message and destroys input parameter msg
      //virtual routerMsg *generateAckMessageFromTimeOutMsg(routerMsg *msg);
      virtual routerMsg *generateUpdateMessage(int transId, int receiver, double amount, int htlcIndex);
          //just generates routerMsg with no encapsulated msg inside
      virtual routerMsg *generateStatMessage();
      virtual routerMsg *generateClearStateMessage();
      virtual routerMsg *generateTimeOutMessage(routerMsg *transMsg);
      virtual routerMsg *generateProbeMessage(int destNode, int pathIdx, vector<int> path);
      virtual routerMsg *generateTriggerPriceUpdateMessage();
      virtual routerMsg *generatePriceUpdateMessage(double xLocal, int dest);
      virtual routerMsg *generateTriggerPriceQueryMessage();
      virtual routerMsg *generatePriceQueryMessage(vector<int> route, int routeIndex);
      virtual routerMsg *generateTriggerTransactionSendMessage(vector<int> route, int routeIndex, int destNode);

      virtual void splitTransactionForWaterfilling(routerMsg * ttMsg);
      virtual void initialize() override;
      virtual void handleMessage(cMessage *msg) override;
      virtual void handleTransactionMessage(routerMsg *msg);
      virtual bool handleTransactionMessageTimeOut(routerMsg *msg); //returns true if message was deleted
      virtual void handleTransactionMessageWaterfilling(routerMsg *msg);
      virtual void handleTransactionMessagePriceScheme(routerMsg *msg);
      virtual void handleTimeOutMessageShortestPath(routerMsg *msg);
      virtual void handleTimeOutMessageWaterfilling(routerMsg *msg);
      virtual void handleAckMessage(routerMsg *msg);
      virtual void handleAckMessageTimeOut(routerMsg *msg);
      virtual void handleAckMessageWaterfilling(routerMsg *msg);
      virtual void handleAckMessageShortestPath(routerMsg *msg);
      virtual void handleUpdateMessage(routerMsg *msg);
      virtual void handleStatMessage(routerMsg *msg);
      virtual void handleStatMessagePriceScheme(routerMsg *msg);
      virtual void handleProbeMessage(routerMsg *msg);
      virtual void handleClearStateMessage(routerMsg *msg);
      //virtual void handleClearStateMessagePriceScheme(routerMsg *msg);
      virtual void handleClearStateMessageWaterfilling(routerMsg *msg);

      virtual void handleTriggerPriceUpdateMessage(routerMsg *msg);
      virtual void handlePriceUpdateMessage(routerMsg* ttmsg);
      virtual void handleTriggerPriceQueryMessage(routerMsg *msg);
      virtual void handlePriceQueryMessage(routerMsg* ttmsg);
      virtual void handleTriggerTransactionSendMessage(routerMsg* ttmsg);
      virtual void handleAckMessagePriceScheme(routerMsg* ttmsg);
      virtual bool forwardTransactionMessage(routerMsg *msg);
      virtual void forwardAckMessage(routerMsg *msg);
      virtual void forwardTimeOutMessage(routerMsg *msg);
      virtual void forwardMessage(routerMsg *msg);
      virtual void forwardProbeMessage(routerMsg *msg);
      virtual void sendUpdateMessage(routerMsg *msg);
      virtual void processTransUnits(int dest, vector<tuple<int, double , routerMsg *, Id>>& q);
      virtual void initializeProbes(vector<vector<int>> kShortestPaths, int destNode); //takes a vector of routes, and puts them into the map
      virtual void initializePriceProbes(vector<vector<int>> kShortestPaths, int destNode);
      virtual void restartProbes(int destNode);
      virtual void deleteMessagesInQueues();

      //helper
      virtual bool printNodeToPaymentChannel();
      virtual int updatePathProbabilities(vector<double> bottleneckBalances, int destNode);
      virtual int sampleFromDistribution(vector<double> probabilities);
      virtual bool ratesFeasible(vector<PathRateTuple> actualRates, double demand);
      virtual vector<PathRateTuple> computeProjection(vector<PathRateTuple> recommendedRates, double demand);
};

#endif
