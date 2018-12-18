

#include "initialize.h"
#include "routerNode.h"
#include <queue>


#define MSGSIZE 100

Define_Module(routerNode);


void routerNode::deleteMessagesInQueues(){
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

routerMsg * routerNode::generateProbeMessage(int destNode, int pathIdx, vector<int> path){

    char msgname[MSGSIZE];
    sprintf(msgname, "tic-%d-to-%d probeMsg [idx %d]", getIndex(), destNode, pathIdx);
    //int pathIndex;
     //   int sender;
      //  int receiver;
       // bool isReversed = false;
       // IntVector pathBalances;
    probeMsg *pMsg = new probeMsg(msgname);
    pMsg->setSender(getIndex());
    pMsg->setPathIndex(pathIdx);
    pMsg->setReceiver(destNode);
    pMsg->setIsReversed(false);
    vector<double> pathBalances;
    pMsg->setPathBalances(pathBalances);
    pMsg->setPath(path);

    sprintf(msgname, "tic-%d-to-%d router-probeMsg [idx %d]", getIndex(), destNode, pathIdx);
    routerMsg *rMsg = new routerMsg(msgname);
    rMsg->setRoute(path);

    rMsg->setHopCount(0);
    rMsg->setMessageType(PROBE_MSG);

    rMsg->encapsulate(pMsg);
    return rMsg;


}


void routerNode::forwardProbeMessage(routerMsg *msg){
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

double minVectorElemDouble(vector<double> v){
    double min = v[0];
    for (double d: v){
        if (d<min){
            min=d;
        }
    }
    return min;
}

void routerNode::handleProbeMessage(routerMsg* ttmsg){
    probeMsg *pMsg = check_and_cast<probeMsg *>(ttmsg->getEncapsulatedPacket());
    if (simTime()>10){
       ttmsg->decapsulate();
       delete pMsg;
       delete ttmsg;
       return;
    }



    bool isReversed = pMsg->getIsReversed();
    int nextDest = ttmsg->getRoute()[ttmsg->getHopCount()+1];

    if(ttmsg->getHopCount() <  ttmsg->getRoute().size()-1){ // are not at last index of route
        forwardProbeMessage(ttmsg);

    }
    else{ // are at last index of route check if is reversed == false

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
}



void routerNode::initializeProbes(vector<vector<int>> kShortestPaths, int destNode){ //maybe less than k routes
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
 *      - initialize global parameters: instantiate all transUnits, and add to global list "trans_unit_list"
 *      - create "channels" map - adjacency list representation of network
 *      - create "balances" map - map representing initial balances; each key is directed edge (source, dest)
 *          and value is balance
 *  all nodes:
 *      - find my transUnits from "trans_unit_list" and store them into my private list "my_trans_units"
 *      - create "node_to_gate" map - map to identify channels, each key is node index adjacent to this node
 *          value is gate connecting to that adjacent node
 *      - create "node_to_balances" map - map to identify remaining outgoing balance for gates/channels connected
 *          to this node (key is adjacent node index; value is remaining outgoing balance)
 *      - create "node_to_queued_trans_units" map - map to get transUnit queue for outgoing transUnits,
 *          one queue corresponding to each adjacent node/each connected channel
 *      - send all transUnits in my_trans_units as a message (using generateTransactionMessage), sent to myself (node index),
 *          scheduled to arrive at timeSent parameter field in transUnit
 */


void routerNode::initialize()
{
   string topologyFile_ = par("topologyFile");
   string workloadFile_ = par("workloadFile");

   completionTimeSignal = registerSignal("completionTime");

   successfulDoNotSendTimeOut = {};

   if (getIndex() == 0){  //main initialization for global parameters
       useWaterfilling = true;

      if (useWaterfilling){
          kValue = 3;
      }


      setNumNodes(topologyFile_); //TODO: condense into generate_trans_unit_list
      // add all the transUnits into global list
      generateTransUnitList(workloadFile_, transUnitList);

      //create "channels" map - from topology file
      //create "balances" map - from topology file
      generateChannelsBalancesMap(topologyFile_, channels, balances );
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
         nodeToPaymentChannel[nextGate->getOwnerModule()->getIndex()] = temp;
      }
   } while (i < gateSize);

   //initialize everything for adjacent nodes/nodes with payment channel to me
   for(auto iter = nodeToPaymentChannel.begin(); iter != nodeToPaymentChannel.end(); ++iter)
   {
      int key =  iter->first; //node

      //fill in balance field of nodeToPaymentChannel
      nodeToPaymentChannel[key].balance = balances[make_tuple(getIndex(),key)];

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

   //initialize signals with all other nodes in graph
   for (int i = 0; i < numNodes; ++i) {
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

   //iterate through the global trans_unit_list and find my transUnits
   for (int i=0; i<(int)transUnitList.size(); i++){
      if (transUnitList[i].sender == getIndex()){
         myTransUnits.push_back(transUnitList[i]);
      }
   }


   //implementing timeSent parameter, send all msgs at beginning
   for (int i=0 ; i<(int)myTransUnits.size(); i++){
      transUnit j = myTransUnits[i];
      double timeSent = j.timeSent;
      routerMsg *msg = generateTransactionMessage(j);
      if (useWaterfilling){
          vector<int> blankPath = {};
          msg->setRoute(blankPath);
      }

      scheduleAt(timeSent, msg);

      if (useWaterfilling){
          if (nodeToShortestPathsMap.count(j.receiver) == 0 ){

             vector<vector<int>> kShortestRoutes = getKShortestRoutes(j.sender, j.receiver, kValue);

             initializeProbes(kShortestRoutes, j.receiver);

          }
      }

      if (j.hasTimeOut && !useWaterfilling){ //TODO : timeOut implementation is not going to work with waterfilling
         routerMsg *toutMsg = generateTimeOutMessage(msg);
         scheduleAt(timeSent+j.timeOut, toutMsg );
      }
   }

   //set statRate
   statRate = 0.5;
   //get stat message
   routerMsg *statMsg = generateStatMessage();
   scheduleAt(0, statMsg);

}

void routerNode::handleMessage(cMessage *msg)
{
   routerMsg *ttmsg = check_and_cast<routerMsg *>(msg);
   if (ttmsg->getMessageType()==ACK_MSG){ //preprocessor constants defined in routerMsg_m.h
      //cout << "[NODE "<< getIndex() <<": RECEIVED ACK MSG] \n" <<endl;
      //print_message(ttmsg);
      //print_private_values();
      handleAckMessage(ttmsg);
      //cout << "[AFTER HANDLING:]\n" <<endl;
      // print_private_values();
   }
   else if(ttmsg->getMessageType()==TRANSACTION_MSG){
      //cout<< "[NODE "<< getIndex() <<": RECEIVED TRANSACTION MSG]  \n"<<endl;
      // print_message(ttmsg);
      // print_private_values();
      handleTransactionMessage(ttmsg);
      //cout<< "[AFTER HANDLING:] \n"<<endl;
      // print_private_values();
   }
   else if(ttmsg->getMessageType()==UPDATE_MSG){
      //cout<< "[NODE "<< getIndex() <<": RECEIVED UPDATE MSG] \n"<<endl;
      // print_message(ttmsg);
      // print_private_values();
      handleUpdateMessage(ttmsg);
      //cout<< "[AFTER HANDLING:]  \n"<<endl;
      //print_private_values();

   }
   else if (ttmsg->getMessageType() ==STAT_MSG){
      //cout<< "[NODE "<< getIndex() <<": RECEIVED STAT MSG] \n"<<endl;
      handleStatMessage(ttmsg);
      //cout<< "[AFTER HANDLING:]  \n"<<endl;
   }
   else if (ttmsg->getMessageType() == TIME_OUT_MSG){
      //cout<< "[NODE "<< getIndex() <<": RECEIVED TIME_OUT_MSG] \n"<<endl;
      handleTimeOutMessage(ttmsg);
      //cout<< "[AFTER HANDLING:]  \n"<<endl;
   }
   else if (ttmsg->getMessageType() == PROBE_MSG){
       handleProbeMessage(ttmsg);
       //TODO:
   }

}


routerMsg *routerNode::generateStatMessage(){
   char msgname[MSGSIZE];
   sprintf(msgname, "node-%d statMsg", getIndex());
   routerMsg *rMsg = new routerMsg(msgname);
   rMsg->setMessageType(STAT_MSG);
   return rMsg;
}



void routerNode::handleStatMessage(routerMsg* ttmsg){
   if (simTime() >10){
      delete ttmsg;
      deleteMessagesInQueues();
   }
   else{
      scheduleAt(simTime()+statRate, ttmsg);
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


   for (auto it = 0; it<numNodes; it++){ //iterate through all nodes in graph
      emit(numAttemptedPerDestSignals[it], statNumAttempted[it]);

      emit(numCompletedPerDestSignals[it], statNumCompleted[it]);
   }

}


void routerNode::forwardTimeOutMessage(routerMsg* msg){
   // Increment hop count.
   msg->setHopCount(msg->getHopCount()+1);
   //use hopCount to find next destination
   int nextDest = msg->getRoute()[msg->getHopCount()];
   //ackMsg *aMsg = check_and_cast<ackMsg *>(msg->getEncapsulatedPacket());

   send(msg, nodeToPaymentChannel[nextDest].gate);

}

/*
 *  handleTimeOutMessage -
 *  - if no incoming transaction, (transMsg will be deleted when arriving to next dest, or when existing current queue)
 *  - if has incoming transaction, check outgoing transaction.
 *  - if has outgoing transaction, increment back funds delete outgoing transaction and forward ttmsg
 *  - if no outgoing transaction, (generate ack back) delete ttmsg (stuck in queue and will be)
 *  - Note: has outgoing transaction means you need to increment back funds
 */

void routerNode::handleTimeOutMessage(routerMsg* ttmsg){

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
            routerMsg* msg = generateAckMessage(ttmsg, true);
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
               routerMsg* msg = generateAckMessage(ttmsg, true);
               forwardAckMessage(msg);

            }
         }

      }
      else{
         cout<<"here9" <<endl;
         //no incoming transaction, we can (generate ack back) delete ttmsg
         routerMsg* msg = generateAckMessage(ttmsg, true);
         forwardAckMessage(msg);
      }

   }//end else (is self messsage)

}


/* handleAckMessage - passes on ack message if not at end of root else deletes it
 *      - generates and sends corresponding update message
 *      - erases transId on received ack msg from "want_ack_trans_units"
 *      - adds transId to "sent_ack_trans_units", list of transUnits waiting for
 *          balance update message with their transId (in forwardAckMessage())
 */
void routerNode::handleAckMessage(routerMsg* ttmsg){
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
         //remove transaction from incoming transunits
         //map<int, double> *incomingTransUnits = &(nodeToPaymentChannel[nextNode].outgoingTransUnits);
         // (*incomingTransUnits).erase(aMsg->getTransactionId());

         forwardAckMessage(ttmsg);
      }
      else{ //at last index of route and not successful

         //int destNode = ttmsg->getRoute()[0]; //destination of origin transUnit job

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

         int destNode = ttmsg->getRoute()[0]; //destination of origin transUnit job

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
routerMsg *routerNode::generateAckMessage(routerMsg* ttmsg, bool isTimeOutMsg){ //default is false
   bool isSuccess;
   int transactionId;
   double timeSent;
   double amount;
   int sender = (ttmsg->getRoute())[0];
   int receiver = (ttmsg->getRoute())[(ttmsg->getRoute()).size() -1];
   bool hasTimeOut;

   if (isTimeOutMsg){
      cout<<"a here" << endl;
      timeOutMsg *toutMsg = check_and_cast<timeOutMsg *>(ttmsg->getEncapsulatedPacket());
      transactionId = toutMsg->getTransactionId();
      timeSent = toutMsg->getTimeSentTransUnit();
      amount = toutMsg->getAmount();
      isSuccess = false;
      ttmsg->decapsulate();
      delete toutMsg;
      hasTimeOut = true;

   }
   else{ //is transaction message

      transactionMsg *transMsg = check_and_cast<transactionMsg *>(ttmsg->getEncapsulatedPacket());
      transactionId = transMsg->getTransactionId();
      timeSent = transMsg->getTimeSent();
      amount = transMsg->getAmount();
      isSuccess = true;
      ttmsg->decapsulate();

      hasTimeOut = transMsg->getHasTimeOut();

      delete transMsg;

   }
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
void routerNode::forwardAckMessage(routerMsg *msg){
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
void routerNode::handleUpdateMessage(routerMsg* msg){
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


/*
 *  generateUpdateMessage - creates new message to send as updateMessage
 *      - note: all update messages have route length of exactly 2
 *
 */
routerMsg *routerNode::generateUpdateMessage(int transId, int receiver, double amount){
   char msgname[MSGSIZE];

   sprintf(msgname, "tic-%d-to-%d updateMsg", getIndex(), receiver);

   routerMsg *rMsg = new routerMsg(msgname);

   vector<int> route={getIndex(),receiver};
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
void routerNode::sendUpdateMessage(routerMsg *msg){
   // Increment hop count.
   msg->setHopCount(msg->getHopCount()+1);
   //use hopCount to find next destination
   int nextDest = msg->getRoute()[msg->getHopCount()];
   send(msg, nodeToPaymentChannel[nextDest].gate);
}

routerMsg* routerNode::generateWaterfillingTransactionMessage(double amt, vector<int> path, transactionMsg* transMsg){

    char msgname[MSGSIZE];
    sprintf(msgname, "tic-%d-to-%d water-transMsg", getIndex(), transMsg->getReceiver());
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


       sprintf(msgname, "tic-%d-to-%d water-routerMsg", getIndex(), transMsg->getReceiver());
       routerMsg *rMsg = new routerMsg(msgname);
       rMsg->setRoute(path);


       rMsg->setHopCount(0);
       rMsg->setMessageType(TRANSACTION_MSG);
       rMsg->encapsulate(msg);
       return rMsg;

}

void routerNode::splitTransactionForWaterfilling(routerMsg * ttmsg){
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
void routerNode::handleTransactionMessage(routerMsg* ttmsg){
   int hopcount = ttmsg->getHopCount();
   vector<tuple<int, double , routerMsg *>> *q;
   transactionMsg *transMsg = check_and_cast<transactionMsg *>(ttmsg->getEncapsulatedPacket());

   int destination = transMsg->getReceiver();

   //check if message is already failed: (1) past time()>timeSent+timeOut (2) on failedTransUnit list
   if ( transMsg->getHasTimeOut() && (simTime() > (transMsg->getTimeSent() + transMsg->getTimeOut()))  ){

      //delete yourself
      ttmsg->decapsulate();
      delete transMsg;
      delete ttmsg;
   }
   else{
      if ((ttmsg->getHopCount())>0){ //not a self-message, add to incoming_trans_units
         int prevNode = ttmsg->getRoute()[ttmsg->getHopCount()-1];
         map<int, double> *incomingTransUnits = &(nodeToPaymentChannel[prevNode].incomingTransUnits);
         (*incomingTransUnits)[transMsg->getTransactionId()] = transMsg->getAmount();
      }
      else{
         //is a self-message
         int destNode = transMsg->getReceiver();
         statNumAttempted[destNode] = statNumAttempted[destNode]+1;

         //cout << "WATERFILLING 1" <<endl;
         if(useWaterfilling && (ttmsg->getRoute().size() == 0)){ //no route is specified -
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



      }

      if(ttmsg->getHopCount() ==  ttmsg->getRoute().size()-1){ // are at last index of route

         //cout << "1 HERE" << endl;
         //cout <<"transMsg hasTimeOUt: " << transMsg->getHasTimeOut() <<endl;
         routerMsg* newMsg =  generateAckMessage(ttmsg);
         //forward ack message - no need to wait;
         forwardAckMessage(newMsg);
      }
      else{
         int nextNode = ttmsg->getRoute()[hopcount+1];
         q = &(nodeToPaymentChannel[nextNode].queuedTransUnits);

         (*q).push_back(make_tuple(transMsg->getPriorityClass(), transMsg->getAmount(),
                  ttmsg));
         push_heap((*q).begin(), (*q).end(), sortPriorityThenAmtFunction);

         processTransUnits(nextNode, *q);
      }
   }
}



routerMsg *routerNode::generateTimeOutMessage(routerMsg* msg)
{
   transactionMsg *transMsg = check_and_cast<transactionMsg *>(msg->getEncapsulatedPacket());
   char msgname[MSGSIZE];

   sprintf(msgname, "tic-%d-to-%d timeOutMsg", transMsg->getSender(), transMsg->getReceiver());
   timeOutMsg *toutMsg = new timeOutMsg(msgname);
   toutMsg->setAmount(transMsg->getAmount());
   toutMsg->setTransactionId(transMsg->getTransactionId());
   toutMsg->setTimeSentTransUnit(transMsg->getTimeSent());

   sprintf(msgname, "tic-%d-to-%d routerTimeOutMsg", transMsg->getSender(), transMsg->getReceiver());
   routerMsg *rMsg = new routerMsg(msgname);
   rMsg->setRoute(msg->getRoute());
   rMsg->setHopCount(0);
   rMsg->setMessageType(TIME_OUT_MSG);
   rMsg->encapsulate(toutMsg);
   return rMsg;
}


/* generateTransactionMessage - takes in a transUnit object and returns corresponding routerMsg message
 *      with encapsulated transactionMsg inside.
 *      note: calls get_route function to get route from sender to receiver
 */
routerMsg *routerNode::generateTransactionMessage(transUnit transUnit)
{
   char msgname[MSGSIZE];
   sprintf(msgname, "tic-%d-to-%d transactionMsg", transUnit.sender, transUnit.receiver);
   transactionMsg *msg = new transactionMsg(msgname);
   msg->setAmount(transUnit.amount);
   msg->setTimeSent(transUnit.timeSent);
   msg->setSender(transUnit.sender);
   msg->setReceiver(transUnit.receiver);
   msg->setPriorityClass(transUnit.priorityClass);
   msg->setTransactionId(msg->getId());
   msg->setHasTimeOut(transUnit.hasTimeOut);

   msg->setTimeOut(transUnit.timeOut);

   sprintf(msgname, "tic-%d-to-%d routerMsg", transUnit.sender, transUnit.receiver);
   routerMsg *rMsg = new routerMsg(msgname);
   if (destNodeToPath.count(transUnit.receiver) == 0){ //compute route and add to memoization table
      vector<int> route = getRoute(transUnit.sender,transUnit.receiver);
      destNodeToPath[transUnit.receiver] = route;
      rMsg->setRoute(route);
   }
   else{
      rMsg->setRoute(destNodeToPath[transUnit.receiver]);
   }

   rMsg->setHopCount(0);
   rMsg->setMessageType(TRANSACTION_MSG);

   rMsg->encapsulate(msg);
   return rMsg;
}


/*
 * processTransUnits - given an adjacent node, and transUnit queue of things to send to that node, sends
 *  transUnits until channel funds are too low by calling forwardMessage on message representing transUnit
 */
void routerNode:: processTransUnits(int dest, vector<tuple<int, double , routerMsg *>>& q){
   bool successful = true;

   while((int)q.size()>0 && successful){
      successful = forwardTransactionMessage(get<2>(q.back()));
      if (successful){
         q.pop_back();
      }
   }
}


/*
 *  forwardTransactionMessage - given a message representing a transUnit, increments hopCount, finds next destination,
 *      adjusts (decrements) channel balance, sends message to next node on route
 */
bool routerNode::forwardTransactionMessage(routerMsg *msg)
{
   transactionMsg *transMsg = check_and_cast<transactionMsg *>(msg->getEncapsulatedPacket());

   //cout <<"forwardTransactionMesg: " << transMsg->getHasTimeOut() << endl;

   if (transMsg->getHasTimeOut() && (simTime() > (transMsg->getTimeSent() + transMsg->getTimeOut()))){
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
