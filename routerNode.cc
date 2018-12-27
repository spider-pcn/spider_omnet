#include "routerNode.h"
#include "hostInitialize.h"
#include <queue>

Define_Module(routerNode);

int routerNode::myIndex(){
   return getIndex() + _numHostNodes;
}

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

   if (myIndex() == 0){  //main initialization for global parameters
      //TODO: initialize numRouterNodes
      //TODO: initiialize numHostNodes
      _useWaterfilling = true;

      if (_useWaterfilling){
         _kValue = 3;
      }

      setNumNodes(topologyFile_); //TODO: condense into generate_trans_unit_list
      // add all the TransUnits into global list
      generateTransUnitList(workloadFile_, _transUnitList);

      //create "channels" map - from topology file - let hostNodes be 0-k, routerNodes be k+1-n
      //to calculate index of routerNode
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
         nodeToPaymentChannel[key] = temp; //TODO: change myIndex() to myIndex()
      }
   } while (i < gateSize);

   //initialize everything for adjacent nodes/nodes with payment channel to me
   for(auto iter = nodeToPaymentChannel.begin(); iter != nodeToPaymentChannel.end(); ++iter)
   {
      int key =  iter->first; //node

      //fill in balance field of nodeToPaymentChannel
      nodeToPaymentChannel[key].balance = _balances[make_tuple(myIndex(),key)];

      //initialize queuedTransUnits
      vector<tuple<int, double , routerMsg *, Id>> temp;
      make_heap(temp.begin(), temp.end(), sortPriorityThenAmtFunction);
      nodeToPaymentChannel[key].queuedTransUnits = temp;

      //register signals
      char signalName[64];
      simsignal_t signal;
      cProperty *statisticTemplate;

      //numInQueuePerChannel signal
      if (key<_numHostNodes){
              sprintf(signalName, "numInQueuePerChannel(host %d)", key);
          }
          else{
              sprintf(signalName, "numInQueuePerChannel(router %d [%d] )", key - _numHostNodes, key);
          }
      signal = registerSignal(signalName);
      statisticTemplate = getProperties()->get("statisticTemplate", "numInQueuePerChannelTemplate");
      getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
      nodeToPaymentChannel[key].numInQueuePerChannelSignal = signal;

      //numProcessedPerChannel signal
      if (key<_numHostNodes){
           sprintf(signalName, "numProcessedPerChannel(host %d)", key);
           }
           else{
             sprintf(signalName, "numProcessedPerChannel(router %d [%d])", key-_numHostNodes, key);
           }
      signal = registerSignal(signalName);
      statisticTemplate = getProperties()->get("statisticTemplate", "numProcessedPerChannelTemplate");
      getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
      nodeToPaymentChannel[key].numProcessedPerChannelSignal = signal;

      //statNumProcessed int
      nodeToPaymentChannel[key].statNumProcessed = 0;

      //balancePerChannel signal
      if (key<_numHostNodes){
                  sprintf(signalName, "balancePerChannel(host %d)", key);
                  }
                  else{
                    sprintf(signalName, "balancePerChannel(router %d [%d])", key-_numHostNodes, key);
                  }

      signal = registerSignal(signalName);
      statisticTemplate = getProperties()->get("statisticTemplate", "balancePerChannelTemplate");
      getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
      nodeToPaymentChannel[key].balancePerChannelSignal = signal;

      //numSentPerChannel signal
      if (key<_numHostNodes){
                  sprintf(signalName, "numSentPerChannel(host %d)", key);
                  }
                  else{
                    sprintf(signalName, "numSentPerChannel(router %d [%d])", key-_numHostNodes, key);
                  }
      signal = registerSignal(signalName);
      statisticTemplate = getProperties()->get("statisticTemplate", "numSentPerChannelTemplate");
      getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
      nodeToPaymentChannel[key].numSentPerChannelSignal = signal;

      //statNumSent int
      nodeToPaymentChannel[key].statNumSent = 0;
   }

   routerMsg *statMsg = generateStatMessage();
   scheduleAt(simTime() + 0, statMsg);

   routerMsg *clearStateMsg = generateClearStateMessage();
   scheduleAt(simTime() + _clearRate, clearStateMsg);


}

