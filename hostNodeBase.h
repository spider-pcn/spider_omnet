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



class hostNodeBase : public cSimpleModule {
    protected:
        int index; // node identifier
        unordered_map<int, PaymentChannel> nodeToPaymentChannel;
        unordered_map<int, DestInfo> nodeToDestInfo; //one structure per destination;
        unordered_set<int> destList; // list of destinations with non zero demand
             
        // statistic collection related variables
        unordered_map<int, int> statNumFailed = {};
        unordered_map<int, double> statAmtFailed = {};
        unordered_map<int, int> statRateFailed = {};
        unordered_map<int, int> statNumCompleted = {};
        unordered_map<int, double> statAmtCompleted = {};
        unordered_map<int, int> statNumArrived = {};
        unordered_map<int, int> statRateCompleted = {};
        unordered_map<int, double> statAmtArrived = {};
        unordered_map<int, double> statAmtAttempted = {};
        unordered_map<int, int> statRateAttempted = {};
        unordered_map<int, int> statNumTimedOut = {};
        unordered_map<int, int> statNumTimedOutAtSender = {};
        unordered_map<int, int> statRateArrived = {};
        unordered_map<int, double> statProbabilities = {};
        unordered_map<int, double> statCompletionTimes = {};
        priority_queue<double, vector<double>, greater<double>> statNumTries;
        double maxPercentileHeapSize;
        int numCleared = 0;

        //store shortest paths 
        unordered_map<int, vector<int>> destNodeToPath = {};
        unordered_map<int, unordered_map<int, PathInfo>> nodeToShortestPathsMap = {};  

        // number of transactions pending to a given destination
        unordered_map<int, int> destNodeToNumTransPending = {};

        // signals for recording statistics above
        simsignal_t completionTimeSignal;
        simsignal_t numClearedSignal;

        unordered_map<int, simsignal_t> rateCompletedPerDestSignals = {};
        unordered_map<int, simsignal_t> rateAttemptedPerDestSignals = {};
        unordered_map<int, simsignal_t> rateArrivedPerDestSignals = {};
        unordered_map<int, simsignal_t> numCompletedPerDestSignals = {};
        unordered_map<int, simsignal_t> numArrivedPerDestSignals = {};
        unordered_map<int, simsignal_t> numTimedOutPerDestSignals = {};
        unordered_map<int, simsignal_t> numPendingPerDestSignals = {};       
        unordered_map<int, simsignal_t> rateFailedPerDestSignals = {};
        unordered_map<int, simsignal_t> numFailedPerDestSignals = {};
        unordered_map<int, simsignal_t> fracSuccessfulPerDestSignals = {};

        //set of transaction units WITH timeouts, that we already received acks for
        set<int> successfulDoNotSendTimeOut = {};   
        unordered_set<CanceledTrans, hashCanceledTrans, equalCanceledTrans> canceledTransactions = {};

        /****** state for splitting transactions into many HTLCs ********/
        // structure that tracks how much of a transaction has been completed 
        // this tracks it for every path, key is (transactionId, routeIndex)
        unordered_map<tuple<int,int>,AckState, hashId> transPathToAckState = {}; 
        // this is per transaction across all paths
        unordered_map<int, AckState> transToAmtLeftToComplete = {};
        //allows us to calculate the htlcIndex number
        unordered_map<int, int> transactionIdToNumHtlc = {};       

        // structure to enable rebalancing - keeps track of how much each sender has sent to you, so you can refund 
        // accordingly and how muh you've sent to each receiver
        unordered_map<int, double> senderToAmtRefundable = {};  
        unordered_map<int, double> receiverToAmtRefunded = {};  
    
    protected:

    public:
        //if host node, return getIndex(), if routerNode, return getIndex()+numHostNodes
        virtual int myIndex();
        virtual void setIndex(int index);  


        //helper functions common to all algorithms
        virtual bool printNodeToPaymentChannel();
        virtual int sampleFromDistribution(vector<double> probabilities);
        virtual double getTotalAmount(int x);
        virtual double getTotalAmountIncomingInflight(int x);
        virtual double getTotalAmountOutgoingInflight(int x);
        virtual void setPaymentChannelBalanceByNode(int node, double balance);

        virtual void pushIntoSenderQueue(DestInfo* destInfo, routerMsg* msg);
        virtual void deleteTransaction(routerMsg* msg);
        virtual void generateNextTransaction();
        virtual simsignal_t registerSignalPerDestPath(string signalStart, 
              int pathIdx, int destNode);
        virtual simsignal_t registerSignalPerChannel(string signalStart, int id);
        virtual simsignal_t registerSignalPerDest(string signalStart, int destNode, 
                string suffix);
        virtual simsignal_t registerSignalPerChannelPerDest(string signalStart,
              int pathIdx, int destNode);
        virtual void recordTailCompletionTime(simtime_t timeSent, double amount, double completionTime);

        // generators for the standard messages
        virtual routerMsg* generateTransactionMessageForPath(double amt, 
                vector<int> path, int pathIndex, transactionMsg* transMsg);
        virtual routerMsg *generateTransactionMessage(TransUnit TransUnit);
        virtual routerMsg *generateDuplicateTransactionMessage(ackMsg* aMsg);
        virtual routerMsg *generateAckMessage(routerMsg *msg, bool isSuccess = true);
        virtual routerMsg *generateUpdateMessage(int transId, 
                int receiver, double amount, int htlcIndex);        
        virtual routerMsg *generateStatMessage();
        virtual routerMsg *generateComputeMinBalanceMessage();
        virtual routerMsg *generateClearStateMessage();
        virtual routerMsg* generateTimeOutMessageForPath(vector<int> path, int transactionId, int receiver);
        virtual routerMsg *generateTimeOutMessage(routerMsg *transMsg);
        virtual routerMsg *generateTriggerRebalancingMessage();
        virtual routerMsg *generateAddFundsMessage(map<int,double> fundsToBeAdded);
      

        // message handlers
        virtual void handleMessage(cMessage *msg) override;
        virtual void handleTransactionMessage(routerMsg *msg, bool revisit=false);
        virtual void handleTransactionMessageSpecialized(routerMsg *msg);
        //returns true if message was deleted
        virtual bool handleTransactionMessageTimeOut(routerMsg *msg); 
        virtual void handleTimeOutMessage(routerMsg *msg);
        virtual void handleAckMessageSpecialized(routerMsg *msg);
        virtual void handleTriggerRebalancingMessage(routerMsg* ttmsg);
        virtual void handleComputeMinAvailableBalanceMessage(routerMsg* ttmsg);
        virtual void handleAddFundsMessage(routerMsg* ttmsg);

        
        virtual void handleAckMessage(routerMsg *msg);
        virtual void handleAckMessageTimeOut(routerMsg *msg);
        virtual void handleUpdateMessage(routerMsg *msg);
        virtual void handleStatMessage(routerMsg *msg);
        virtual void handleClearStateMessage(routerMsg *msg);
        
        // message forwarders
        virtual int forwardTransactionMessage(routerMsg *msg, int dest, simtime_t arrivalTime);
        virtual void forwardMessage(routerMsg *msg);

        // core simulator functions
        virtual void initialize() override;
        virtual void finish() override;
        virtual bool processTransUnits(int dest, 
                vector<tuple<int, double , routerMsg *, Id, simtime_t>>& q);
        virtual void deleteMessagesInQueues();
};

#endif
