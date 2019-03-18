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
      nodeToPaymentChannel[key].origTotalCapacity = nodeToPaymentChannel[key].balance + balanceOpp;
      nodeToPaymentChannel[key].lambda = 0;
      nodeToPaymentChannel[key].muLocal = 0;
      nodeToPaymentChannel[key].muRemote = 0;

      //initialize queuedTransUnits
      vector<tuple<int, double , routerMsg *, Id, simtime_t>> temp;
      make_heap(temp.begin(), temp.end(), sortFIFO);
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

      //balancePerChannel signal
      if (key<_numHostNodes){
         sprintf(signalName, "queueDelayEWMAPerChannel(host %d)", key);
      }
      else{
         sprintf(signalName, "queueDelayEWMAPerChannel(router %d [%d])", key-_numHostNodes, key);
      }

      signal = registerSignal(signalName);
      statisticTemplate = getProperties()->get("statisticTemplate", "queueDelayEWMAPerChannelTemplate");
      getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
      nodeToPaymentChannel[key].queueDelayEWMASignal = signal;
      
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
            sprintf(signalName, "fakeRebalanceQPerChannel(host %d)", key);
         }
         else {
            sprintf(signalName, "fakeRebalanceQPerChannel(router %d [%d])", key - _numHostNodes, key);
         }
         signal = registerSignal(signalName);
         statisticTemplate = getProperties()->get("statisticTemplate", "fakeRebalanceQPerChannelTemplate");
         getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
         nodeToPaymentChannel[key].fakeRebalanceQSignal = signal;

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
            sprintf(signalName, "serviceRatePerChannel(host %d)", key);
         }
         else {
            sprintf(signalName, "serviceRatePerChannel(router %d [%d])", key - _numHostNodes, key);
         }
         signal = registerSignal(signalName);
         statisticTemplate = getProperties()->get("statisticTemplate", "serviceRatePerChannelTemplate");
         getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
         nodeToPaymentChannel[key].serviceRateSignal = signal;

         if (key<_numHostNodes) {
            sprintf(signalName, "arrivalRatePerChannel(host %d)", key);
         }
         else {
            sprintf(signalName, "arrivalRatePerChannel(router %d [%d])", key - _numHostNodes, key);
         }
         signal = registerSignal(signalName);
         statisticTemplate = getProperties()->get("statisticTemplate", "arrivalRatePerChannelTemplate");
         getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
         nodeToPaymentChannel[key].arrivalRateSignal = signal;

         if (key<_numHostNodes) {
            sprintf(signalName, "inflightOutgoingPerChannel(host %d)", key);
         }
         else {
            sprintf(signalName, "inflightOutgoingPerChannel(router %d [%d])", key - _numHostNodes, key);
         }
         signal = registerSignal(signalName);
         statisticTemplate = getProperties()->get("statisticTemplate", "inflightOutgoingPerChannelTemplate");
         getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
         nodeToPaymentChannel[key].inflightOutgoingSignal = signal;

         if (key<_numHostNodes) {
            sprintf(signalName, "inflightIncomingPerChannel(host %d)", key);
         }
         else {
            sprintf(signalName, "inflightIncomingPerChannel(router %d [%d])", key - _numHostNodes, key);
         }
         signal = registerSignal(signalName);
         statisticTemplate = getProperties()->get("statisticTemplate", "inflightIncomingPerChannelTemplate");
         getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
         nodeToPaymentChannel[key].inflightIncomingSignal = signal;

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

}

