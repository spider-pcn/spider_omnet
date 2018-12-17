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



class routerNode : public cSimpleModule
{
   private:
      string topologyFile_;
      string workloadFile_;
      map<int, paymentChannel> nodeToPaymentChannel;
      //int statNumProcessed;
      //vector<int> statNumProcessed;
      vector<int> statNumCompleted;
      vector<int> statNumAttempted;
      map<int, vector<int>> destNodeToPath;
      //vector<int> statNumSent;
      //simsignal_t numInQueueSignal;
      //simsignal_t numProcessedSignal; //whenever something is removed from incoming or outgoing
      simsignal_t completionTimeSignal;
      //vector<simsignal_t> numInQueuePerChannelSignals;
      //vector<simsignal_t> numProcessedPerChannelSignals;
      vector<simsignal_t> numCompletedPerDestSignals;
      vector<simsignal_t> numAttemptedPerDestSignals;
      //vector<simsignal_t> balancePerChannelSignals;
      //TODO:
      //vector<simsignal_t> numSentPerChannelSignals;

      vector< transUnit > myTransUnits; //list of transUnits that have me as sender
      //map<int, cGate*> node_to_gate; //map that takes in index of node adjacent to me, returns gate to that node
      //map<int, double> node_to_balance; //map takes in index of adjacent node, returns outgoing balance
      //map<int, deque<tuple<int, double , routerMsg *>>> node_to_queued_trans_units;
      //map<int, double> want_ack_trans_units;
      //map<int, double> sent_ack_trans_units;
      //map<int, map<int, double>> incoming_trans_units;
      //map<int, map<int, double>> outgoing_trans_units;
          //map takes in index of adjacent node, returns queue of trans_units to send to that node
   protected:
      virtual routerMsg *generateTransactionMessage(transUnit transUnit);
      virtual routerMsg *generateAckMessage(routerMsg *msg);
      virtual routerMsg *generateUpdateMessage(int transId, int receiver, double amount);
          //just generates routerMsg with no encapsulated msg inside
      virtual routerMsg *generateStatMessage();

      virtual void initialize() override;
      virtual void handleMessage(cMessage *msg) override;
      virtual void handleTransactionMessage(routerMsg *msg);
      virtual void handleAckMessage(routerMsg *msg);
      virtual void handleUpdateMessage(routerMsg *msg);
      virtual void handleStatMessage(routerMsg *msg);
      virtual void forwardTransactionMessage(routerMsg *msg);
      virtual void forwardAckMessage(routerMsg *msg);
      virtual void sendUpdateMessage(routerMsg *msg);
      virtual void processTransUnits(int dest, vector<tuple<int, double , routerMsg *>>& q);
      //virtual string string_node_to_balance();
      //virtual void print_private_values();
      //virtual void print_message(routerMsg* msg);
};


//global parameters
extern vector<transUnit> transUnitList;
extern int numNodes;
//number of nodes in network
extern map<int, vector<pair<int,int>>> channels; //adjacency list format of graph edges of network
extern map<tuple<int,int>,double> balances;
extern double statRate;
//map of balances for each edge; key = <int,int> is <source, destination>

#endif