void routerNode::handleMessage(cMessage *msg)
{
   //cout << "Router Time: " << simTime() << endl;
   //cout << "msg: " << msg->getName() << endl;
   routerMsg *ttmsg = check_and_cast<routerMsg *>(msg);
   if (ttmsg->getMessageType()==ACK_MSG){ //preprocessor constants defined in routerMsg_m.h
      //cout << "[ROUTER "<< myIndex() <<": RECEIVED ACK MSG]"<< msg->getName() << endl;
      //print_message(ttmsg);
      //print_private_values();
      handleAckMessage(ttmsg);
      //cout << "[AFTER HANDLING:]" <<endl;
      // print_private_values();
   }
   else if(ttmsg->getMessageType()==TRANSACTION_MSG){
      //cout<< "[ROUTER "<< myIndex() <<": RECEIVED TRANSACTION MSG] "<< msg->getName()<<endl;
      // print_message(ttmsg);
      // print_private_values();
      handleTransactionMessage(ttmsg);
      //cout<< "[AFTER HANDLING:]"<<endl;
      // print_private_values();
   }
   else if(ttmsg->getMessageType()==UPDATE_MSG){
      //cout<< "[ROUTER "<< myIndex() <<": RECEIVED UPDATE MSG]"<< msg->getName()<<endl;
      // print_message(ttmsg);
      // print_private_values();
      handleUpdateMessage(ttmsg);
      //cout<< "[AFTER HANDLING:]  "<<endl;
      //print_private_values();
   }
   else if (ttmsg->getMessageType() ==STAT_MSG){
      //cout<< "[ROUTER "<< myIndex() <<": RECEIVED STAT MSG]"<< msg->getName()<<endl;
      handleStatMessage(ttmsg);
      //cout<< "[AFTER HANDLING:]  "<<endl;
   }
   else if (ttmsg->getMessageType() == TIME_OUT_MSG){
      //cout<< "[ROUTER "<< myIndex() <<": RECEIVED TIME_OUT_MSG] "<< msg->getName()<<endl;
      handleTimeOutMessage(ttmsg);
      //cout<< "[AFTER HANDLING:]  "<<endl;
   }
   else if (ttmsg->getMessageType() == PROBE_MSG){
      //cout<< "[ROUTER "<< myIndex() <<": RECEIVED PROBE_MSG] "<< msg->getName()<<endl;
      handleProbeMessage(ttmsg);
      //cout<< "[AFTER HANDLING:]  "<<endl;
   }
   else if (ttmsg->getMessageType() == CLEAR_STATE_MSG){
      //cout<< "[ROUTER "<< myIndex() <<": RECEIVED CLEAR_STATE_MSG] "<< msg->getName()<<endl;
      handleClearStateMessage(ttmsg);
      //cout<< "[AFTER HANDLING:]  "<<endl;
   }



}


routerMsg *routerNode::generateStatMessage(){
   char msgname[MSGSIZE];
   sprintf(msgname, "node-%d statMsg", myIndex());
   routerMsg *rMsg = new routerMsg(msgname);
   rMsg->setMessageType(STAT_MSG);
   return rMsg;
}
bool manualFindQueuedTransUnitsByTransactionId( vector<tuple<int, double, routerMsg*, Id>> (queuedTransUnits), int transactionId){
    for (auto q: queuedTransUnits){
        int qId = get<0>(get<3>(q));
        if (qId == transactionId){
            return true;
        }
    }
    return false;
}



