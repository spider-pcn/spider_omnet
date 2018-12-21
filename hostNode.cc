#include "hostNode.h"
#include <queue>
#include "hostInitialize.h"

//global parameters
vector<TransUnit> _transUnitList; //list of all transUnits
int _numNodes;
int _numRouterNodes;
int _numHostNodes;
double _maxTravelTime;
//number of nodes in network
map<int, vector<pair<int,int>>> _channels; //adjacency list format of graph edges of network
map<tuple<int,int>,double> _balances;
//map of balances for each edge; key = <int,int> is <source, destination>
double _statRate;
double _clearRate;
//bool withFailures;
bool _useWaterfilling;
int _kValue;
double _simulationLength; //TODO: need to set this in hostInitialize

#define MSGSIZE 100

Define_Module(hostNode);

int hostNode::myIndex(){
    return getIndex();
}

void hostNode::deleteMessagesInQueues(){
   for (auto iter = nodeToPaymentChannel.begin(); iter!=nodeToPaymentChannel.end(); iter++){
      int key = iter->first;

      for (auto temp = (nodeToPaymentChannel[key].queuedTransUnits).begin();
            temp!= (nodeToPaymentChannel[key].queuedTransUnits).end(); temp++){
         routerMsg * rMsg = get<2>(*temp);
         auto tMsg = rMsg->getEncapsulatedPacket();
         rMsg->decapsulate();
         delete tMsg;
         delete rMsg;
      }
   }
}

routerMsg * hostNode::generateProbeMessage(int destNode, int pathIdx, vector<int> path){

    char msgname[MSGSIZE];
    sprintf(msgname, "tic-%d-to-%d probeMsg [idx %d]", myIndex(), destNode, pathIdx);
    //int pathIndex;
     //   int sender;
      //  int receiver;
       // bool isReversed = false;
       // IntVector pathBalances;
    probeMsg *pMsg = new probeMsg(msgname);
    pMsg->setSender(myIndex());
    pMsg->setPathIndex(pathIdx);
    pMsg->setReceiver(destNode);
    pMsg->setIsReversed(false);
    vector<double> pathBalances;
    pMsg->setPathBalances(pathBalances);
    pMsg->setPath(path);

    sprintf(msgname, "tic-%d-to-%d router-probeMsg [idx %d]", myIndex(), destNode, pathIdx);
    routerMsg *rMsg = new routerMsg(msgname);
    rMsg->setRoute(path);

    rMsg->setHopCount(0);
    rMsg->setMessageType(PROBE_MSG);

    rMsg->encapsulate(pMsg);
    return rMsg;


}


void hostNode::forwardProbeMessage(routerMsg *msg){
   // Increment hop count.
   msg->setHopCount(msg->getHopCount()+1);
   //use hopCount to find next destination
   int nextDest = msg->getRoute()[msg->getHopCount()];

   probeMsg *pMsg = check_and_cast<probeMsg *>(msg->getEncapsulatedPacket());

   if (pMsg->getIsReversed() == false){
       vector<double> *pathBalances = & ( pMsg->getPathBalances());
       (*pathBalances).push_back(nodeToPaymentChannel[nextDest].balance);
   }

   send(msg, nodeToPaymentChannel[nextDest].gate);
}


void hostNode::handleProbeMessage(routerMsg* ttmsg){
    probeMsg *pMsg = check_and_cast<probeMsg *>(ttmsg->getEncapsulatedPacket());
    if (simTime()> _simulationLength ){
       ttmsg->decapsulate();
       delete pMsg;
       delete ttmsg;
       return;
    }

    bool isReversed = pMsg->getIsReversed();
      int nextDest = ttmsg->getRoute()[ttmsg->getHopCount()+1];
     if (isReversed == true){ //store times into private map, delete message

            //TODO
            int pathIdx = pMsg->getPathIndex();

            int destNode = pMsg->getReceiver();

            PathInfo * p = &(nodeToShortestPathsMap[destNode][pathIdx]);
            assert(p->path == pMsg->getPath());

            p->lastUpdated = simTime();
            //cout << "number of probes: " << nodeToShortestPathsMap[destNode].size();
            for (auto idx: nodeToShortestPathsMap[destNode]){
                //cout << "lastUpdated: " << (idx.second).lastUpdated << endl;

            }
            vector<double> pathBalances = pMsg->getPathBalances();
            int bottleneck = minVectorElemDouble(pathBalances);

            p->bottleneck = bottleneck ;
            p->pathBalances = pathBalances;


            //reset the probe message
            vector<int> route = ttmsg->getRoute();
            reverse(route.begin(), route.end());
            ttmsg->setRoute(route);
            ttmsg->setHopCount(0);
            pMsg->setIsReversed(false);
            vector<double> tempPathBal = {};
            pMsg->setPathBalances(tempPathBal);
            forwardProbeMessage(ttmsg);
        }
        else{ //reverse and send message again
            pMsg->setIsReversed(true);
            ttmsg->setHopCount(0);
            vector<int> route = ttmsg->getRoute();
            reverse(route.begin(), route.end());
            ttmsg->setRoute(route);
            forwardProbeMessage(ttmsg);

        }


}



