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
            temp!= (nodeToPaymentChannel[key].queuedTransUnits).end(); ){
         routerMsg * rMsg = get<2>(*temp);
         auto tMsg = rMsg->getEncapsulatedPacket();
         rMsg->decapsulate();
         delete tMsg;
         delete rMsg;
         temp = (nodeToPaymentChannel[key].queuedTransUnits).erase(temp);
      }
   }

   //check queue sizes after deletion:
   /*
   cout << "myIndex: " << myIndex();
   for (auto iter = nodeToPaymentChannel.begin(); iter!=nodeToPaymentChannel.end(); iter++){
        int key = iter->first;
        cout << " (" << key << ": size " << nodeToPaymentChannel[key].queuedTransUnits.size() << ")" ;
     }
   cout << endl;
   */
}

void routerNode:: printNodeToPaymentChannel(){

   printf("print of channels\n" );
   for (auto i : nodeToPaymentChannel){
      printf("(key: %d, %f )",i.first, i.second.balance);
      /*
         for (auto k: i.second){
         printf("(%d, %d) ",get<0>(k), get<1>(k));
         }

         printf("] \n");
       */
   }
   cout<<endl;

}

void routerNode::updateBalance(int destNode, double amtToAdd){
    double totalCapacity = nodeToPaymentChannel[destNode].totalCapacity;
    double oldBalance = nodeToPaymentChannel[destNode].balance;
    double newBalance = nodeToPaymentChannel[destNode].balance + amtToAdd;
    assert(newBalance >= 0 && newBalance <= totalCapacity);

    nodeToPaymentChannel[destNode].balance = newBalance;
    if (!_rebalanceEnabled)
        return;

    if (oldBalance > 0 && newBalance == 0 ) //update zeroStartTime, oldBal>0 catches amtToAdd = 0 case
        nodeToPaymentChannel[destNode].zeroStartTime = simTime();
    else if (oldBalance == 0 && newBalance > 0)
        nodeToPaymentChannel[destNode].zeroStartTime = -1;
}


void routerNode::handleRebalanceMessage(routerMsg* ttmsg){
    // reschedule this message to be sent again
    if (simTime() > _simulationLength){
        delete ttmsg;
    }
    else {
        scheduleAt(simTime()+_rebalanceRate, ttmsg);
    }

    for(auto iter = nodeToPaymentChannel.begin(); iter != nodeToPaymentChannel.end(); ++iter)
    {
        int key =iter->first; //node

        double oldBalance = nodeToPaymentChannel[key].balance;
        simtime_t zeroStartTime = nodeToPaymentChannel[key].zeroStartTime;

        if ( ( oldBalance == 0 ) && ( zeroStartTime >= 0)  && ( zeroStartTime + _rebalanceTimeReq <= simTime()) )
        {
            double addedAmt= oldBalance * _rebalanceFrac;
            //rebalance channel
            nodeToPaymentChannel[key].balance += addedAmt;
            //adjust total capacity
            nodeToPaymentChannel[key].totalCapacity += addedAmt;
            nodeToPaymentChannel[key].balanceAdded += addedAmt;

        }
    }
}