void routerNode::handleClearStateMessage(routerMsg* ttmsg){


   if (simTime() > _simulationLength){
      delete ttmsg;

   }
   else{

      scheduleAt(simTime()+_clearRate, ttmsg);
   }


   for ( auto it = canceledTransactions.begin(); it!= canceledTransactions.end();){ //iterate through all canceledTransactions
      int transactionId = get<0>(*it);
      simtime_t msgArrivalTime = get<1>(*it);
      int prevNode = get<2>(*it);
      int nextNode = get<3>(*it);

      if (simTime() > (msgArrivalTime + _maxTravelTime)){ // we can process



          //fixed not deleting from the queue
          // start queue searching
          vector<tuple<int, double, routerMsg*, Id>>* queuedTransUnits = &(nodeToPaymentChannel[nextNode].queuedTransUnits);
          //sort_heap((*queuedTransUnits).begin(), (*queuedTransUnits).end());

          auto iterQueue = find_if((*queuedTransUnits).begin(),
                (*queuedTransUnits).end(),
                [&transactionId](const tuple<int, double, routerMsg*, Id>& p)
                { return (get<0>(get<3>(p)) == transactionId); });
          if (iterQueue != (*queuedTransUnits).end()){
              cout << "queuedTransUnits size before: " << (*queuedTransUnits).size();
             iterQueue =   (*queuedTransUnits).erase(iterQueue);

             cout << "maxTravelTime: " << _maxTravelTime << endl;
             cout << "cleared message, size after: " << (*queuedTransUnits).size() << endl;

          }
          else{

              if (manualFindQueuedTransUnitsByTransactionId((*queuedTransUnits), transactionId)){
              cout << "could not find " << transactionId << "in " << endl;
              checkQueuedTransUnits((*queuedTransUnits), nextNode);
              }



          }








          //we can process it
         map<tuple<int,int>, double> *incomingTransUnits = &(nodeToPaymentChannel[prevNode].incomingTransUnits);
         auto iterIncoming = find_if((*incomingTransUnits).begin(),
               (*incomingTransUnits).end(),
               [&transactionId](const pair<tuple<int, int >, double> & p)
               { return get<0>(p.first) == transactionId; });

         while (iterIncoming != (*incomingTransUnits).end()){

                iterIncoming = (*incomingTransUnits).erase(iterIncoming);

               // start queue searching
               vector<tuple<int, double, routerMsg*, Id>>* queuedTransUnits = &(nodeToPaymentChannel[nextNode].queuedTransUnits);
               //sort_heap((*queuedTransUnits).begin(), (*queuedTransUnits).end());

               auto iterQueue = find_if((*queuedTransUnits).begin(),
                     (*queuedTransUnits).end(),
                     [&transactionId](const tuple<int, double, routerMsg*, Id>& p)
                     { return (get<0>(get<3>(p)) == transactionId); });
               if (iterQueue != (*queuedTransUnits).end()){
                   cout << "queuedTransUnits size before: " << (*queuedTransUnits).size();
                  iterQueue =   (*queuedTransUnits).erase(iterQueue);

                  cout << "maxTravelTime: " << _maxTravelTime << endl;
                  cout << "cleared message, size after: " << (*queuedTransUnits).size() << endl;

               }
               else{

                   if (manualFindQueuedTransUnitsByTransactionId((*queuedTransUnits), transactionId)){
                   cout << "could not find " << transactionId << "in " << endl;
                   checkQueuedTransUnits((*queuedTransUnits), nextNode);
                   }



               }

               make_heap((*queuedTransUnits).begin(), (*queuedTransUnits).end(), sortPriorityThenAmtFunction);

            //find next in incoming
            auto iterIncoming = find_if((*incomingTransUnits).begin(),
                  (*incomingTransUnits).end(),
                  [&transactionId](const pair<tuple<int, int >, double> & p)
                  { return get<0>(p.first) == transactionId; });
         }

         map<tuple<int,int>, double> *outgoingTransUnits = &(nodeToPaymentChannel[nextNode].outgoingTransUnits);

         auto iterOutgoing = find_if((*outgoingTransUnits).begin(),
               (*outgoingTransUnits).end(),
               [&transactionId](const pair<tuple<int, int >, double> & p)
               { return get<0>(p.first) == transactionId; });

         while (iterOutgoing != (*outgoingTransUnits).end()){

            if (iterOutgoing != (*outgoingTransUnits).end()){
               double amount = iterOutgoing -> second;
               nodeToPaymentChannel[nextNode].balance = nodeToPaymentChannel[nextNode].balance + amount;
               iterOutgoing = (*outgoingTransUnits).erase(iterOutgoing);

            }

            auto iterOutgoing = find_if((*outgoingTransUnits).begin(),
                  (*outgoingTransUnits).end(),
                  [&transactionId](const pair<tuple<int, int >, double> & p)
                  { return get<0>(p.first) == transactionId; });
         }

         //it = canceledTransactions.erase(it);
         it++;
      }
      else{
          it++;
      }


   }

}

