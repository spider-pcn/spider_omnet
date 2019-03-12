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
#include <map>
#include <fstream>
#include "global.h"
#include "hostInitialize.h"

using namespace std;
using namespace omnetpp;

class hostNodeBase : public cSimpleModule {
    protected:
        int index; // node identifier
        map<int, PaymentChannel> nodeToPaymentChannel;
        map<int, DestInfo> nodeToDestInfo; //one structure per destination;
        set<int> destList; // list of destinations with non zero demand
             
        //TODO: incorporate the signals into nodeToDestInfo
        // statistic collection related variables
        map<int, int> statNumFailed = {};
        map<int, double> statAmtFailed = {};
        map<int, int> statRateFailed = {};
        map<int, int> statNumCompleted = {};
        map<int, double> statAmtCompleted = {};
        map<int, int> statNumArrived = {};
        map<int, int> statRateCompleted = {};
        map<int, double> statAmtArrived = {};
        map<int, double> statAmtAttempted = {};
        map<int, int> statRateAttempted = {};
        map<int, int> statNumTimedOut = {};
        map<int, int> statNumTimedOutAtSender = {};
        map<int, int> statRateArrived = {};
        map<int, double> statProbabilities = {};
        map<int, double> statCompletionTimes = {};
        int numCleared = 0;

        //store shortest paths 
        // TODO: remove the first one's use all together and use the second for shortest paths
        map<int, vector<int>> destNodeToPath = {};
        map<int, map<int, PathInfo>> nodeToShortestPathsMap = {};  

        // number of transactions pending to a given destination
        map<int, int> destNodeToNumTransPending = {};

        // signals for recording statistics above
        simsignal_t completionTimeSignal;
        simsignal_t numClearedSignal;

        map<int, simsignal_t> rateCompletedPerDestSignals = {};
        map<int, simsignal_t> rateAttemptedPerDestSignals = {};
        map<int, simsignal_t> rateArrivedPerDestSignals = {};
        map<int, simsignal_t> numCompletedPerDestSignals = {};
        map<int, simsignal_t> numArrivedPerDestSignals = {};
        map<int, simsignal_t> numTimedOutPerDestSignals = {};
        map<int, simsignal_t> numPendingPerDestSignals = {};       
        map<int, simsignal_t> rateFailedPerDestSignals = {};
        map<int, simsignal_t> numFailedPerDestSignals = {};
        map<int, simsignal_t> fracSuccessfulPerDestSignals = {};

        //set of transaction units WITH timeouts, that we already received acks for
        set<int> successfulDoNotSendTimeOut = {};       
        set<CanceledTrans> canceledTransactions = {};

        /****** state for splitting transactions into many HTLCs ********/
        // structure that tracks how much of a transaction has been completed 
        // this tracks it for every path, key is (transactionId, routeIndex)
        map<tuple<int,int>,AckState> transPathToAckState = {}; 
        // this is per transaction across all paths
        map<int, AckState> transToAmtLeftToComplete = {};
        //allows us to calculate the htlcIndex number
        map<int, int> transactionIdToNumHtlc = {};         
    
    protected:

    public:
        //if host node, return getIndex(), if routerNode, return getIndex()+numHostNodes
        virtual int myIndex();
        virtual void setIndex(int index);  


        //helper functions common to all algorithms
        virtual bool printNodeToPaymentChannel();
        virtual int sampleFromDistribution(vector<double> probabilities);
        virtual void generateNextTransaction();
        virtual simsignal_t registerSignalPerDestPath(string signalStart, 
              int pathIdx, int destNode);
        virtual simsignal_t registerSignalPerChannel(string signalStart, int id);
        virtual simsignal_t registerSignalPerDest(string signalStart, int destNode, 
                string suffix);

        // generators for the standard messages
        virtual routerMsg* generateTransactionMessageForPath(double amt, 
                vector<int> path, int pathIndex, transactionMsg* transMsg);
        virtual routerMsg *generateTransactionMessage(TransUnit TransUnit);
        virtual routerMsg *generateAckMessage(routerMsg *msg, bool isSuccess = true);
        virtual routerMsg *generateUpdateMessage(int transId, 
                int receiver, double amount, int htlcIndex);        
        virtual routerMsg *generateStatMessage();
        virtual routerMsg *generateClearStateMessage();
        virtual routerMsg* generateTimeOutMessageForPath(vector<int> path, int transactionId, int receiver);
        virtual routerMsg *generateTimeOutMessage(routerMsg *transMsg);
        virtual routerMsg *generateRebalanceMessage();
      

        // message handlers
        virtual void handleMessage(cMessage *msg) override;
        virtual void handleTransactionMessage(routerMsg *msg, bool revisit=false);
        virtual void handleTransactionMessageSpecialized(routerMsg *msg);
        //returns true if message was deleted
        virtual bool handleTransactionMessageTimeOut(routerMsg *msg); 
        virtual void handleTimeOutMessage(routerMsg *msg);
        virtual void handleAckMessageSpecialized(routerMsg *msg);

        
        virtual void handleAckMessage(routerMsg *msg);
        virtual void handleAckMessageTimeOut(routerMsg *msg);
        virtual void handleUpdateMessage(routerMsg *msg);
        virtual void handleStatMessage(routerMsg *msg);
        virtual void handleClearStateMessage(routerMsg *msg);
        virtual void handleRebalanceMessage(routerMsg *msg);       
 
        // message forwarders
        virtual bool forwardTransactionMessage(routerMsg *msg);
        virtual void forwardMessage(routerMsg *msg);

        // core simulator functions
        virtual void initialize() override;
        virtual void finish() override;
        virtual void processTransUnits(int dest, 
                vector<tuple<int, double , routerMsg *, Id>>& q);
        virtual void deleteMessagesInQueues();
      
        virtual void updateBalance(int destNode, double amtToAdd);
            //amtToAdd can be negative if we are decreasing
        virtual double rebalanceTotalAmtAtNode();
};

#endif