void routerNode::forwardProbeMessage(routerMsg *msg){
   int prevDest = msg->getRoute()[msg->getHopCount() - 1];
   bool updateOnReverse = true;

   // Increment hop count.
   msg->setHopCount(msg->getHopCount()+1);
   //use hopCount to find next destination
   int nextDest = msg->getRoute()[msg->getHopCount()];

   probeMsg *pMsg = check_and_cast<probeMsg *>(msg->getEncapsulatedPacket());

   if (pMsg->getIsReversed() == true && updateOnReverse == true){
      vector<double> *pathBalances = & ( pMsg->getPathBalances());
      (*pathBalances).push_back(nodeToPaymentChannel[prevDest].balanceEWMA);
   } 
   else if (pMsg->getIsReversed() == false && updateOnReverse == false) {
      vector<double> *pathBalances = & ( pMsg->getPathBalances());
      (*pathBalances).push_back(nodeToPaymentChannel[nextDest].balanceEWMA);
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
   // no need for if (myIndex==0), as will always be initialized in a host node
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
      nodeToPaymentChannel[key].balanceEWMA = nodeToPaymentChannel[key].balance;


      // initialize total capacity and other price variables
      double balanceOpp =  _balances[make_tuple(key, myIndex())];
      nodeToPaymentChannel[key].totalCapacity = nodeToPaymentChannel[key].balance + balanceOpp;
      nodeToPaymentChannel[key].lambda = 0;
      nodeToPaymentChannel[key].muLocal = 0;
      nodeToPaymentChannel[key].muRemote = 0;

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
      nodeToPaymentChannel[key].amtInQueuePerChannelSignal = signal;

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

      //InflightPerChannel signal
      if (key<_numHostNodes){
         sprintf(signalName, "numInflightPerChannel(host %d)", key);
      }
      else{
         sprintf(signalName, "numInflightPerChannel(router %d [%d])", key-_numHostNodes, key);
      }

      signal = registerSignal(signalName);
      statisticTemplate = getProperties()->get("statisticTemplate", "numInflightPerChannelTemplate");
      getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
      nodeToPaymentChannel[key].numInflightPerChannelSignal = signal;


      if (_priceSchemeEnabled){

         if (key<_numHostNodes) {
            sprintf(signalName, "nValuePerChannel(host %d)", key);
         }
         else {
            sprintf(signalName, "nValuePerChannel(router %d [%d])", key - _numHostNodes, key);
         }
         signal = registerSignal(signalName);
         statisticTemplate = getProperties()->get("statisticTemplate", "nValuePerChannelTemplate");
         getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
         nodeToPaymentChannel[key].nValueSignal = signal;

         if (key<_numHostNodes) {
            sprintf(signalName, "inFlightSumPerChannel(host %d)", key);
         }
         else {
            sprintf(signalName, "inFlightSumPerChannel(router %d [%d])", key - _numHostNodes, key);
         }
         signal = registerSignal(signalName);
         statisticTemplate = getProperties()->get("statisticTemplate", "inFlightSumPerChannelTemplate");
         getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
         nodeToPaymentChannel[key].inFlightSumSignal = signal;

         if (key<_numHostNodes) {
            sprintf(signalName, "balSumPerChannel(host %d)", key);
         }
         else {
            sprintf(signalName, "balSumPerChannel(router %d [%d])", key - _numHostNodes, key);
         }
         signal = registerSignal(signalName);
         statisticTemplate = getProperties()->get("statisticTemplate", "balSumPerChannelTemplate");
         getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
         nodeToPaymentChannel[key].balSumSignal = signal;

         if (key<_numHostNodes) {
            sprintf(signalName, "lambdaPerChannel(host %d)", key);
         }
         else {
            sprintf(signalName, "lambdaPerChannel(router %d [%d])", key - _numHostNodes, key);
         }
         signal = registerSignal(signalName);
         statisticTemplate = getProperties()->get("statisticTemplate", "lambdaPerChannelTemplate");
         getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
         nodeToPaymentChannel[key].lambdaSignal = signal;


         if (key<_numHostNodes) {
            sprintf(signalName, "muLocalPerChannel(host %d)", key);
         }
         else {
            sprintf(signalName, "muLocalPerChannel(router %d [%d])", key - _numHostNodes, key);
         }
         signal = registerSignal(signalName);
         statisticTemplate = getProperties()->get("statisticTemplate", "muLocalPerChannelTemplate");
         getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
         nodeToPaymentChannel[key].muLocalSignal = signal;
      }

   }

   routerMsg *statMsg = generateStatMessage();
   scheduleAt(simTime() + 0, statMsg);

   if (_rebalanceEnabled){
        routerMsg *rebalanceMsg = generateRebalanceMessage();
        scheduleAt(simTime() + 0, rebalanceMsg);
   }

   routerMsg *clearStateMsg = generateClearStateMessage();
   scheduleAt(simTime() + _clearRate, clearStateMsg);

   if (_priceSchemeEnabled){
      routerMsg *triggerPriceUpdateMsg = generateTriggerPriceUpdateMessage();
      scheduleAt(simTime() + _tUpdate, triggerPriceUpdateMsg );
   }

}