void hostNode::initializeProbes(vector<vector<int>> kShortestPaths, int destNode){ //maybe less than k routes
    //cout << "number of probes: " << kShortestPaths.size() << endl;
    for (int pathIdx = 0; pathIdx < kShortestPaths.size(); pathIdx++){
        //map<int, map<int, PathInfo>> nodeToShortestPathsMap;
        PathInfo temp = {};
        nodeToShortestPathsMap[destNode][pathIdx] = temp;
        nodeToShortestPathsMap[destNode][pathIdx].path = kShortestPaths[pathIdx];

        routerMsg * msg = generateProbeMessage(destNode, pathIdx, kShortestPaths[pathIdx]);
        forwardProbeMessage(msg);
    }
    //cout << "number of probes2: " << kShortestPaths.size() << endl;

}



/* initialize() -
 *  if node index is 0:
 *      - initialize global parameters: instantiate all TransUnits, and add to global list "trans_unit_list"
 *      - create "channels" map - adjacency list representation of network
 *      - create "balances" map - map representing initial balances; each key is directed edge (source, dest)
 *          and value is balance
 *  all nodes:
 *      - find my TransUnits from "trans_unit_list" and store them into my private list "my_trans_units"
 *      - create "node_to_gate" map - map to identify channels, each key is node index adjacent to this node
 *          value is gate connecting to that adjacent node
 *      - create "node_to_balances" map - map to identify remaining outgoing balance for gates/channels connected
 *          to this node (key is adjacent node index; value is remaining outgoing balance)
 *      - create "node_to_queued_trans_units" map - map to get TransUnit queue for outgoing TransUnits,
 *          one queue corresponding to each adjacent node/each connected channel
 *      - send all TransUnits in my_trans_units as a message (using generateTransactionMessage), sent to myself (node index),
 *          scheduled to arrive at timeSent parameter field in TransUnit
 */