routerMsg *routerNode::generateClearStateMessage(){
   char msgname[MSGSIZE];
   sprintf(msgname, "node-%d clearStateMsg", myIndex());
   routerMsg *rMsg = new routerMsg(msgname);
   rMsg->setMessageType(CLEAR_STATE_MSG);
   return rMsg;
}



void routerNode::checkQueuedTransUnits(vector<tuple<int, double, routerMsg*,  Id >> queuedTransUnits, int nextNode){

    if (queuedTransUnits.size()>0){

    cout << "simTime(): " << simTime() << endl;
    cout << "queuedTransUnits size: " << queuedTransUnits.size() << endl;
    for (auto q: queuedTransUnits){
        routerMsg* msg = get<2>(q);
        transactionMsg *transMsg = check_and_cast<transactionMsg *>(msg->getEncapsulatedPacket());

        if (transMsg->getHasTimeOut()){
            cout << "(" << (transMsg->getTimeSent() + transMsg->getTimeOut()) <<","<< transMsg->getTransactionId() << ","<<nextNode << ") ";
        }
        else{
            cout << "(-1) ";
        }

    }
    cout << endl;
    for (auto c: canceledTransactions){
        cout << "(" << get<0>(c) << "," <<get<1>(c) <<","<< get<2>(c)<< "," << get<3>(c)<< ") ";

    }
    cout << endl;
    }
}

