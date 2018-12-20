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


void routerNode::handleProbeMessage(routerMsg* ttmsg){
    probeMsg *pMsg = check_and_cast<probeMsg *>(ttmsg->getEncapsulatedPacket());
    if (simTime()> _simulationLength){
       ttmsg->decapsulate();
       delete pMsg;
       delete ttmsg;
       return;
    }
    bool isReversed = pMsg->getIsReversed();
    int nextDest = ttmsg->getRoute()[ttmsg->getHopCount()+1];

    forwardProbeMessage(ttmsg);
}


void routerNode::initialize()
{
   string topologyFile_ = par("topologyFile");
   string workloadFile_ = par("workloadFile");


   //TODO: may be redudant if hostNode is initialized first
   if (getIndex() == 0){  //main initialization for global parameters
       //TODO: initialize numRouterNodes
       //TODO: initiialize numHostNodes
       useWaterfilling = true;

      if (useWaterfilling){
          kValue = 3;
      }


      setNumNodes(topologyFile_); //TODO: condense into generate_trans_unit_list
      // add all the TransUnits into global list
      generateTransUnitList(workloadFile_, transUnitList);

      //create "channels" map - from topology file - let hostNodes be 0-k, routerNodes be k+1-n
          //to calculate index of routerNode
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
         nodeToPaymentChannel[nextGate->getOwnerModule()->getIndex()] = temp; //TODO: change getIndex() to myIndex()
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

}


void routerNode::forwardTimeOutMessage(routerMsg* msg){
   // Increment hop count.
   msg->setHopCount(msg->getHopCount()+1);
   //use hopCount to find next destination
   int nextDest = msg->getRoute()[msg->getHopCount()];
   //ackMsg *aMsg = check_and_cast<ackMsg *>(msg->getEncapsulatedPacket());

   send(msg, nodeToPaymentChannel[nextDest].gate);

}

void routerNode::handleTimeOutMessage(routerMsg* ttmsg){ //TODO: need to fix handleTimeOutMessage

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



void routerNode::handleAckMessage(routerMsg* ttmsg){ //TODO: need to fix handleAckMessage
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


void routerNode::forwardAckMessage(routerMsg *msg){
   // Increment hop count.
   msg->setHopCount(msg->getHopCount()+1);
   //use hopCount to find next destination
   int nextDest = msg->getRoute()[msg->getHopCount()];
   //ackMsg *aMsg = check_and_cast<ackMsg *>(msg->getEncapsulatedPacket());

   send(msg, nodeToPaymentChannel[nextDest].gate);
}


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

   int destination = transMsg->getReceiver();

   //check if message is already failed: (1) past time()>timeSent+timeOut (2) on failedTransUnit list
   if ( transMsg->getHasTimeOut() && (simTime() > (transMsg->getTimeSent() + transMsg->getTimeOut()))  ){
       //TODO: change to if transaction is in canceledTransactions set

      //delete yourself
      ttmsg->decapsulate();
      delete transMsg;
      delete ttmsg;
   }
   else{
        //not a self-message, add to incoming_trans_units
         int prevNode = ttmsg->getRoute()[ttmsg->getHopCount()-1];
         map<int, double> *incomingTransUnits = &(nodeToPaymentChannel[prevNode].incomingTransUnits);
         (*incomingTransUnits)[transMsg->getTransactionId()] = transMsg->getAmount();

         int nextNode = ttmsg->getRoute()[hopcount+1];
         q = &(nodeToPaymentChannel[nextNode].queuedTransUnits);

         (*q).push_back(make_tuple(transMsg->getPriorityClass(), transMsg->getAmount(),
                  ttmsg));
         push_heap((*q).begin(), (*q).end(), sortPriorityThenAmtFunction);

         processTransUnits(nextNode, *q);

   }
}



/*
 * processTransUnits - given an adjacent node, and TransUnit queue of things to send to that node, sends
 *  TransUnits until channel funds are too low by calling forwardMessage on message representing TransUnit
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
 *  forwardTransactionMessage - given a message representing a TransUnit, increments hopCount, finds next destination,
 *      adjusts (decrements) channel balance, sends message to next node on route
 */
bool routerNode::forwardTransactionMessage(routerMsg *msg)
{
   transactionMsg *transMsg = check_and_cast<transactionMsg *>(msg->getEncapsulatedPacket());

   //cout <<"forwardTransactionMesg: " << transMsg->getHasTimeOut() << endl;

   if (transMsg->getHasTimeOut() && (simTime() > (transMsg->getTimeSent() + transMsg->getTimeOut()))){
       //TODO: check if transaction is in set canceledTransactions
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