void hostNode::initialize()
{
   string topologyFile_ = par("topologyFile");
   string workloadFile_ = par("workloadFile");

   completionTimeSignal = registerSignal("completionTime");

   successfulDoNotSendTimeOut = {};

   if (getIndex() == 0){  //main initialization for global parameters

       _maxTravelTime = 0.2;
       //set statRate
       _statRate = 0.5;
       _clearRate = 0.5;


       _useWaterfilling = true;

      if (_useWaterfilling){
          _kValue = 3;
      }


      setNumNodes(topologyFile_); //TODO: condense into generate_trans_unit_list
      // add all the TransUnits into global list
      generateTransUnitList(workloadFile_, _transUnitList);

      //create "channels" map - from topology file
      //create "balances" map - from topology file
      generateChannelsBalancesMap(topologyFile_, _channels, _balances );
   }

   //Create nodeToPaymentChannel map
   const char * gateName = "out";
   cGate *destGate = NULL;

   int i = 0;
   int gateSize = gate(gateName, 0)->size();

   do {
      destGate = gate(gateName, i++);
      cGate *nextGate = destGate->getNextGate();
      if (nextGate ) {
         PaymentChannel temp =  {};
         temp.gate = destGate;


         bool isHost = nextGate->getOwnerModule()->par("isHost");
                 int key = nextGate->getOwnerModule()->getIndex();
                 if (!isHost){
                     key = key + _numHostNodes;
                 }
                 nodeToPaymentChannel[key] = temp; //TODO: change to myIndex
      }
   } while (i < gateSize);

   //initialize everything for adjacent nodes/nodes with payment channel to me
   for(auto iter = nodeToPaymentChannel.begin(); iter != nodeToPaymentChannel.end(); ++iter)
   {
      int key =  iter->first; //node

      //fill in balance field of nodeToPaymentChannel
      nodeToPaymentChannel[key].balance = _balances[make_tuple(myIndex(),key)];

      //initialize queuedTransUnits
      vector<tuple<int, double , routerMsg *>> temp;
      make_heap(temp.begin(), temp.end(), sortPriorityThenAmtFunction);
      nodeToPaymentChannel[key].queuedTransUnits = temp;

      //register signals
      char signalName[64];
      simsignal_t signal;
      cProperty *statisticTemplate;

      //numInQueuePerChannel signal
      sprintf(signalName, "numInQueuePerChannel(node %d)", i);
      signal = registerSignal(signalName);
      statisticTemplate = getProperties()->get("statisticTemplate", "numInQueuePerChannelTemplate");
      getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
      nodeToPaymentChannel[key].numInQueuePerChannelSignal = signal;

      //numProcessedPerChannel signal
      sprintf(signalName, "numProcessedPerChannel(node %d)", i);
      signal = registerSignal(signalName);
      statisticTemplate = getProperties()->get("statisticTemplate", "numProcessedPerChannelTemplate");
      getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
      nodeToPaymentChannel[key].numProcessedPerChannelSignal = signal;

      //statNumProcessed int
      nodeToPaymentChannel[key].statNumProcessed = 0;

      //balancePerChannel signal
      sprintf(signalName, "balancePerChannel(node %d)", i);
      signal = registerSignal(signalName);
      statisticTemplate = getProperties()->get("statisticTemplate", "balancePerChannelTemplate");
      getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
      nodeToPaymentChannel[key].balancePerChannelSignal = signal;

      //numSentPerChannel signal
      sprintf(signalName, "numSentPerChannel(node %d)", i);
      signal = registerSignal(signalName);
      statisticTemplate = getProperties()->get("statisticTemplate", "numSentPerChannelTemplate");
      getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
      nodeToPaymentChannel[key].numSentPerChannelSignal = signal;

      //statNumSent int
      nodeToPaymentChannel[key].statNumSent = 0;
   }

   cout << "hostNode1: "<<myIndex() << endl;

   //initialize signals with all other nodes in graph
   for (int i = 0; i < _numHostNodes; ++i) {
      char signalName[64];
      simsignal_t signal;
      cProperty *statisticTemplate;

      //numCompletedPerDest signal

      sprintf(signalName, "numCompletedPerDest_Total(dest node %d)", i);
      signal = registerSignal(signalName);
      statisticTemplate = getProperties()->get("statisticTemplate", "numCompletedPerDestTemplate");
      getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
      numCompletedPerDestSignals.push_back(signal);

      statNumCompleted.push_back(0);

      //numAttemptedPerDest signal
      sprintf(signalName, "numAttemptedPerDest_Total(dest node %d)", i);
      signal = registerSignal(signalName);
      statisticTemplate = getProperties()->get("statisticTemplate", "numAttemptedPerDestTemplate");
      getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
      numAttemptedPerDestSignals.push_back(signal);

      statNumAttempted.push_back(0);

   }
   cout << "hostNode2: "<<myIndex() << endl;

   //iterate through the global trans_unit_list and find my TransUnits
   for (int i=0; i<(int)_transUnitList.size(); i++){
      if (_transUnitList[i].sender == myIndex()){
         myTransUnits.push_back(_transUnitList[i]);
      }
   }

   cout << "hostNode3: "<<myIndex() << endl;

   //implementing timeSent parameter, send all msgs at beginning
   for (int i=0 ; i<(int)myTransUnits.size(); i++){
      TransUnit j = myTransUnits[i];
      double timeSent = j.timeSent;
      routerMsg *msg = generateTransactionMessage(j);
      if (_useWaterfilling){
          vector<int> blankPath = {};
          msg->setRoute(blankPath);
      }
      cout << "hostNode3.1: "<<myIndex() << endl;
      cout <<"timeSent: "<< timeSent << endl;
      scheduleAt(timeSent, msg);

      if (_useWaterfilling){
          cout << "hostNode3.11: "<<myIndex() << endl;
          if (nodeToShortestPathsMap.count(j.receiver) == 0 ){
              cout << "hostNode3.12: "<<myIndex() << endl;
             vector<vector<int>> kShortestRoutes = getKShortestRoutes(j.sender, j.receiver, _kValue);
             cout << "hostNode3.13: "<<myIndex() << endl;
             initializeProbes(kShortestRoutes, j.receiver);
             cout << "hostNode3.14: "<<myIndex() << endl;
          }
      }

      cout << "hostNode3.2: "<<myIndex() << endl;
      if (j.hasTimeOut){ //TODO : timeOut implementation is not going to work with waterfilling
         routerMsg *toutMsg = generateTimeOutMessage(msg);
         cout <<"here start" <<endl;
         cout <<"timeSent+j.timeOut: "<< timeSent+j.timeOut << endl;
         scheduleAt(timeSent+j.timeOut, toutMsg );
         cout <<"here end" << endl;
      }

      cout << "hostNode3.3: "<<myIndex() << endl;
   }
   cout << "hostNode4: "<<myIndex() << endl;


   //get stat message
   routerMsg *statMsg = generateStatMessage();
   cout << " simTime() + 0 : "<< simTime() + 0 << endl;
   scheduleAt(simTime()+0, statMsg);
   cout << "end schedule stat" << endl;

   routerMsg *clearStateMsg = generateClearStateMessage();
     scheduleAt(simTime()+_clearRate, clearStateMsg);

   cout << "end hostNode " << myIndex() << " initialization" << endl;

}

void hostNode::handleMessage(cMessage *msg)
{
   routerMsg *ttmsg = check_and_cast<routerMsg *>(msg);
   if (ttmsg->getMessageType()==ACK_MSG){ //preprocessor constants defined in routerMsg_m.h
      //cout << "[NODE "<< myIndex() <<": RECEIVED ACK MSG] \n" <<endl;
      //print_message(ttmsg);
      //print_private_values();
      handleAckMessage(ttmsg);
      //cout << "[AFTER HANDLING:]\n" <<endl;
      // print_private_values();
   }
   else if(ttmsg->getMessageType()==TRANSACTION_MSG){
      //cout<< "[NODE "<< myIndex() <<": RECEIVED TRANSACTION MSG]  \n"<<endl;
      // print_message(ttmsg);
      // print_private_values();
      handleTransactionMessage(ttmsg);
      //cout<< "[AFTER HANDLING:] \n"<<endl;
      // print_private_values();
   }
   else if(ttmsg->getMessageType()==UPDATE_MSG){
      //cout<< "[NODE "<< myIndex() <<": RECEIVED UPDATE MSG] \n"<<endl;
      // print_message(ttmsg);
      // print_private_values();
      handleUpdateMessage(ttmsg);
      //cout<< "[AFTER HANDLING:]  \n"<<endl;
      //print_private_values();

   }
   else if (ttmsg->getMessageType() ==STAT_MSG){
      //cout<< "[NODE "<< myIndex() <<": RECEIVED STAT MSG] \n"<<endl;
      handleStatMessage(ttmsg);
      //cout<< "[AFTER HANDLING:]  \n"<<endl;
   }
   else if (ttmsg->getMessageType() == TIME_OUT_MSG){
      //cout<< "[NODE "<< myIndex() <<": RECEIVED TIME_OUT_MSG] \n"<<endl;
      handleTimeOutMessage(ttmsg);
      //cout<< "[AFTER HANDLING:]  \n"<<endl;
   }
   else if (ttmsg->getMessageType() == PROBE_MSG){
       handleProbeMessage(ttmsg);
       //TODO:
   }

}