void routerNode::handleMessage(cMessage *msg)
{
    routerMsg *ttmsg = check_and_cast<routerMsg *>(msg);
    if (simTime() > _simulationLength){
        auto encapMsg = (ttmsg->getEncapsulatedPacket());
           ttmsg->decapsulate();
           delete ttmsg;
           delete encapMsg;
           return;
    }


   if (ttmsg->getMessageType()==ACK_MSG){ //preprocessor constants defined in routerMsg_m.h
      if (_loggingEnabled) cout << "[ROUTER "<< myIndex() <<": RECEIVED ACK MSG]"<< msg->getName() << endl;
      if (_timeoutEnabled){
         handleAckMessageTimeOut(ttmsg);
      }
      handleAckMessage(ttmsg);
      if (_loggingEnabled) cout << "[AFTER HANDLING:]" <<endl;
   }
   else if(ttmsg->getMessageType()==TRANSACTION_MSG){
      if (_loggingEnabled) cout<< "[ROUTER "<< myIndex() <<": RECEIVED TRANSACTION MSG] "<< msg->getName()<<endl;
      if (_timeoutEnabled && handleTransactionMessageTimeOut(ttmsg)){ //handleTransactionMessageTimeOut(ttmsg) returns true if msg deleted
         return;
      }
      if (_priceSchemeEnabled){
         handleTransactionMessagePriceScheme(ttmsg); //increment N value
      }

      handleTransactionMessage(ttmsg);
      if (_loggingEnabled) cout<< "[AFTER HANDLING:]"<<endl;
   }
   else if(ttmsg->getMessageType()==UPDATE_MSG){
      if (_loggingEnabled) cout<< "[ROUTER "<< myIndex() <<": RECEIVED UPDATE MSG]"<< msg->getName()<<endl;
      handleUpdateMessage(ttmsg);
      if (_loggingEnabled) cout<< "[AFTER HANDLING:]  "<<endl;
   }
   else if (ttmsg->getMessageType() ==STAT_MSG){
      if (_loggingEnabled) cout<< "[ROUTER "<< myIndex() <<": RECEIVED STAT MSG]"<< msg->getName()<<endl;
      if (_priceSchemeEnabled){
         handleStatMessagePriceScheme(ttmsg);
      }
      handleStatMessage(ttmsg);
      if (_loggingEnabled) cout<< "[AFTER HANDLING:]  "<<endl;
   }
   else if (ttmsg->getMessageType() == TIME_OUT_MSG){
      if (_loggingEnabled) cout<< "[ROUTER "<< myIndex() <<": RECEIVED TIME_OUT_MSG] "<< msg->getName()<<endl;
      handleTimeOutMessage(ttmsg);
      if (_loggingEnabled) cout<< "[AFTER HANDLING:]  "<<endl;
   }
   else if (ttmsg->getMessageType() == PROBE_MSG){
      if (_loggingEnabled)  cout<< "[ROUTER "<< myIndex() <<": RECEIVED PROBE_MSG] "<< msg->getName()<<endl;
      handleProbeMessage(ttmsg);
      if (_loggingEnabled)  cout<< "[AFTER HANDLING:]  "<<endl;
   }
   else if (ttmsg->getMessageType() == CLEAR_STATE_MSG){
      if (_loggingEnabled)  cout<< "[ROUTER "<< myIndex() <<": RECEIVED CLEAR_STATE_MSG] "<< msg->getName()<<endl;
      handleClearStateMessage(ttmsg);
      if (_loggingEnabled) cout<< "[AFTER HANDLING:]  "<<endl;
   }
   else if (ttmsg->getMessageType() == TRIGGER_PRICE_UPDATE_MSG){
      if (_loggingEnabled)  cout<< "[ROUTER "<< myIndex() <<": RECEIVED TRIGGER_PRICE_UPDATE_MSG] "<< msg->getName()<<endl;
      handleTriggerPriceUpdateMessage(ttmsg); //TODO: need to write
      if (_loggingEnabled) cout<< "[AFTER HANDLING:]  "<<endl;
   }
   else if (ttmsg->getMessageType() == PRICE_UPDATE_MSG){
      if (_loggingEnabled)  cout<< "[ROUTER "<< myIndex() <<": RECEIVED PRICE_UPDATE_MSG] "<< msg->getName()<<endl;
      handlePriceUpdateMessage(ttmsg); //TODO: need to write
      if (_loggingEnabled) cout<< "[AFTER HANDLING:]  "<<endl;
   }
   else if (ttmsg->getMessageType() == PRICE_QUERY_MSG){
      if (_loggingEnabled)  cout<< "[ROUTER "<< myIndex() <<": RECEIVED PRICE_QUERY_MSG] "<< msg->getName()<<endl;
      handlePriceQueryMessage(ttmsg); //TODO: need to write
      if (_loggingEnabled) cout<< "[AFTER HANDLING:]  "<<endl;
   }
   else if (ttmsg->getMessageType() == REBALANCE_MSG){
      if (_loggingEnabled)  cout<< "[ROUTER "<< myIndex() <<": RECEIVED REBALANCE_MSG] "<< msg->getName()<<endl;
      handleRebalanceMessage(ttmsg); //TODO: need to write
      if (_loggingEnabled) cout<< "[AFTER HANDLING:]  "<<endl;
   }


}

void routerNode::finish(){
    deleteMessagesInQueues();

   if (_rebalanceEnabled) {
        char buffer[30];
        sprintf(buffer, "rebalance $ added at node %d", myIndex());
        recordScalar(buffer, rebalanceTotalAmtAtNode());
   }
}

void routerNode::handlePriceQueryMessage(routerMsg* ttmsg){
   priceQueryMsg *pqMsg = check_and_cast<priceQueryMsg *>(ttmsg->getEncapsulatedPacket());

   bool isReversed = pqMsg->getIsReversed();
   if (!isReversed){ //is at hopcount 0
      int nextNode = ttmsg->getRoute()[ttmsg->getHopCount()+1];
      double zOld = pqMsg->getZValue();
      double lambda = nodeToPaymentChannel[nextNode].lambda;
      double muLocal = nodeToPaymentChannel[nextNode].muLocal;
      double muRemote = nodeToPaymentChannel[nextNode].muRemote;
      double zNew = zOld;

      if (ttmsg->getHopCount() < ttmsg->getRoute().size() - 2)
        zNew += (2 * lambda) + muLocal  - muRemote;
      pqMsg->setZValue(zNew);
      forwardMessage(ttmsg);
   }
   else{
      forwardMessage(ttmsg);
   }
}

