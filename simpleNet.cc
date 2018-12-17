

#include "initialize.h"
#include "routerNode.h"

#define MSGSIZE 100

Define_Module(routerNode);


void routerNode::deleteMessagesInQueues(){
    for (auto iter = nodeToPaymentChannel.begin(); iter!=nodeToPaymentChannel.end(); iter++){
        int key = iter->first;

        //vector<tuple<int, double , routerMsg *>> *q;
        //q = &(nodeToPaymentChannel[prevNode].queuedTransUnits);
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


   if (getIndex() == 0){  //main initialization for global parameters

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
         paymentChannel temp =  {};
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
      scheduleAt(timeSent, msg);
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
      // EV<< "[NODE "<< getIndex() <<": RECEIVED ACK MSG] \n";
      //print_message(ttmsg);
      //print_private_values();
      handleAckMessage(ttmsg);
      // EV<< "[AFTER HANDLING:]\n";
      // print_private_values();
   }
   else if(ttmsg->getMessageType()==TRANSACTION_MSG){
      // EV<< "[NODE "<< getIndex() <<": RECEIVED TRANSACTION MSG]  \n";
      // print_message(ttmsg);
      // print_private_values();
      handleTransactionMessage(ttmsg);
      // EV<< "[AFTER HANDLING:] \n";
      // print_private_values();
   }
   else if(ttmsg->getMessageType()==UPDATE_MSG){
      //  EV<< "[NODE "<< getIndex() <<": RECEIVED UPDATE MSG] \n";
      // print_message(ttmsg);
      // print_private_values();
      handleUpdateMessage(ttmsg);
      // EV<< "[AFTER HANDLING:]  \n";
      //print_private_values();

   }
   else if (ttmsg->getMessageType() ==STAT_MSG){
      handleStatMessage(ttmsg);
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
   if (simTime() >20){
      delete ttmsg;
      deleteMessagesInQueues();

   }
   else{
      scheduleAt(simTime()+statRate, ttmsg);
   }


   //  map<int, deque<tuple<int, double , routerMsg *>>> node_to_queued_trans_units;
   for ( auto it = nodeToPaymentChannel.begin(); it!= nodeToPaymentChannel.end(); it++){ //iterate through all adjacent nodes
      //EV << "node "<< getIndex() << ": queued "<<(it->second).size();
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

   // virtual routerMsg *generateUpdateMessage(int transId, int receiver, double amount);
   routerMsg* uMsg =  generateUpdateMessage(aMsg->getTransactionId(), prevNode, aMsg->getAmount() );

   sendUpdateMessage(uMsg);

   if(ttmsg->getHopCount() <  ttmsg->getRoute().size()-1){ // are not at last index of route
      forwardAckMessage(ttmsg);
   }
   else{

      int destNode = ttmsg->getRoute()[0]; //destination of origin transUnit job

      statNumCompleted[destNode] = statNumCompleted[destNode]+1;

      //broadcast completion time signal
      ackMsg *aMsg = check_and_cast<ackMsg *>(ttmsg->getEncapsulatedPacket());
      simtime_t timeTakenInMilli = 1000*(simTime() - aMsg->getTimeSent());

      emit(completionTimeSignal, timeTakenInMilli);




      //delete ack message
      ttmsg->decapsulate();
      delete aMsg;
      delete ttmsg;

   }
}

/*
 * generateAckMessage - called only when a transactionMsg reaches end of its path, keeps routerMsg casing of msg
 *   and reuses it to send ackMsg in reversed order of route
 */
routerMsg *routerNode::generateAckMessage(routerMsg* ttmsg){
   transactionMsg *transMsg = check_and_cast<transactionMsg *>(ttmsg->getEncapsulatedPacket());
   char msgname[MSGSIZE];
   sprintf(msgname, "receiver-%d-to-sender-%d ackMsg", transMsg->getReceiver(), transMsg->getSender());
   routerMsg *msg = new routerMsg(msgname);
   ackMsg *aMsg = new ackMsg(msgname);
   aMsg->setTransactionId(transMsg->getTransactionId());
   aMsg->setIsSuccess(true);
   aMsg->setTimeSent(transMsg->getTimeSent());
   aMsg->setAmount(transMsg->getAmount());
   //no need to set secret;
   vector<int> route=ttmsg->getRoute();
   reverse(route.begin(), route.end());
   msg->setRoute(route);
   msg->setHopCount(0);
   msg->setMessageType(ACK_MSG); //now an ack message
   ttmsg->decapsulate(); // remove encapsulated packet
   delete transMsg;
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

   //TODO: need to get encapsulated
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
// virtual routerMsg *generateUpdateMessage(int transId, int receiver, double amount);
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


/* handleTransactionMessage - checks if message has arrived
 *      1. has arrived - turn transactionMsg into ackMsg, forward ackMsg
 *      2. has not arrived - add to appropriate job queue q, process q as
 *          much as we have funds for
 */
void routerNode::handleTransactionMessage(routerMsg* ttmsg){
   int hopcount = ttmsg->getHopCount();
   vector<tuple<int, double , routerMsg *>> *q;
   transactionMsg *transMsg = check_and_cast<transactionMsg *>(ttmsg->getEncapsulatedPacket());

   if ((ttmsg->getHopCount())>0){ //not a self-message, add to incoming_trans_units
      int prevNode = ttmsg->getRoute()[ttmsg->getHopCount()-1];
      map<int, double> *incomingTransUnits = &(nodeToPaymentChannel[prevNode].incomingTransUnits);
      (*incomingTransUnits)[transMsg->getTransactionId()] = transMsg->getAmount();

   }
   else{
       //is a self-message
      int destNode = ttmsg->getRoute()[ttmsg->getRoute().size()-1];
      statNumAttempted[destNode] = statNumAttempted[destNode]+1;

   }

   if(ttmsg->getHopCount() ==  ttmsg->getRoute().size()-1){ // are at last index of route

      routerMsg* newMsg =  generateAckMessage(ttmsg);
      //forward ack message - no need to wait;
      forwardAckMessage(newMsg);
   }
   else{

      int nextNode = ttmsg->getRoute()[hopcount+1];

      q = &(nodeToPaymentChannel[nextNode].queuedTransUnits);

      //TODO: how to push to heap?
      (*q).push_back(make_tuple(transMsg->getPriorityClass(), transMsg->getAmount(),
               ttmsg));
      push_heap((*q).begin(), (*q).end(), sortPriorityThenAmtFunction);

      processTransUnits(nextNode, *q);

   }
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
   while((int)q.size()>0 && get<1>(q.back())<=nodeToPaymentChannel[dest].balance){

      forwardTransactionMessage(get<2>(q.back()));
      q.pop_back();

   }
}


/*
 *  forwardTransactionMessage - given a message representing a transUnit, increments hopCount, finds next destination,
 *      adjusts (decrements) channel balance, sends message to next node on route
 */
void routerNode::forwardTransactionMessage(routerMsg *msg)
{
   // Increment hop count.
   msg->setHopCount(msg->getHopCount()+1);
   //use hopCount to find next destination
   int nextDest = msg->getRoute()[msg->getHopCount()];
   transactionMsg *transMsg = check_and_cast<transactionMsg *>(msg->getEncapsulatedPacket());
   //add amount to outgoing map

   map<int, double> *outgoingTransUnits = &(nodeToPaymentChannel[nextDest].outgoingTransUnits);
   (*outgoingTransUnits)[transMsg->getTransactionId()] = transMsg->getAmount();

   //numSentPerChannel incremented every time (key,value) pair added to outgoing_trans_units map
   nodeToPaymentChannel[nextDest].statNumSent =  nodeToPaymentChannel[nextDest].statNumSent+1;

   int amt = transMsg->getAmount();
   nodeToPaymentChannel[nextDest].balance = nodeToPaymentChannel[nextDest].balance - amt;

   int transId = transMsg->getTransactionId();

   send(msg, nodeToPaymentChannel[nextDest].gate);

}