routerMsg *hostNode::generateStatMessage(){
   char msgname[MSGSIZE];
   sprintf(msgname, "node-%d statMsg", myIndex());
   routerMsg *rMsg = new routerMsg(msgname);
   rMsg->setMessageType(STAT_MSG);
   return rMsg;
}



void hostNode::handleStatMessage(routerMsg* ttmsg){
   if (simTime() > _simulationLength){
      delete ttmsg;
      deleteMessagesInQueues();
   }
   else{
       cout << "handleStatMsg start" << endl;
      scheduleAt(simTime()+_statRate, ttmsg);
      cout << "handleStatMsg end" << endl;

   }

   for ( auto it = nodeToPaymentChannel.begin(); it!= nodeToPaymentChannel.end(); it++){ //iterate through all adjacent nodes

      int node = it->first; //key
      emit(nodeToPaymentChannel[node].numInQueuePerChannelSignal, (nodeToPaymentChannel[node].queuedTransUnits).size());

      emit(nodeToPaymentChannel[node].balancePerChannelSignal, nodeToPaymentChannel[node].balance);

      emit(nodeToPaymentChannel[node].numProcessedPerChannelSignal, nodeToPaymentChannel[node].statNumProcessed);
      nodeToPaymentChannel[node].statNumProcessed = 0;

      emit(nodeToPaymentChannel[node].numSentPerChannelSignal, nodeToPaymentChannel[node].statNumSent);
      nodeToPaymentChannel[node].statNumSent = 0;
   }


   for (auto it = 0; it<_numHostNodes; it++){ //iterate through all nodes in graph
      emit(numAttemptedPerDestSignals[it], statNumAttempted[it]);

      emit(numCompletedPerDestSignals[it], statNumCompleted[it]);
   }

}


void hostNode::forwardTimeOutMessage(routerMsg* msg){
   // Increment hop count.
   msg->setHopCount(msg->getHopCount()+1);
   //use hopCount to find next destination
   int nextDest = msg->getRoute()[msg->getHopCount()];
   //ackMsg *aMsg = check_and_cast<ackMsg *>(msg->getEncapsulatedPacket());

   send(msg, nodeToPaymentChannel[nextDest].gate);

}

/*
 *  handleTimeOutMessage -
 */

void hostNode::handleTimeOutMessage(routerMsg* ttmsg){ //TODO: fix handleTimeOutMessage

   timeOutMsg *toutMsg = check_and_cast<timeOutMsg *>(ttmsg->getEncapsulatedPacket());

   if ((ttmsg->getHopCount())==0){ //is a self message
      cout<<"here1" <<endl;

      if (successfulDoNotSendTimeOut.count(toutMsg->getTransactionId())>0){ //already received ack for it, do not send out
         cout<<"here2" <<endl;
         successfulDoNotSendTimeOut.erase(toutMsg->getTransactionId());
         ttmsg->decapsulate();
         delete toutMsg;
         delete ttmsg;
      }
      else{
         cout<<"here3" <<endl;
         int nextNode = (ttmsg->getRoute())[ttmsg->getHopCount()+1];

         //remove from outgoing transactions
         map<int, double> *outgoingTransUnits = &(nodeToPaymentChannel[nextNode].outgoingTransUnits);
         (*outgoingTransUnits).erase(toutMsg->getTransactionId());

         //increment back amount
         nodeToPaymentChannel[nextNode].balance = nodeToPaymentChannel[nextNode].balance + toutMsg->getAmount();
         forwardTimeOutMessage(ttmsg);
      }

   }
   else{ //not a self message
      cout<<"here4" <<endl;
      int prevNode = ttmsg->getRoute()[ttmsg->getHopCount()-1];

      map<int, double> *incomingTransUnits = &(nodeToPaymentChannel[prevNode].incomingTransUnits);

      //check for incoming transaction
      if ((*incomingTransUnits).count(toutMsg->getTransactionId())>0){ //has incoming transaction
         cout<<"here5" <<endl;
         //if has incoming, decrement balance towards incoming/prev node
         nodeToPaymentChannel[prevNode].balance = nodeToPaymentChannel[prevNode].balance - toutMsg->getAmount();
         (*incomingTransUnits).erase(toutMsg->getTransactionId());

         if ( ttmsg->getHopCount() == (ttmsg->getRoute()).size()-1) { //at last stop
            cout<<"here6" <<endl;
            routerMsg* msg = generateAckMessage(ttmsg);
            forwardAckMessage(msg);

         }
         else{
            cout<<"here7" <<endl;
            int nextNode = ttmsg->getRoute()[ttmsg->getHopCount()+1];
            map<int, double> *outgoingTransUnits = &(nodeToPaymentChannel[nextNode].outgoingTransUnits);
            if ((*outgoingTransUnits).count(toutMsg->getTransactionId())>0){ //has outgoing transaction
               //if has outgoing, increment balance towards outgoing/next node
               nodeToPaymentChannel[nextNode].balance = nodeToPaymentChannel[nextNode].balance + toutMsg->getAmount();
               (*outgoingTransUnits).erase(toutMsg->getTransactionId());
               forwardTimeOutMessage(ttmsg);

            }
            else{
               cout<<"here8" <<endl;

               //transaction is stuck in queue
               routerMsg* msg = generateAckMessage(ttmsg);
               forwardAckMessage(msg);

            }
         }

      }
      else{
         cout<<"here9" <<endl;
         //no incoming transaction, we can (generate ack back) delete ttmsg
         routerMsg* msg = generateAckMessage(ttmsg);
         forwardAckMessage(msg);
      }

   }//end else (is self messsage)

}