void routerNode::handlePriceUpdateMessage(routerMsg* ttmsg){
   priceUpdateMsg *puMsg = check_and_cast<priceUpdateMsg *>(ttmsg->getEncapsulatedPacket());
   double nRemote = puMsg->getNLocal();
   double inflightRemote = puMsg->getSumInFlight();
   double balSumRemote = puMsg->getBalSum();
   int sender = ttmsg->getRoute()[0];

   PaymentChannel *neighborChannel = &(nodeToPaymentChannel[sender]);

   //Update $\lambda$, $mu_local$ and $mu_remote$
   double xLocal = neighborChannel->xLocal;
   int nLocal = neighborChannel->lastNValue;
   double inflightLocal = neighborChannel->lastSumInFlight;
   double balSumLocal = neighborChannel->lastBalSum;
   if (balSumLocal == -1)
       balSumLocal = neighborChannel->balance/max(double(neighborChannel->incomingTransUnits.size()), 1.0);

   double cValue = nodeToPaymentChannel[sender].totalCapacity;
   double oldLambda = nodeToPaymentChannel[sender].lambda;
   double oldMuLocal = nodeToPaymentChannel[sender].muLocal;
   double oldMuRemote = nodeToPaymentChannel[sender].muRemote;

    // Nesterov's gradient descent equation
    // and other speeding up mechanisms
    double newLambda = 0.0;
    double newMuLocal = 0.0;
    double newMuRemote = 0.0;
    if (_nesterov) {
        double yLambda = nodeToPaymentChannel[sender].yLambda;
        double yMuLocal = nodeToPaymentChannel[sender].yMuLocal;
        double yMuRemote = nodeToPaymentChannel[sender].yMuRemote;

        double yLambdaNew = oldLambda + _eta*(nLocal + nRemote - inflightLocal -inflightRemote 
                - balSumLocal - balSumRemote);
        newLambda = yLambdaNew + _rhoLambda*(yLambdaNew - yLambda); 
        nodeToPaymentChannel[sender].yLambda = yLambdaNew;

        double yMuLocalNew = oldMuLocal + _kappa*(nLocal - nRemote);
        newMuLocal = yMuLocalNew + _rhoMu*(yMuLocalNew - yMuLocal);
        nodeToPaymentChannel[sender].yMuLocal = yMuLocalNew;

        double yMuRemoteNew = oldMuRemote + _kappa*(nRemote - nLocal);
        newMuRemote = yMuRemoteNew + _rhoMu*(yMuRemoteNew - yMuRemote);
        nodeToPaymentChannel[sender].yMuRemote = yMuRemoteNew;
    } 
    else if (_secondOrderOptimization) {
        double lastLambdaGrad = nodeToPaymentChannel[sender].lastLambdaGrad;
        double newLambdaGrad = nLocal + nRemote - inflightLocal -inflightRemote 
                - balSumLocal - balSumRemote;
        newLambda = oldLambda +  _eta*newLambdaGrad + _rhoLambda*(newLambdaGrad - lastLambdaGrad);
        nodeToPaymentChannel[sender].lastLambdaGrad = newLambdaGrad;

        double lastMuLocalGrad = nodeToPaymentChannel[sender].lastMuLocalGrad;
        double newMuLocalGrad = nLocal - nRemote;
        newMuLocal = oldMuLocal + _kappa*newMuLocalGrad + _rhoMu*(newMuLocalGrad - lastMuLocalGrad);
        newMuRemote = oldMuRemote - _kappa*newMuLocalGrad - _rhoMu*(newMuLocalGrad - lastMuLocalGrad);
        nodeToPaymentChannel[sender].lastMuLocalGrad = newMuLocalGrad;
    } 
    else {
        newLambda = oldLambda +  _eta*(nLocal + nRemote - inflightLocal -inflightRemote 
                - balSumLocal - balSumRemote);
        newMuLocal = oldMuLocal + _kappa*(nLocal - nRemote);
        newMuRemote = oldMuRemote + _kappa*(nRemote - nLocal); 
    }

   nodeToPaymentChannel[sender].lambda = maxDouble(newLambda, 0);
   nodeToPaymentChannel[sender].muLocal = maxDouble(newMuLocal, 0);
   nodeToPaymentChannel[sender].muRemote = maxDouble(newMuRemote, 0);

    //delete both messages
    ttmsg->decapsulate();
    delete puMsg;
    delete ttmsg;
}

void routerNode::handleTriggerPriceUpdateMessage(routerMsg* ttmsg){
   if (simTime() > _simulationLength){
      delete ttmsg;
   }
   else{
      scheduleAt(simTime()+_tUpdate, ttmsg);
   }

   for ( auto it = nodeToPaymentChannel.begin(); it!= nodeToPaymentChannel.end(); it++){ 
       //iterate through all channels
      PaymentChannel *neighborChannel = &(nodeToPaymentChannel[it->first]);
      neighborChannel->xLocal =  neighborChannel->nValue / _tUpdate;
      double balSum = neighborChannel->lastBalSum;
      if (balSum == -1)
          balSum = neighborChannel->balance/max(double(neighborChannel->incomingTransUnits.size()), 1.0);
      
      routerMsg * priceUpdateMsg = generatePriceUpdateMessage(neighborChannel->nValue,
             balSum, neighborChannel->sumInFlight, it->first);
      neighborChannel->lastNValue = neighborChannel->nValue;
      neighborChannel->nValue = 0;

      neighborChannel->lastBalSum = neighborChannel->balSum;
      neighborChannel->balSum = -1;

      neighborChannel->lastSumInFlight = neighborChannel->sumInFlight;
      neighborChannel->sumInFlight = 0;
      sendUpdateMessage(priceUpdateMsg);
   }

}