void routerNode::finish(){
    deleteMessagesInQueues();
    double numPoints = (_transStatEnd - _transStatStart)/(double) _statRate;
    double sumRebalancing = 0;
    double sumAmtAdded = 0;

    for ( auto it = nodeToPaymentChannel.begin(); it!= nodeToPaymentChannel.end(); it++){ //iterate through all adjacent nodes
        int node = it->first; //key
        char buffer[30];
        sprintf(buffer, "queueSize %d -> %d", myIndex(), node);
        recordScalar(buffer, nodeToPaymentChannel[node].queueSizeSum/numPoints);

        sumRebalancing += nodeToPaymentChannel[node].numRebalanceEvents;
        sumAmtAdded += nodeToPaymentChannel[node].amtAdded;
        
    }
        char buffer[30];
        sprintf(buffer, "totalNumRebalancingEvents %d", myIndex());
        recordScalar(buffer, sumRebalancing);
        sprintf(buffer, "totalAmtAdded %d", myIndex());
        recordScalar(buffer, sumAmtAdded);


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

      if (ttmsg->getHopCount() < ttmsg->getRoute().size() - 2) {
        zNew += (2 * lambda) + muLocal  - muRemote;
      }
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
    double serviceRateRemote = puMsg->getServiceRate();
    double arrivalRateRemote = puMsg->getArrivalRate();
    int qRemote = puMsg->getQueueSize();
    int sender = ttmsg->getRoute()[0];
    PaymentChannel *neighborChannel = &(nodeToPaymentChannel[sender]);
    int inflightRemote = neighborChannel->incomingTransUnits.size() + serviceRateRemote * _avgDelay/1000 ; 

    double xLocal = neighborChannel->xLocal;
    double updateRateLocal = neighborChannel->updateRate;
    int nLocal = neighborChannel->lastNValue;
    int inflightLocal = neighborChannel->outgoingTransUnits.size() + updateRateLocal* _avgDelay/1000.0;
    int qLocal = neighborChannel->lastQueueSize;
    double serviceRateLocal = neighborChannel->serviceRate;
    double arrivalRateLocal = neighborChannel->arrivalRate;
 
    tuple<int, int> node1node2Pair = (myIndex() < sender) ? make_tuple(myIndex(), sender) : make_tuple(sender,
            myIndex());
   double cValue = _capacities[node1node2Pair]; 
   double oldLambda = nodeToPaymentChannel[sender].lambda;
   double oldMuLocal = nodeToPaymentChannel[sender].muLocal;
   double oldMuRemote = nodeToPaymentChannel[sender].muRemote;

    double newLambdaGrad;
    if (_useQueueEquation) {
     newLambdaGrad	= inflightLocal*arrivalRateLocal/serviceRateLocal + 
       inflightRemote * arrivalRateRemote/serviceRateRemote + 2*_xi*min(qLocal, qRemote) - (_capacityFactor * cValue);
    } else {
     newLambdaGrad = inflightLocal*arrivalRateLocal/serviceRateLocal + 
       inflightRemote * arrivalRateRemote/serviceRateRemote - (_capacityFactor * cValue); 
    }
     /*if (sender >= _numHostNodes && (oldLambda > 0 || newLambdaGrad > 0)) {
        cout << "to sender " << sender << " from  " << myIndex() << " at time " << simTime();
        cout << " old lambda" << oldLambda << " new lambda grad "
            << newLambdaGrad << " service rate local " << serviceRateLocal << 
       " service rate remote " << serviceRateRemote << " arrival rate local " << arrivalRateLocal 
      << " arrival rate remote " << arrivalRateRemote << "extra term " << serviceRateRemote * _avgDelay/1000 
      << " inflightLocal " << inflightLocal <<
      "inflightRemote " << inflightRemote << "cValue " << cValue  << "updateRateLocal " << updateRateLocal
      << "qLocal" << qLocal << "qRemote" << qRemote << endl; 
     } */
        
    double newMuLocalGrad;
    if (_useQueueEquation) {
     newMuLocalGrad	= nLocal - nRemote + qLocal*_tUpdate/_routerQueueDrainTime -
       qRemote*_tUpdate/_routerQueueDrainTime;
    } else {
     newMuLocalGrad	= nLocal - nRemote;
    }
    /*cout << " to sender " << sender << " from " << myIndex() << " at time " << simTime();
    cout << "nLocal " << nLocal << " nRemote " << nRemote << " qlocal and term " << qLocal 
        << " " << qLocal * _tUpdate / _routerQueueDrainTime << "qRemote " << qRemote
        << " " << qLocal * _tUpdate / _routerQueueDrainTime << endl;*/
    
    // Nesterov's gradient descent equation
    // and other speeding up mechanisms
    double newLambda = 0.0;
    double newMuLocal = 0.0;
    double newMuRemote = 0.0;
    double myKappa = _kappa /**20.0 /cValue*/;
    double myEta = _eta /** 20.0 / cValue*/;
    
    if (_nesterov) {
        double yLambda = nodeToPaymentChannel[sender].yLambda;
        double yMuLocal = nodeToPaymentChannel[sender].yMuLocal;
        double yMuRemote = nodeToPaymentChannel[sender].yMuRemote;

        double yLambdaNew = oldLambda + myEta*newLambdaGrad;
        newLambda = yLambdaNew + _rhoLambda*(yLambdaNew - yLambda); 
        nodeToPaymentChannel[sender].yLambda = yLambdaNew;

        double yMuLocalNew = oldMuLocal + myKappa*newMuLocalGrad;
        newMuLocal = yMuLocalNew + _rhoMu*(yMuLocalNew - yMuLocal);
        nodeToPaymentChannel[sender].yMuLocal = yMuLocalNew;

        double yMuRemoteNew = oldMuRemote - myKappa*newMuLocalGrad;
        newMuRemote = yMuRemoteNew + _rhoMu*(yMuRemoteNew - yMuRemote);
        nodeToPaymentChannel[sender].yMuRemote = yMuRemoteNew;
    } 
    /*else if (_secondOrderOptimization) {
        double lastLambdaGrad = nodeToPaymentChannel[sender].lastLambdaGrad;
        newLambda = oldLambda +  myEta*newLambdaGrad + _rhoLambda*(newLambdaGrad - lastLambdaGrad);
        nodeToPaymentChannel[sender].lastLambdaGrad = newLambdaGrad;

        double lastMuLocalGrad = nodeToPaymentChannel[sender].lastMuLocalGrad;
        newMuLocal = oldMuLocal + myKappa*newMuLocalGrad + _rhoMu*(newMuLocalGrad - lastMuLocalGrad);
        newMuRemote = oldMuRemote - myKappa*newMuLocalGrad - _rhoMu*(newMuLocalGrad - lastMuLocalGrad);
        nodeToPaymentChannel[sender].lastMuLocalGrad = newMuLocalGrad;
    } */
    else {
        newLambda = oldLambda +  myEta*newLambdaGrad;
        newMuLocal = oldMuLocal + myKappa*newMuLocalGrad;
        newMuRemote = oldMuRemote - myKappa*newMuLocalGrad; 
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
      neighborChannel->updateRate = neighborChannel->numUpdateMessages / _tUpdate;
        
      auto firstTransTimes = neighborChannel->serviceArrivalTimeStamps.front();
      auto lastTransTimes =  neighborChannel->serviceArrivalTimeStamps.back();
      double serviceTimeDiff = get<0>(lastTransTimes).dbl() - get<0>(firstTransTimes).dbl(); 
      double arrivalTimeDiff = get<1>(lastTransTimes).dbl() - get<1>(firstTransTimes).dbl(); 

      neighborChannel->serviceRate = _serviceArrivalWindow / serviceTimeDiff; 
      neighborChannel->arrivalRate = _serviceArrivalWindow / arrivalTimeDiff;

      neighborChannel->lastQueueSize = neighborChannel->queuedTransUnits.size();
      
      routerMsg * priceUpdateMsg = generatePriceUpdateMessage(neighborChannel->nValue, 
              neighborChannel->serviceRate, neighborChannel->arrivalRate, 
            neighborChannel->queuedTransUnits.size(), it->first);
      
      neighborChannel->lastNValue = neighborChannel->nValue;
      neighborChannel->nValue = 0;
      neighborChannel->numUpdateMessages = 0;
      
      sendUpdateMessage(priceUpdateMsg);
   }

}