/* handleAckMessage
 */
void hostNode::handleAckMessage(routerMsg* ttmsg){ //TODO: fix handleAckMessage
   //generate updateMsg
   int prevNode = ttmsg->getRoute()[ttmsg->getHopCount()-1];

   //remove transaction from outgoing_trans_unit
   map<int, double> *outgoingTransUnits = &(nodeToPaymentChannel[prevNode].outgoingTransUnits);
   ackMsg *aMsg = check_and_cast<ackMsg *>(ttmsg->getEncapsulatedPacket());

   (*outgoingTransUnits).erase(aMsg->getTransactionId());

   //increment signal numProcessed
   nodeToPaymentChannel[prevNode].statNumProcessed = nodeToPaymentChannel[prevNode].statNumProcessed+1;

   if (aMsg->getIsSuccess() == false){ //not successful ackMsg is just forwarded back to sender

      if(ttmsg->getHopCount() <  ttmsg->getRoute().size()-1){ // are not at last index of route
         // int nextNode = (ttmsg->getRoute())[ttmsg->getHopCount()+1];
         //remove transaction from incoming TransUnits
         //map<int, double> *incomingTransUnits = &(nodeToPaymentChannel[nextNode].outgoingTransUnits);
         // (*incomingTransUnits).erase(aMsg->getTransactionId());

         forwardAckMessage(ttmsg);
      }
      else{ //at last index of route and not successful

         //int destNode = ttmsg->getRoute()[0]; //destination of origin TransUnit job

         //TODO: introduce some statNumFailed
         //statNumCompleted[destNode] = statNumCompleted[destNode]+1;


         //broadcast completion time signal, TODO: call completionTime for notSuccesful (just timeOut + travel time)
         //simtime_t timeTakenInMilli = 1000*(simTime() - aMsg->getTimeSent());
         //emit(completionTimeSignal, timeTakenInMilli);

         //delete ack message
         ttmsg->decapsulate();
         delete aMsg;
         delete ttmsg;

      }
   }
   else{ //successful transaction

      routerMsg* uMsg =  generateUpdateMessage(aMsg->getTransactionId(), prevNode, aMsg->getAmount() );
      sendUpdateMessage(uMsg);

      if(ttmsg->getHopCount() <  ttmsg->getRoute().size()-1){ // are not at last index of route
         forwardAckMessage(ttmsg);
      }
      else{ //at last index of route
         //add transactionId to set of successful transactions that we don't need to send time out messages
         if (aMsg->getHasTimeOut()){
            //cout << "HAS TIME OUT" << endl;
            successfulDoNotSendTimeOut.insert(aMsg->getTransactionId());
         }

         int destNode = ttmsg->getRoute()[0]; //destination of origin TransUnit job

         statNumCompleted[destNode] = statNumCompleted[destNode]+1;

         //broadcast completion time signal
         //ackMsg *aMsg = check_and_cast<ackMsg *>(ttmsg->getEncapsulatedPacket());
         simtime_t timeTakenInMilli = 1000*(simTime() - aMsg->getTimeSent());

         emit(completionTimeSignal, timeTakenInMilli);

         //delete ack message
         ttmsg->decapsulate();
         delete aMsg;
         delete ttmsg;
      }
   }
}

/*
 * generateAckMessage - called only when a transactionMsg reaches end of its path, keeps routerMsg casing of msg
 *   and reuses it to send ackMsg in reversed order of route
 */
routerMsg *hostNode::generateAckMessage(routerMsg* ttmsg){ //default is false
   bool isSuccess;
   int transactionId;
   double timeSent;
   double amount;
   int sender = (ttmsg->getRoute())[0];
   int receiver = (ttmsg->getRoute())[(ttmsg->getRoute()).size() -1];
   bool hasTimeOut;

      transactionMsg *transMsg = check_and_cast<transactionMsg *>(ttmsg->getEncapsulatedPacket());
      transactionId = transMsg->getTransactionId();
      timeSent = transMsg->getTimeSent();
      amount = transMsg->getAmount();
      isSuccess = true;
      ttmsg->decapsulate();

      hasTimeOut = transMsg->getHasTimeOut();

      delete transMsg;


   char msgname[MSGSIZE];

   sprintf(msgname, "receiver-%d-to-sender-%d ackMsg", receiver, sender);
   routerMsg *msg = new routerMsg(msgname);
   ackMsg *aMsg = new ackMsg(msgname);
   aMsg->setTransactionId(transactionId);
   aMsg->setIsSuccess(isSuccess);
   aMsg->setTimeSent(timeSent);
   aMsg->setAmount(amount);
   aMsg->setHasTimeOut(hasTimeOut);
   //no need to set secret;
   vector<int> route=ttmsg->getRoute();
   reverse(route.begin(), route.end());
   msg->setRoute(route);
   msg->setHopCount((route.size()-1) - ttmsg->getHopCount());
   //need to reverse path from current hop number in case of partial failure
   msg->setMessageType(ACK_MSG); //now an ack message
   // remove encapsulated packet
   delete ttmsg;
   msg->encapsulate(aMsg);

   return msg;
}