routerMsg * routerNode::generatePriceUpdateMessage(double nLocal, double balSum, double sumInFlight, int reciever){
   char msgname[MSGSIZE];

   sprintf(msgname, "tic-%d-to-%d priceUpdateMsg", myIndex(), reciever);

   routerMsg *rMsg = new routerMsg(msgname);

   vector<int> route= {myIndex(),reciever};
   rMsg->setRoute(route);
   rMsg->setHopCount(0);
   rMsg->setMessageType(PRICE_UPDATE_MSG);

   priceUpdateMsg *puMsg = new priceUpdateMsg(msgname);
   puMsg->setNLocal(nLocal);
   puMsg->setBalSum(balSum);
   puMsg->setSumInFlight(sumInFlight);
   
   rMsg->encapsulate(puMsg);
   return rMsg;
}

routerMsg *routerNode::generateStatMessage(){
   char msgname[MSGSIZE];
   sprintf(msgname, "node-%d statMsg", myIndex());
   routerMsg *rMsg = new routerMsg(msgname);
   rMsg->setMessageType(STAT_MSG);
   return rMsg;
}

routerMsg *routerNode::generateRebalanceMessage(){
   char msgname[MSGSIZE];
   sprintf(msgname, "node-%d rebalanceMsg", myIndex());
   routerMsg *rMsg = new routerMsg(msgname);
   rMsg->setMessageType(REBALANCE_MSG);
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
      int destNode = get<4>(*it);

      if (simTime() > (msgArrivalTime + _maxTravelTime)){ // we can process

         //fixed not deleting from the queue
         // start queue searching
         vector<tuple<int, double, routerMsg*, Id>>* queuedTransUnits = &(nodeToPaymentChannel[nextNode].queuedTransUnits);

         auto iterQueue = find_if((*queuedTransUnits).begin(),
               (*queuedTransUnits).end(),
               [&transactionId](const tuple<int, double, routerMsg*, Id>& p)
               { return (get<0>(get<3>(p)) == transactionId); });
         while (iterQueue != (*queuedTransUnits).end()){

             routerMsg * rMsg = get<2>(*iterQueue);
                    auto tMsg = rMsg->getEncapsulatedPacket();
                    rMsg->decapsulate();
                    delete tMsg;
                    delete rMsg;
            iterQueue =   (*queuedTransUnits).erase(iterQueue);

            iterQueue = find_if((*queuedTransUnits).begin(),
                  (*queuedTransUnits).end(),
                  [&transactionId](const tuple<int, double, routerMsg*, Id>& p)
                  { return (get<0>(get<3>(p)) == transactionId); });

         }

         map<tuple<int,int>, double> *incomingTransUnits = &(nodeToPaymentChannel[prevNode].incomingTransUnits);
         auto iterIncoming = find_if((*incomingTransUnits).begin(),
               (*incomingTransUnits).end(),
               [&transactionId](const pair<tuple<int, int >, double> & p)
               { return get<0>(p.first) == transactionId; });

         while (iterIncoming != (*incomingTransUnits).end()){

            iterIncoming = (*incomingTransUnits).erase(iterIncoming);

            // start queue searching
            vector<tuple<int, double, routerMsg*, Id>>* queuedTransUnits = &(nodeToPaymentChannel[nextNode].queuedTransUnits);

            iterQueue = find_if((*queuedTransUnits).begin(),
                  (*queuedTransUnits).end(),
                  [&transactionId](const tuple<int, double, routerMsg*, Id>& p)
                  { return (get<0>(get<3>(p)) == transactionId); });
            if (iterQueue != (*queuedTransUnits).end()){
               iterQueue =   (*queuedTransUnits).erase(iterQueue);
            }
            else{

               if (manualFindQueuedTransUnitsByTransactionId((*queuedTransUnits), transactionId)){
                  cout << "could not find " << transactionId << "in " << endl;
                  checkQueuedTransUnits((*queuedTransUnits), nextNode);
               }
            }

            make_heap((*queuedTransUnits).begin(), (*queuedTransUnits).end(), sortPriorityThenAmtFunction);
            //find next in incoming
            iterIncoming = find_if((*incomingTransUnits).begin(),
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
                
               double updatedBalance = nodeToPaymentChannel[nextNode].balance + amount;
               updateBalance(nextNode, amount);
               nodeToPaymentChannel[nextNode].balanceEWMA = 
          (1 -_ewmaFactor) * nodeToPaymentChannel[nextNode].balanceEWMA + (_ewmaFactor) * updatedBalance;

               iterOutgoing = (*outgoingTransUnits).erase(iterOutgoing);

            }
            iterOutgoing = find_if((*outgoingTransUnits).begin(),
                  (*outgoingTransUnits).end(),
                  [&transactionId](const pair<tuple<int, int >, double> & p)
                  { return get<0>(p.first) == transactionId; });
         }
         it = canceledTransactions.erase(it);
      }//end if we can process
      else{
         it++;
      }
   }//end iterating over canceledTransactions
}

routerMsg *routerNode::generateClearStateMessage(){
   char msgname[MSGSIZE];
   sprintf(msgname, "node-%d clearStateMsg", myIndex());
   routerMsg *rMsg = new routerMsg(msgname);
   rMsg->setMessageType(CLEAR_STATE_MSG);
   return rMsg;
}