routerMsg * routerNode::generatePriceUpdateMessage(double nLocal, double serviceRate, double arrivalRate, 
        int queueSize, int reciever){
   char msgname[MSGSIZE];

   sprintf(msgname, "tic-%d-to-%d priceUpdateMsg", myIndex(), reciever);

   routerMsg *rMsg = new routerMsg(msgname);

   vector<int> route= {myIndex(),reciever};
   rMsg->setRoute(route);
   rMsg->setHopCount(0);
   rMsg->setMessageType(PRICE_UPDATE_MSG);

   priceUpdateMsg *puMsg = new priceUpdateMsg(msgname);
    puMsg->setNLocal(nLocal);
    puMsg->setServiceRate(serviceRate);
    puMsg->setArrivalRate(arrivalRate);
    puMsg->setQueueSize(queueSize);
   
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

bool manualFindQueuedTransUnitsByTransactionId( vector<tuple<int, double, routerMsg*, Id, simtime_t>> (queuedTransUnits), int transactionId){
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
    
   /* hack for now to do this periodically */
    if (_rebalancingEnabled && !_priceSchemeEnabled) {
        for ( auto it = nodeToPaymentChannel.begin(); it!= nodeToPaymentChannel.end(); it++ ) {
            PaymentChannel *neighborChannel = &(nodeToPaymentChannel[it->first]);   

            auto lastTransTimes =  neighborChannel->serviceArrivalTimeStamps.back();
            double curQueueingDelay = get<0>(lastTransTimes).dbl() - get<1>(lastTransTimes).dbl();
            neighborChannel->queueDelayEWMA = 0.6*curQueueingDelay + 0.4*neighborChannel->queueDelayEWMA;

            if (neighborChannel->queueDelayEWMA > _queueDelayThreshold) {
                neighborChannel->balance += getTotalAmount(neighborChannel->queuedTransUnits);
                tuple<int, int> senderReceiverTuple = 
                    (it->first < myIndex()) ? make_tuple(it->first, myIndex()) :
                    make_tuple(myIndex(), it->first);
                    _capacities[senderReceiverTuple] += getTotalAmount(neighborChannel->queuedTransUnits);
            
                neighborChannel->numRebalanceEvents += 1; 
                processTransUnits(it->first, neighborChannel->queuedTransUnits);
            }
        }
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
         vector<tuple<int, double, routerMsg*, Id, simtime_t>>* queuedTransUnits = 
             &(nodeToPaymentChannel[nextNode].queuedTransUnits);

         auto iterQueue = find_if((*queuedTransUnits).begin(),
               (*queuedTransUnits).end(),
               [&transactionId](const tuple<int, double, routerMsg*, Id, simtime_t>& p)
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
                  [&transactionId](const tuple<int, double, routerMsg*, Id, simtime_t>& p)
                  { return (get<0>(get<3>(p)) == transactionId); });
         }
         make_heap((*queuedTransUnits).begin(), (*queuedTransUnits).end(), sortFIFO);

         map<tuple<int,int>, double> *incomingTransUnits = &(nodeToPaymentChannel[prevNode].incomingTransUnits);
         auto iterIncoming = find_if((*incomingTransUnits).begin(),
               (*incomingTransUnits).end(),
               [&transactionId](const pair<tuple<int, int >, double> & p)
               { return get<0>(p.first) == transactionId; });

         while (iterIncoming != (*incomingTransUnits).end()){

            iterIncoming = (*incomingTransUnits).erase(iterIncoming);

            // start queue searching
            vector<tuple<int, double, routerMsg*, Id, simtime_t>>* queuedTransUnits = 
                &(nodeToPaymentChannel[nextNode].queuedTransUnits);

            iterQueue = find_if((*queuedTransUnits).begin(),
                  (*queuedTransUnits).end(),
                  [&transactionId](const tuple<int, double, routerMsg*, Id, simtime_t>& p)
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

            make_heap((*queuedTransUnits).begin(), (*queuedTransUnits).end(), sortFIFO);
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
               nodeToPaymentChannel[nextNode].balance = updatedBalance; 
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


void routerNode::checkQueuedTransUnits(vector<tuple<int, double, routerMsg*,  Id, simtime_t>> queuedTransUnits, int nextNode){
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
         emit(p->fakeRebalanceQSignal, p->fakeRebalancingQueue);
         emit(p->inflightOutgoingSignal, p->outgoingTransUnits.size());
         emit(p->inflightIncomingSignal, p->incomingTransUnits.size());
         emit(p->serviceRateSignal, p->arrivalRate/p->serviceRate);
         //emit(p->arrivalRateSignal, p->arrivalRate);
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


    for ( auto it = nodeToPaymentChannel.begin(); it!= nodeToPaymentChannel.end(); it++){ //iterate through all adjacent nodes
        int node = it->first; //key
        if (simTime() > _transStatStart && simTime() < _transStatEnd)
            nodeToPaymentChannel[node].queueSizeSum += getTotalAmount(nodeToPaymentChannel[node].queuedTransUnits);

        if (_signalsEnabled) {
            emit(nodeToPaymentChannel[node].amtInQueuePerChannelSignal, 
                    getTotalAmount(nodeToPaymentChannel[node].queuedTransUnits));
            emit(nodeToPaymentChannel[node].balancePerChannelSignal, nodeToPaymentChannel[node].balance);

            // compute number in flight and report that too - hack right now that just sums the number of entries in the map
            emit(nodeToPaymentChannel[node].numInflightPerChannelSignal, 
                 getTotalAmount(nodeToPaymentChannel[node].incomingTransUnits) +
                 getTotalAmount(nodeToPaymentChannel[node].outgoingTransUnits));
            emit(nodeToPaymentChannel[node].queueDelayEWMASignal, nodeToPaymentChannel[node].queueDelayEWMA);
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
          nodeToPaymentChannel[prevNode].balance = updatedBalance;
      }

      // this is nextNode on the ack path and so prev node in the forward path or rather
      // node sending you mayments
      int nextNode = ttmsg->getRoute()[ttmsg->getHopCount()+1];
      map<Id, double> *incomingTransUnits = &(nodeToPaymentChannel[nextNode].incomingTransUnits);
      (*incomingTransUnits).erase(make_tuple(aMsg->getTransactionId(), aMsg->getHtlcIndex()));

   }
   else{ //isSuccess == true
      routerMsg* uMsg =  generateUpdateMessage(aMsg->getTransactionId(), prevNode, aMsg->getAmount(), aMsg->getHtlcIndex() );
      nodeToPaymentChannel[prevNode].numUpdateMessages += 1;
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
   vector<tuple<int, double , routerMsg *, Id, simtime_t>> *q;
   int prevNode = msg->getRoute()[msg->getHopCount()-1];

   updateMsg *uMsg = check_and_cast<updateMsg *>(msg->getEncapsulatedPacket());
   //increment the in flight funds back
   
   double newBalance = nodeToPaymentChannel[prevNode].balance + uMsg->getAmount();
   nodeToPaymentChannel[prevNode].balance =  newBalance;       
   nodeToPaymentChannel[prevNode].balanceEWMA = 
          (1 -_ewmaFactor) * nodeToPaymentChannel[prevNode].balanceEWMA + (_ewmaFactor) * newBalance; 
   double origTotalCapacity = nodeToPaymentChannel[prevNode].origTotalCapacity;

    if (_rebalancingEnabled) {
        if (newBalance > _rebalancingUpFactor * origTotalCapacity) {
            nodeToPaymentChannel[prevNode].balance = origTotalCapacity;
            tuple<int, int> senderReceiverTuple = (prevNode < myIndex()) ? make_tuple(prevNode, myIndex()) :
                make_tuple(myIndex(), prevNode);
            _capacities[senderReceiverTuple] -= (_rebalancingUpFactor - 1)*origTotalCapacity;

            nodeToPaymentChannel[prevNode].numRebalanceEvents += 1;
            nodeToPaymentChannel[prevNode].amtAdded -= (_rebalancingUpFactor - 1) * origTotalCapacity;
        }
    }

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
   vector<tuple<int, double , routerMsg *, Id, simtime_t>> *q;
   transactionMsg *transMsg = check_and_cast<transactionMsg *>(ttmsg->getEncapsulatedPacket());

   int destination = transMsg->getReceiver();

   //not a self-message, add to incoming_trans_units
   int prevNode = ttmsg->getRoute()[ttmsg->getHopCount()-1];
   map<Id, double> *incomingTransUnits = &(nodeToPaymentChannel[prevNode].incomingTransUnits);
   (*incomingTransUnits)[make_tuple(transMsg->getTransactionId(), transMsg->getHtlcIndex())] = transMsg->getAmount();

   int nextNode = ttmsg->getRoute()[hopcount+1];

   q = &(nodeToPaymentChannel[nextNode].queuedTransUnits);

    if (_hasQueueCapacity && _queueCapacity == 0) {
       if (forwardTransactionMessage(ttmsg, nextNode, simTime()) == false) {
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
               ttmsg, make_tuple(transMsg->getTransactionId(), transMsg->getHtlcIndex()), simTime()));
      push_heap((*q).begin(), (*q).end(), sortFIFO);
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
void routerNode:: processTransUnits(int dest, vector<tuple<int, double , routerMsg *, Id, simtime_t>>& q){
   bool successful = true;

   while((int)q.size()>0 && successful){
      pop_heap(q.begin(), q.end(), sortFIFO);
      successful = forwardTransactionMessage(get<2>(q.back()), dest, get<4>(q.back()));
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
bool routerNode::forwardTransactionMessage(routerMsg *msg, int dest, simtime_t arrivalTime)
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

        // update service arrival times
        nodeToPaymentChannel[nextDest].serviceArrivalTimeStamps.push_back(make_tuple(simTime(), arrivalTime));
        if (nodeToPaymentChannel[nextDest].serviceArrivalTimeStamps.size() > _serviceArrivalWindow)
           nodeToPaymentChannel[nextDest].serviceArrivalTimeStamps.pop_front(); 
     
        //add amount to outgoing map
      map<Id, double> *outgoingTransUnits = &(nodeToPaymentChannel[nextDest].outgoingTransUnits);
      (*outgoingTransUnits)[make_tuple(transMsg->getTransactionId(), transMsg->getHtlcIndex())] = transMsg->getAmount();

      //numSentPerChannel incremented every time (key,value) pair added to outgoing_trans_units map
      nodeToPaymentChannel[nextDest].statNumSent =  nodeToPaymentChannel[nextDest].statNumSent+1;

      double amt = transMsg->getAmount();
      double newBalance = nodeToPaymentChannel[nextDest].balance - amt;
      nodeToPaymentChannel[nextDest].balance = newBalance;
      nodeToPaymentChannel[nextDest].balanceEWMA = 
          (1 -_ewmaFactor) * nodeToPaymentChannel[nextDest].balanceEWMA + (_ewmaFactor) * newBalance;

      send(msg, nodeToPaymentChannel[nextDest].gate);
      return true;
   } //end else (transMsg->getAmount() >nodeToPaymentChannel[dest].balance)
}