/*
 * forwardAckMessage - called inside handleAckMsg, sends ackMsg to next destination, and

 *      adds transId to sent_ack_trans_units list
 */
void hostNode::forwardAckMessage(routerMsg *msg){
   // Increment hop count.
   msg->setHopCount(msg->getHopCount()+1);
   //use hopCount to find next destination
   int nextDest = msg->getRoute()[msg->getHopCount()];
   //ackMsg *aMsg = check_and_cast<ackMsg *>(msg->getEncapsulatedPacket());

   send(msg, nodeToPaymentChannel[nextDest].gate);
}

/*
 * handleUpdateMessage - called when receive update message, increment back funds, see if we can
 *      process more jobs with new funds, delete update message
 */
void hostNode::handleUpdateMessage(routerMsg* msg){
   vector<tuple<int, double , routerMsg *>> *q;
   int prevNode = msg->getRoute()[msg->getHopCount()-1];

   updateMsg *uMsg = check_and_cast<updateMsg *>(msg->getEncapsulatedPacket());
   //increment the in flight funds back
   nodeToPaymentChannel[prevNode].balance =  nodeToPaymentChannel[prevNode].balance + uMsg->getAmount();

   //remove transaction from incoming_trans_units
   map<int, double> *incomingTransUnits = &(nodeToPaymentChannel[prevNode].incomingTransUnits);

   (*incomingTransUnits).erase(uMsg->getTransactionId());

   nodeToPaymentChannel[prevNode].statNumProcessed = nodeToPaymentChannel[prevNode].statNumProcessed + 1;

   msg->decapsulate();
   delete uMsg;
   delete msg; //delete update message

   //see if we can send more jobs out
   q = &(nodeToPaymentChannel[prevNode].queuedTransUnits);

   processTransUnits(prevNode, *q);

}


routerMsg *hostNode::generateClearStateMessage(){
   char msgname[MSGSIZE];
   sprintf(msgname, "node-%d clearStateMsg", myIndex());
   routerMsg *rMsg = new routerMsg(msgname);
   rMsg->setMessageType(CLEAR_STATE_MSG);
   return rMsg;
}

/*
 *  generateUpdateMessage - creates new message to send as updateMessage
 *      - note: all update messages have route length of exactly 2
 *
 */
routerMsg *hostNode::generateUpdateMessage(int transId, int receiver, double amount){
   char msgname[MSGSIZE];

   sprintf(msgname, "tic-%d-to-%d updateMsg", myIndex(), receiver);

   routerMsg *rMsg = new routerMsg(msgname);

   vector<int> route={myIndex(),receiver};
   rMsg->setRoute(route);
   rMsg->setHopCount(0);
   rMsg->setMessageType(UPDATE_MSG);

   updateMsg *uMsg = new updateMsg(msgname);

   uMsg->setAmount(amount);
   uMsg->setTransactionId(transId);
   rMsg->encapsulate(uMsg);
   return rMsg;
}


/*
 * forwardUpdateMessage - sends updateMessage to appropriate destination
 */
void hostNode::sendUpdateMessage(routerMsg *msg){
   // Increment hop count.
   msg->setHopCount(msg->getHopCount()+1);
   //use hopCount to find next destination
   int nextDest = msg->getRoute()[msg->getHopCount()];
   send(msg, nodeToPaymentChannel[nextDest].gate);
}

routerMsg* hostNode::generateWaterfillingTransactionMessage(double amt, vector<int> path, transactionMsg* transMsg){

    char msgname[MSGSIZE];
    sprintf(msgname, "tic-%d-to-%d water-transMsg", myIndex(), transMsg->getReceiver());
     transactionMsg *msg = new transactionMsg(msgname);
      msg->setAmount(amt);
      msg->setTimeSent(transMsg->getTimeSent());
       msg->setSender(transMsg->getSender());
       msg->setReceiver(transMsg->getReceiver());
       msg->setPriorityClass(transMsg->getPriorityClass());
       msg->setTransactionId(msg->getId());
       msg->setHasTimeOut(false); //TODO: all waterfilling transactions do not have time out rn , transMsg->hasTimeOut);

       msg->setTimeOut(-1);
       msg->setParentTransactionId(transMsg->getTransactionId());


       sprintf(msgname, "tic-%d-to-%d water-routerMsg", myIndex(), transMsg->getReceiver());
       routerMsg *rMsg = new routerMsg(msgname);
       rMsg->setRoute(path);


       rMsg->setHopCount(0);
       rMsg->setMessageType(TRANSACTION_MSG);
       rMsg->encapsulate(msg);
       return rMsg;

}