routerMsg *routerNode::generateTriggerPriceUpdateMessage(){
   char msgname[MSGSIZE];
   sprintf(msgname, "node-%d triggerPriceUpdateMsg", myIndex());
   routerMsg *rMsg = new routerMsg(msgname);
   rMsg->setMessageType(TRIGGER_PRICE_UPDATE_MSG);
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

void routerNode::handleStatMessagePriceScheme(routerMsg* ttmsg){

   if (_signalsEnabled) {
      for ( auto it = nodeToPaymentChannel.begin(); it!= nodeToPaymentChannel.end(); it++){ //iterate through all adjacent nodes

         int node = it->first; //key

         PaymentChannel* p = &(nodeToPaymentChannel[node]);
         emit(p->nValueSignal, p->lastNValue);
         emit(p->inFlightSumSignal, p->lastSumInFlight);
         emit(p->balSumSignal, p->lastBalSum);
         emit(p->lambdaSignal, p->lambda);
         emit(p->muLocalSignal, p->muLocal);
      }
   } // end if (_loggingEnabled)
}

void routerNode::handleStatMessage(routerMsg* ttmsg){
   if (simTime() > _simulationLength){
      delete ttmsg;
      deleteMessagesInQueues();
   }
   else{
      scheduleAt(simTime()+_statRate, ttmsg);
   }

   if (_signalsEnabled) {
      for ( auto it = nodeToPaymentChannel.begin(); it!= nodeToPaymentChannel.end(); it++){ //iterate through all adjacent nodes

         int node = it->first; //key

         emit(nodeToPaymentChannel[node].amtInQueuePerChannelSignal, getTotalAmount(nodeToPaymentChannel[node].queuedTransUnits));

         emit(nodeToPaymentChannel[node].balancePerChannelSignal, nodeToPaymentChannel[node].balance);

         // compute number in flight and report that too - hack right now that just sums the number of entries in the map
         emit(nodeToPaymentChannel[node].numInflightPerChannelSignal, 
                 getTotalAmount(nodeToPaymentChannel[node].incomingTransUnits) +
                 getTotalAmount(nodeToPaymentChannel[node].outgoingTransUnits));

      }
   }


}


void routerNode::forwardTimeOutMessage(routerMsg* msg){
   // Increment hop count.
   msg->setHopCount(msg->getHopCount()+1);

   //use hopCount to find next destination
   int nextDest = msg->getRoute()[msg->getHopCount()];
   send(msg, nodeToPaymentChannel[nextDest].gate);
}


void routerNode::forwardMessage(routerMsg *msg){
   msg->setHopCount(msg->getHopCount()+1);
   //use hopCount to find next destination
   int nextDest = msg->getRoute()[msg->getHopCount()];

   send(msg, nodeToPaymentChannel[nextDest].gate);
}

void routerNode::handleTimeOutMessage(routerMsg* ttmsg){
   timeOutMsg *toutMsg = check_and_cast<timeOutMsg *>(ttmsg->getEncapsulatedPacket());
   int nextNode = (ttmsg->getRoute())[ttmsg->getHopCount()+1];
   int prevNode = (ttmsg->getRoute())[ttmsg->getHopCount()-1];

   CanceledTrans ct = make_tuple(toutMsg->getTransactionId(),simTime(),prevNode, nextNode, toutMsg->getReceiver());
   canceledTransactions.insert(ct);
   forwardTimeOutMessage(ttmsg);

}

void routerNode::handleAckMessageTimeOut(routerMsg* ttmsg){
   ackMsg *aMsg = check_and_cast<ackMsg *>(ttmsg->getEncapsulatedPacket());
   //check if transactionId is in canceledTransaction set
   int transactionId = aMsg->getTransactionId();
   auto iter = find_if(canceledTransactions.begin(),
         canceledTransactions.end(),
         [&transactionId](const tuple<int, simtime_t, int, int, int>& p)
         { return get<0>(p) == transactionId; });
   if (iter!=canceledTransactions.end()){
      canceledTransactions.erase(iter);
   }


}

void routerNode::handleAckMessage(routerMsg* ttmsg){
   //generate updateMsg
   int prevNode = ttmsg->getRoute()[ttmsg->getHopCount()-1];
   int nextNode = ttmsg->getRoute()[ttmsg->getHopCount() + 1];

   //remove transaction from outgoing_trans_unit
   map<Id, double> *outgoingTransUnits = &(nodeToPaymentChannel[prevNode].outgoingTransUnits);
   ackMsg *aMsg = check_and_cast<ackMsg *>(ttmsg->getEncapsulatedPacket());

   (*outgoingTransUnits).erase(make_tuple(aMsg->getTransactionId(), aMsg->getHtlcIndex()));

   if (aMsg->getIsSuccess() == false){
      if (aMsg->getFailedHopNum() < ttmsg->getHopCount()) {
          //increment payment back to outgoing channel
          double updatedBalance = nodeToPaymentChannel[prevNode].balance + aMsg->getAmount();
          nodeToPaymentChannel[prevNode].balanceEWMA = 
              (1 -_ewmaFactor) * nodeToPaymentChannel[prevNode].balanceEWMA + (_ewmaFactor) * updatedBalance; 
          updateBalance(prevNode, aMsg->getAmount());
      }

      // this is nextNode on the ack path and so prev node in the forward path or rather
      // node sending you mayments
      int nextNode = ttmsg->getRoute()[ttmsg->getHopCount()+1];
      map<Id, double> *incomingTransUnits = &(nodeToPaymentChannel[nextNode].incomingTransUnits);
      (*incomingTransUnits).erase(make_tuple(aMsg->getTransactionId(), aMsg->getHtlcIndex()));

   }
   else{ //isSuccess == true
      routerMsg* uMsg =  generateUpdateMessage(aMsg->getTransactionId(), prevNode, aMsg->getAmount(), aMsg->getHtlcIndex() );
      sendUpdateMessage(uMsg);

   }
   forwardAckMessage(ttmsg);
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
   
   //remove transaction from incoming_trans_units
   if (_priceSchemeEnabled) {
       if (nodeToPaymentChannel[prevNode].balSum == -1)
          nodeToPaymentChannel[prevNode].balSum = 0;  
       nodeToPaymentChannel[prevNode].balSum += nodeToPaymentChannel[prevNode].balance/
           max(double(nodeToPaymentChannel[prevNode].incomingTransUnits.size()), 1.0);
   }

   double newBalance = nodeToPaymentChannel[prevNode].balance + uMsg->getAmount();
   updateBalance(prevNode, uMsg->getAmount());
   nodeToPaymentChannel[prevNode].balanceEWMA = 
          (1 -_ewmaFactor) * nodeToPaymentChannel[prevNode].balanceEWMA + (_ewmaFactor) * newBalance; 

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

routerMsg *routerNode::generateUpdateMessage(int transId, int receiver, double amount, int htlcIndex){
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
   uMsg->setHtlcIndex(htlcIndex);
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


bool routerNode::handleTransactionMessageTimeOut(routerMsg* ttmsg){
   transactionMsg *transMsg = check_and_cast<transactionMsg *>(ttmsg->getEncapsulatedPacket());

   int transactionId = transMsg->getTransactionId();

   auto iter = find_if(canceledTransactions.begin(),
         canceledTransactions.end(),
         [&transactionId](const tuple<int, simtime_t, int, int, int>& p)
         { return get<0>(p) == transactionId; });

   if ( iter!=canceledTransactions.end() ){
      //delete yourself
      ttmsg->decapsulate();
      delete transMsg;
      delete ttmsg;
      return true;
   }
   return false;
}

void routerNode::handleTransactionMessagePriceScheme(routerMsg* ttmsg){ //increment nValue
   int hopcount = ttmsg->getHopCount();
   vector<tuple<int, double , routerMsg *, Id>> *q;
   transactionMsg *transMsg = check_and_cast<transactionMsg *>(ttmsg->getEncapsulatedPacket());

   //not a self-message, add to incoming_trans_units
   int nextNode = ttmsg->getRoute()[ttmsg->getHopCount()+1];
   nodeToPaymentChannel[nextNode].nValue = nodeToPaymentChannel[nextNode].nValue + transMsg->getAmount();

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

   //not a self-message, add to incoming_trans_units
   int prevNode = ttmsg->getRoute()[ttmsg->getHopCount()-1];
   map<Id, double> *incomingTransUnits = &(nodeToPaymentChannel[prevNode].incomingTransUnits);
   (*incomingTransUnits)[make_tuple(transMsg->getTransactionId(), transMsg->getHtlcIndex())] = transMsg->getAmount();

   int nextNode = ttmsg->getRoute()[hopcount+1];


   q = &(nodeToPaymentChannel[nextNode].queuedTransUnits);

    if (_hasQueueCapacity && _queueCapacity == 0) {
       if (forwardTransactionMessage(ttmsg, nextNode) == false) {
          // if there isn't balance, because cancelled txn case will never be hit
          // TODO: make this and forward txn message cleaner
          // maybe just clean out queue when a timeout arrives as opposed to after clear state
         routerMsg * failedAckMsg = generateAckMessage(ttmsg, false);
         handleAckMessage(failedAckMsg);
      }
    }
    else if (_hasQueueCapacity && _queueCapacity<=(*q).size()){ 
        //failed transaction, queue at capacity, others are in queue so no point trying this txn
      routerMsg * failedAckMsg = generateAckMessage(ttmsg, false);
      handleAckMessage(failedAckMsg);
   }
   else{
      (*q).push_back(make_tuple(transMsg->getPriorityClass(), transMsg->getAmount(),
               ttmsg, make_tuple(transMsg->getTransactionId(), transMsg->getHtlcIndex())));
      push_heap((*q).begin(), (*q).end(), sortPriorityThenAmtFunction);
      processTransUnits(nextNode, *q);
   }
}

routerMsg *routerNode::generateAckMessage(routerMsg* ttmsg, bool isSuccess ){ //default is true
   int transactionId;
   double timeSent;
   double amount;
   int sender = (ttmsg->getRoute())[0];
   int receiver = (ttmsg->getRoute())[(ttmsg->getRoute()).size() -1];
   bool hasTimeOut;

   int nextNode = ttmsg->getRoute()[ttmsg->getHopCount()+1];


   transactionMsg *transMsg = check_and_cast<transactionMsg *>(ttmsg->getEncapsulatedPacket());
   transactionId = transMsg->getTransactionId();
   timeSent = transMsg->getTimeSent();
   amount = transMsg->getAmount();
   hasTimeOut = transMsg->getHasTimeOut();
   char msgname[MSGSIZE];

   sprintf(msgname, "receiver-%d-to-sender-%d ackMsg", receiver, sender);
   routerMsg *msg = new routerMsg(msgname);
   ackMsg *aMsg = new ackMsg(msgname);
   aMsg->setTransactionId(transactionId);
   aMsg->setIsSuccess(isSuccess);
   aMsg->setTimeSent(timeSent);
   aMsg->setAmount(amount);
   aMsg->setReceiver(transMsg->getReceiver());
   aMsg->setHasTimeOut(hasTimeOut);
   aMsg->setHtlcIndex(transMsg->getHtlcIndex());
   aMsg->setPathIndex(transMsg->getPathIndex());

   if (!isSuccess) {
      aMsg->setFailedHopNum((ttmsg->getRoute().size()-1) - ttmsg->getHopCount());
   }
      //no need to set secret;
     vector<int> route = ttmsg->getRoute();
     //route.resize(ttmsg->getHopCount() + 1); //don't resize, might mess up destination
     reverse(route.begin(), route.end());

     msg->setRoute(route);
     msg->setHopCount((route.size()-1) - ttmsg->getHopCount());

     //need to reverse path from current hop number in case of partial failure
     msg->setMessageType(ACK_MSG);
       ttmsg->decapsulate();
       if (_lndBaselineEnabled)
       {
           aMsg->encapsulate(transMsg);
           msg->encapsulate(aMsg);
           delete ttmsg;
       }
       else
       {
           delete transMsg;
           delete ttmsg;
           msg->encapsulate(aMsg);
       }
       return msg;
}


/*
 * processTransUnits - given an adjacent node, and TransUnit queue of things to send to that node, sends
 *  TransUnits until channel funds are too low by calling forwardMessage on message representing TransUnit
 */
void routerNode:: processTransUnits(int dest, vector<tuple<int, double , routerMsg *, Id>>& q){
   bool successful = true;

   while((int)q.size()>0 && successful){
      successful = forwardTransactionMessage(get<2>(q.back()), dest);
      if (successful){
         q.pop_back();
      }
   }
}


/*
 *  forwardTransactionMessage - given a message representing a TransUnit, increments hopCount, finds next destination,
 *      adjusts (decrements) channel balance, sends message to next node on route
 */

// call this dest nextnode
bool routerNode::forwardTransactionMessage(routerMsg *msg, int dest)
{
   transactionMsg *transMsg = check_and_cast<transactionMsg *>(msg->getEncapsulatedPacket());

   int nextDest = msg->getRoute()[msg->getHopCount()+1];

   if (nodeToPaymentChannel[nextDest].balance <= 0 || transMsg->getAmount() > nodeToPaymentChannel[nextDest].balance){
      return false;
   } 
   else {
      // check if txn has been cancelled and if so return without doing anything to move on to the next transaction
      int transactionId = transMsg->getTransactionId();
      auto iter = find_if(canceledTransactions.begin(),
           canceledTransactions.end(),
           [&transactionId](const tuple<int, simtime_t, int, int, int>& p)
           { return get<0>(p) == transactionId; });

      // can potentially erase info?
      if (iter != canceledTransactions.end()) {
         // message won't be encountered again
         msg->decapsulate();
         delete transMsg;
         delete msg;
         return true;
      }

      // Increment hop count.
      msg->setHopCount(msg->getHopCount()+1);
      //use hopCount to find next destination

      //add amount to outgoing map
      if (_priceSchemeEnabled) 
          nodeToPaymentChannel[nextDest].sumInFlight += transMsg->getAmount();
      map<Id, double> *outgoingTransUnits = &(nodeToPaymentChannel[nextDest].outgoingTransUnits);
      (*outgoingTransUnits)[make_tuple(transMsg->getTransactionId(), transMsg->getHtlcIndex())] = transMsg->getAmount();

      //numSentPerChannel incremented every time (key,value) pair added to outgoing_trans_units map
      nodeToPaymentChannel[nextDest].statNumSent =  nodeToPaymentChannel[nextDest].statNumSent+1;

      double amt = transMsg->getAmount();
      double newBalance = nodeToPaymentChannel[nextDest].balance - amt;
      updateBalance(nextDest, -1 * amt);
      nodeToPaymentChannel[nextDest].balance = newBalance;
      nodeToPaymentChannel[nextDest].balanceEWMA = 
          (1 -_ewmaFactor) * nodeToPaymentChannel[nextDest].balanceEWMA + (_ewmaFactor) * newBalance;

      send(msg, nodeToPaymentChannel[nextDest].gate);
      return true;
   } //end else (transMsg->getAmount() >nodeToPaymentChannel[dest].balance)
}

double routerNode::rebalanceTotalAmtAtNode(){
    double total = 0;
    for(auto iter = nodeToPaymentChannel.begin(); iter != nodeToPaymentChannel.end(); ++iter)
    {
        int key =iter->first; //node
        total += nodeToPaymentChannel[key].balanceAdded;
    }
    return total;
}