void routerNode::handleStatMessage(routerMsg* ttmsg){
   if (simTime() > _simulationLength){
      delete ttmsg;
      deleteMessagesInQueues();
   }
   else{
      scheduleAt(simTime()+_statRate, ttmsg);
   }

   for ( auto it = nodeToPaymentChannel.begin(); it!= nodeToPaymentChannel.end(); it++){ //iterate through all adjacent nodes

      int node = it->first; //key

      //checkQueuedTransUnits(nodeToPaymentChannel[node].queuedTransUnits, node);


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

void routerNode::handleTimeOutMessage(routerMsg* ttmsg){

   timeOutMsg *toutMsg = check_and_cast<timeOutMsg *>(ttmsg->getEncapsulatedPacket());
   int nextNode = (ttmsg->getRoute())[ttmsg->getHopCount()+1];
   int prevNode = (ttmsg->getRoute())[ttmsg->getHopCount()-1];


   CanceledTrans ct = make_tuple(toutMsg->getTransactionId(),simTime(),prevNode, nextNode);
   canceledTransactions.insert(ct);
   forwardTimeOutMessage(ttmsg);

}





void routerNode::handleAckMessage(routerMsg* ttmsg){
   //generate updateMsg
   int prevNode = ttmsg->getRoute()[ttmsg->getHopCount()-1];

   //remove transaction from outgoing_trans_unit
   map<Id, double> *outgoingTransUnits = &(nodeToPaymentChannel[prevNode].outgoingTransUnits);
   ackMsg *aMsg = check_and_cast<ackMsg *>(ttmsg->getEncapsulatedPacket());

   (*outgoingTransUnits).erase(make_tuple(aMsg->getTransactionId(), aMsg->getHtlcIndex()));

   //increment signal numProcessed
   nodeToPaymentChannel[prevNode].statNumProcessed = nodeToPaymentChannel[prevNode].statNumProcessed+1;

   if (aMsg->getIsSuccess() == false){
      //should not occur
   }
   else{ //successful transaction

      //check if transactionId is in canceledTransaction set
      int transactionId = aMsg->getTransactionId();

      //TODO: not sure if this works properly
      auto iter = find_if(canceledTransactions.begin(),
            canceledTransactions.end(),
            [&transactionId](const tuple<int, simtime_t, int, int>& p)
            { return get<0>(p) == transactionId; });

      if (iter!=canceledTransactions.end()){
         canceledTransactions.erase(iter);
      }


      routerMsg* uMsg =  generateUpdateMessage(aMsg->getTransactionId(), prevNode, aMsg->getAmount() );
      sendUpdateMessage(uMsg);


      forwardAckMessage(ttmsg);

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
   vector<tuple<int, double , routerMsg *, Id>> *q;
   int prevNode = msg->getRoute()[msg->getHopCount()-1];

   updateMsg *uMsg = check_and_cast<updateMsg *>(msg->getEncapsulatedPacket());
   //increment the in flight funds back
   nodeToPaymentChannel[prevNode].balance =  nodeToPaymentChannel[prevNode].balance + uMsg->getAmount();

   //remove transaction from incoming_trans_units
   map<Id, double> *incomingTransUnits = &(nodeToPaymentChannel[prevNode].incomingTransUnits);

   (*incomingTransUnits).erase(make_tuple(uMsg->getTransactionId(),uMsg->getHtlcIndex()));

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
   vector<tuple<int, double , routerMsg *, Id>> *q;
   transactionMsg *transMsg = check_and_cast<transactionMsg *>(ttmsg->getEncapsulatedPacket());

   int destination = transMsg->getReceiver();


   //check if message is already failed: (1) past time()>timeSent+timeOut (2) on failedTransUnit list
   if (transMsg->getHasTimeOut()){
       int transactionId = transMsg->getTransactionId();
   auto iter = find_if(canceledTransactions.begin(),
           canceledTransactions.end(),
           [&transactionId](const tuple<int, simtime_t, int, int>& p)
           { return get<0>(p) == transactionId; });

     if ( iter!=canceledTransactions.end() ){
        //TODO: check to see if transactionId is in canceledTransactions set

        //delete yourself
        ttmsg->decapsulate();
        delete transMsg;
        delete ttmsg;
        return;
     }

   }


      //not a self-message, add to incoming_trans_units
      int prevNode = ttmsg->getRoute()[ttmsg->getHopCount()-1];
      map<Id, double> *incomingTransUnits = &(nodeToPaymentChannel[prevNode].incomingTransUnits);
      (*incomingTransUnits)[make_tuple(transMsg->getTransactionId(), transMsg->getHtlcIndex())] = transMsg->getAmount();

      int nextNode = ttmsg->getRoute()[hopcount+1];
      q = &(nodeToPaymentChannel[nextNode].queuedTransUnits);

      (*q).push_back(make_tuple(transMsg->getPriorityClass(), transMsg->getAmount(),
               ttmsg, make_tuple(transMsg->getTransactionId(), transMsg->getHtlcIndex())));
      push_heap((*q).begin(), (*q).end(), sortPriorityThenAmtFunction);

      processTransUnits(nextNode, *q);


}



/*
 * processTransUnits - given an adjacent node, and TransUnit queue of things to send to that node, sends
 *  TransUnits until channel funds are too low by calling forwardMessage on message representing TransUnit
 */
void routerNode:: processTransUnits(int dest, vector<tuple<int, double , routerMsg *, Id>>& q){
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

   int nextDest = msg->getRoute()[msg->getHopCount()+1];

   if (transMsg->getAmount() >nodeToPaymentChannel[nextDest].balance){
      return false;
   }
   else{




      // Increment hop count.
      msg->setHopCount(msg->getHopCount()+1);
      //use hopCount to find next destination

      //add amount to outgoing map

      map<Id, double> *outgoingTransUnits = &(nodeToPaymentChannel[nextDest].outgoingTransUnits);
      (*outgoingTransUnits)[make_tuple(transMsg->getTransactionId(), transMsg->getHtlcIndex())] = transMsg->getAmount();

      //numSentPerChannel incremented every time (key,value) pair added to outgoing_trans_units map
      nodeToPaymentChannel[nextDest].statNumSent =  nodeToPaymentChannel[nextDest].statNumSent+1;

      int amt = transMsg->getAmount();
      nodeToPaymentChannel[nextDest].balance = nodeToPaymentChannel[nextDest].balance - amt;

      //int transId = transMsg->getTransactionId();

      send(msg, nodeToPaymentChannel[nextDest].gate);
      return true;
   } //end else (transMsg->getAmount() >nodeToPaymentChannel[dest].balance)


}