void hostNode::splitTransactionForWaterfilling(routerMsg * ttmsg){
    transactionMsg *transMsg = check_and_cast<transactionMsg *>(ttmsg->getEncapsulatedPacket());
    int destNode = transMsg->getReceiver();
    double remainingAmt = transMsg->getAmount();

                map<int, double> pathMap = {}; //key is pathIdx, double is amt
                priority_queue<pair<double,int>> pq;
                for (auto iter: nodeToShortestPathsMap[destNode] ){
                    int key = iter.first;
                    double bottleneck = (iter.second).bottleneck;
                    pq.push(make_pair(bottleneck, key)); //automatically sorts with biggest first index-element
                    //cout << "pathLength: " << (iter.second).path.size() << endl;
                }

                double highestBal;
                double secHighestBal;
                double diffToSend;
                double amtToSend;
                int highestBalIdx;
                while(pq.size()>0 && remainingAmt > 0){
                    highestBal = get<0>(pq.top());
                    highestBalIdx = get<1>(pq.top());
                    pq.pop();
                    if (pq.size()==0){
                        secHighestBal=0;
                    }
                    else{
                        secHighestBal = get<0>(pq.top());
                    }
                    diffToSend = highestBal - secHighestBal;
                    amtToSend = min(remainingAmt/(pq.size()+1),diffToSend);

                    for (auto p: pathMap){
                        pathMap[p.first] = p.second + amtToSend;
                        remainingAmt = remainingAmt - amtToSend;
                    }
                    pathMap[highestBalIdx] = amtToSend;
                    remainingAmt = remainingAmt - amtToSend;

                }

                if (remainingAmt > 0){
                    //TODO: update the amt in the transactionMsg (keep transactionId?)

                }
                else{

                    for (auto p: pathMap){
                        //cout <<"("<<p.first<<","<<p.second<<") ";
                        vector<int> path = nodeToShortestPathsMap[destNode][p.first].path;
                        double amt = p.second;
                        routerMsg* waterMsg = generateWaterfillingTransactionMessage(amt, path, transMsg); //TODO:
                        //cout << "waterMsg generated "<<endl;
                        handleTransactionMessage(waterMsg);
                    }
                    //cout<<endl;
                    //cout << "number of probes after water gen: " <<  nodeToShortestPathsMap[destNode].size();

                }
         transMsg->setAmount(remainingAmt);


}

/* handleTransactionMessage - checks if message has arrived
 *      1. has arrived - turn transactionMsg into ackMsg, forward ackMsg
 *      2. has not arrived - add to appropriate job queue q, process q as
 *          much as we have funds for
 */
void hostNode::handleTransactionMessage(routerMsg* ttmsg){
   int hopcount = ttmsg->getHopCount();
   vector<tuple<int, double , routerMsg *>> *q;
   transactionMsg *transMsg = check_and_cast<transactionMsg *>(ttmsg->getEncapsulatedPacket());

   int destination = transMsg->getReceiver();

   //check if message is already failed: (1) past time()>timeSent+timeOut (2) on failedTransUnit list
   if ( transMsg->getHasTimeOut() && (simTime() > (transMsg->getTimeSent() + transMsg->getTimeOut()))  ){
       //TODO: check to see if transactionId is in canceledTransactions set

      //delete yourself
      ttmsg->decapsulate();
      delete transMsg;
      delete ttmsg;
   }
   else{


       if(ttmsg->getHopCount() ==  ttmsg->getRoute().size()-1){ // are at last index of route
         int prevNode = ttmsg->getRoute()[ttmsg->getHopCount()-1];
         map<int, double> *incomingTransUnits = &(nodeToPaymentChannel[prevNode].incomingTransUnits);
         (*incomingTransUnits)[transMsg->getTransactionId()] = transMsg->getAmount();
               //cout << "1 HERE" << endl;
                //cout <<"transMsg hasTimeOUt: " << transMsg->getHasTimeOut() <<endl;
                routerMsg* newMsg =  generateAckMessage(ttmsg);
                //forward ack message - no need to wait;
                forwardAckMessage(newMsg);


      }
      else{
         //is a self-message/at hop count = 0
         int destNode = transMsg->getReceiver();
         statNumAttempted[destNode] = statNumAttempted[destNode]+1;

         //cout << "WATERFILLING 1" <<endl;
         if(_useWaterfilling && (ttmsg->getRoute().size() == 0)){ //no route is specified -
                 //means we need to break up into chunks and assign route
             //cout << "WATERFILLING 2" <<endl;

             bool recent = probesRecent(nodeToShortestPathsMap[destNode]);

             //cout << "recent " << recent;
             if (recent){ // we made sure all the probes are back

                 splitTransactionForWaterfilling(ttmsg);



                 ttmsg->decapsulate();
                 delete transMsg;
                 delete ttmsg;
                 //delete transMsg?
                 return;

             }
             else{
                 //cout << "WATERFILLING 3" << endl;
                 scheduleAt(simTime() + 1, ttmsg);
                 return;
             }

         }
         //cout << "WATERFILLING 3" <<endl;
         int nextNode = ttmsg->getRoute()[hopcount+1];
                 q = &(nodeToPaymentChannel[nextNode].queuedTransUnits);

                 (*q).push_back(make_tuple(transMsg->getPriorityClass(), transMsg->getAmount(),
                          ttmsg));
                 push_heap((*q).begin(), (*q).end(), sortPriorityThenAmtFunction);

                 processTransUnits(nextNode, *q);


      }





   }
}



routerMsg *hostNode::generateTimeOutMessage(routerMsg* msg)
{
   transactionMsg *transMsg = check_and_cast<transactionMsg *>(msg->getEncapsulatedPacket());
   char msgname[MSGSIZE];

   sprintf(msgname, "tic-%d-to-%d timeOutMsg", transMsg->getSender(), transMsg->getReceiver());
   timeOutMsg *toutMsg = new timeOutMsg(msgname);
   toutMsg->setAmount(transMsg->getAmount());
   toutMsg->setTransactionId(transMsg->getTransactionId());


   sprintf(msgname, "tic-%d-to-%d routerTimeOutMsg", transMsg->getSender(), transMsg->getReceiver());
   routerMsg *rMsg = new routerMsg(msgname);
   rMsg->setRoute(msg->getRoute());
   rMsg->setHopCount(0);
   rMsg->setMessageType(TIME_OUT_MSG);
   rMsg->encapsulate(toutMsg);
   return rMsg;
}


/* generateTransactionMessage - takes in a TransUnit object and returns corresponding routerMsg message
 *      with encapsulated transactionMsg inside.
 *      note: calls get_route function to get route from sender to receiver
 */
routerMsg *hostNode::generateTransactionMessage(TransUnit TransUnit)
{
   char msgname[MSGSIZE];
   sprintf(msgname, "tic-%d-to-%d transactionMsg", TransUnit.sender, TransUnit.receiver);
   transactionMsg *msg = new transactionMsg(msgname);
   msg->setAmount(TransUnit.amount);
   msg->setTimeSent(TransUnit.timeSent);
   msg->setSender(TransUnit.sender);
   msg->setReceiver(TransUnit.receiver);
   msg->setPriorityClass(TransUnit.priorityClass);
   msg->setTransactionId(msg->getId());
   msg->setHasTimeOut(TransUnit.hasTimeOut);

   msg->setTimeOut(TransUnit.timeOut);

   sprintf(msgname, "tic-%d-to-%d routerMsg", TransUnit.sender, TransUnit.receiver);
   routerMsg *rMsg = new routerMsg(msgname);
   if (destNodeToPath.count(TransUnit.receiver) == 0){ //compute route and add to memoization table
      vector<int> route = getRoute(TransUnit.sender,TransUnit.receiver);
      destNodeToPath[TransUnit.receiver] = route;
      rMsg->setRoute(route);
   }
   else{
      rMsg->setRoute(destNodeToPath[TransUnit.receiver]);
   }

   rMsg->setHopCount(0);
   rMsg->setMessageType(TRANSACTION_MSG);

   rMsg->encapsulate(msg);
   return rMsg;
}


/*
 * processTransUnits - given an adjacent node, and TransUnit queue of things to send to that node, sends
 *  TransUnits until channel funds are too low by calling forwardMessage on message representing TransUnit
 */
void hostNode:: processTransUnits(int dest, vector<tuple<int, double , routerMsg *>>& q){
   bool successful = true;

   while((int)q.size()>0 && successful){
      successful = forwardTransactionMessage(get<2>(q.back()));
      if (successful){
         q.pop_back();
      }
   }
}


/*
 *  forwardTransactionMessage - given a message representing a TransUnit, increments hopCount, finds next destination,
 *      adjusts (decrements) channel balance, sends message to next node on route
 */
bool hostNode::forwardTransactionMessage(routerMsg *msg)
{
   transactionMsg *transMsg = check_and_cast<transactionMsg *>(msg->getEncapsulatedPacket());

   //cout <<"forwardTransactionMesg: " << transMsg->getHasTimeOut() << endl;

   if (transMsg->getHasTimeOut() && (simTime() > (transMsg->getTimeSent() + transMsg->getTimeOut()))){
       //TODO: change to check if transactionId is in canceledTransactions set
      msg->decapsulate();
      delete transMsg;
      delete msg;
      return true;
   }
   else{

      int nextDest = msg->getRoute()[msg->getHopCount()+1];

      if (transMsg->getAmount() >nodeToPaymentChannel[nextDest].balance){
         return false;
      }
      else{
         // Increment hop count.
         msg->setHopCount(msg->getHopCount()+1);
         //use hopCount to find next destination

         //add amount to outgoing map

         map<int, double> *outgoingTransUnits = &(nodeToPaymentChannel[nextDest].outgoingTransUnits);
         (*outgoingTransUnits)[transMsg->getTransactionId()] = transMsg->getAmount();

         //numSentPerChannel incremented every time (key,value) pair added to outgoing_trans_units map
         nodeToPaymentChannel[nextDest].statNumSent =  nodeToPaymentChannel[nextDest].statNumSent+1;

         int amt = transMsg->getAmount();
         nodeToPaymentChannel[nextDest].balance = nodeToPaymentChannel[nextDest].balance - amt;

         //int transId = transMsg->getTransactionId();

         send(msg, nodeToPaymentChannel[nextDest].gate);
         return true;
      } //end else (transMsg->getAmount() >nodeToPaymentChannel[dest].balance)
   }// end else (transMsg->getHasTimeOut() && (simTime() > (transMsg->getTimeSent() + transMsg->getTimeOut())

}

