#include "routerNodePriceScheme.h"

Define_Module(routerNodePriceScheme);

/* additional initialization that has to be done for the price based scheme
 * in particular set price variables to zero, initialize more signals
 * and schedule the first price update and price trigger
 */
void routerNodePriceScheme::initialize() {
    routerNodeBase::initialize();

    for(auto iter = nodeToPaymentChannel.begin(); iter != nodeToPaymentChannel.end(); ++iter) {
        int key = iter->first;
        
        nodeToPaymentChannel[key].lambda = nodeToPaymentChannel[key].yLambda = 0;
        nodeToPaymentChannel[key].muLocal = nodeToPaymentChannel[key].yMuLocal = 0;
        nodeToPaymentChannel[key].muRemote = nodeToPaymentChannel[key].yMuRemote = 0;

        // register signals for price per payment channel
        simsignal_t signal;
        signal = registerSignalPerChannel("nValue", key);
        nodeToPaymentChannel[key].nValueSignal = signal;
        
        signal = registerSignalPerChannel("muLocal", key);
        nodeToPaymentChannel[key].muLocalSignal = signal;

        signal = registerSignalPerChannel("lambda", key);
        nodeToPaymentChannel[key].lambdaSignal = signal;

        signal = registerSignalPerChannel("fakeRebalanceQ", key);
        nodeToPaymentChannel[key].fakeRebalanceQSignal = signal;

        signal = registerSignalPerChannel("serviceRate", key);
        nodeToPaymentChannel[key].serviceRateSignal = signal;

        signal = registerSignalPerChannel("arrivalRate", key);
        nodeToPaymentChannel[key].arrivalRateSignal = signal;
        
        signal = registerSignalPerChannel("inflightOutgoing", key);
        nodeToPaymentChannel[key].inflightOutgoingSignal = signal;

        signal = registerSignalPerChannel("inflightIncoming", key);
        nodeToPaymentChannel[key].inflightIncomingSignal = signal;
    }

    // trigger the first set of triggers for price update 
    routerMsg *triggerPriceUpdateMsg = generateTriggerPriceUpdateMessage();
    scheduleAt(simTime() + _tUpdate, triggerPriceUpdateMsg );
}


/********* MESSAGE GENERATORS **************/
/* generate the trigger message to initiate price Updates periodically
 */
routerMsg *routerNodePriceScheme::generateTriggerPriceUpdateMessage(){
    char msgname[MSGSIZE];
    sprintf(msgname, "node-%d triggerPriceUpdateMsg", myIndex());
    routerMsg *rMsg = new routerMsg(msgname);
    rMsg->setMessageType(TRIGGER_PRICE_UPDATE_MSG);
    return rMsg;
}

/* generate the actual price Update message when a triggerPriceUpdate
 * tells you to update your price to be sent to your neighbor to tell
 * them your xLocal value
 */
routerMsg * routerNodePriceScheme::generatePriceUpdateMessage(double nLocal, double serviceRate, 
        double arrivalRate, double queueSize, int receiver){
    char msgname[MSGSIZE];
    sprintf(msgname, "tic-%d-to-%d priceUpdateMsg", myIndex(), receiver);
    routerMsg *rMsg = new routerMsg(msgname);
    vector<int> route={myIndex(),receiver};
   
    rMsg->setRoute(route);
    rMsg->setHopCount(0);
    rMsg->setMessageType(PRICE_UPDATE_MSG);
    
    priceUpdateMsg *puMsg = new priceUpdateMsg(msgname);
    puMsg->setNLocal(nLocal);
    puMsg->setServiceRate(serviceRate);
    puMsg->setQueueSize(queueSize);
    puMsg->setArrivalRate(arrivalRate);

    rMsg->encapsulate(puMsg);
    return rMsg;
}


/******* MESSAGE HANDLERS **************************/

/* overall controller for handling messages that dispatches the right function
 * based on message type in price Scheme
 */
void routerNodePriceScheme::handleMessage(cMessage *msg) {
    routerMsg *ttmsg = check_and_cast<routerMsg *>(msg);
    if (simTime() > _simulationLength){
        auto encapMsg = (ttmsg->getEncapsulatedPacket());
        ttmsg->decapsulate();
        delete ttmsg;
        delete encapMsg;
        return;
    } 

    switch(ttmsg->getMessageType()) {
        case TRIGGER_PRICE_UPDATE_MSG:
             if (_loggingEnabled) cout<< "[ROUTER "<< myIndex() 
                 <<": RECEIVED TRIGGER_PRICE_UPDATE MSG] "<< ttmsg->getName() << endl;
             handleTriggerPriceUpdateMessage(ttmsg);
             if (_loggingEnabled) cout<< "[AFTER HANDLING:]  "<< endl;
             break;

        case PRICE_UPDATE_MSG:
             if (_loggingEnabled) cout<< "[ROUTER "<< myIndex() 
                 <<": RECEIVED PRICE_UPDATE MSG] "<< ttmsg->getName() << endl;
             handlePriceUpdateMessage(ttmsg);
             if (_loggingEnabled) cout<< "[AFTER HANDLING:]  "<< endl;
             break;

        case PRICE_QUERY_MSG:
             if (_loggingEnabled) cout<< "[ROUTER "<< myIndex() 
                 <<": RECEIVED PRICE_QUERY MSG] "<< ttmsg->getName() << endl;
             handlePriceQueryMessage(ttmsg);
             if (_loggingEnabled) cout<< "[AFTER HANDLING:]  "<< endl;
             break;

        default:
             routerNodeBase::handleMessage(msg);

    }
}

/* handler for the statistic message triggered every x seconds to also
 * output the price based scheme stats in addition to the default
 */
void routerNodePriceScheme::handleStatMessage(routerMsg* ttmsg) {
    if (_signalsEnabled) {
        // per router payment channel stats
        for ( auto it = nodeToPaymentChannel.begin(); it!= nodeToPaymentChannel.end(); it++){
            int node = it->first; //key
            PaymentChannel* p = &(nodeToPaymentChannel[node]);
            emit(p->nValueSignal, p->lastNValue);
            emit(p->fakeRebalanceQSignal, p->fakeRebalancingQueue);
            emit(p->inflightOutgoingSignal, getTotalAmountOutgoingInflight(it->first));
            emit(p->inflightIncomingSignal, getTotalAmountIncomingInflight(it->first));
            emit(p->serviceRateSignal, p->arrivalRate/p->serviceRate);
            emit(p->lambdaSignal, p->lambda);
            emit(p->muLocalSignal, p->muLocal);
        }
    }
    routerNodeBase::handleStatMessage(ttmsg);
}

/* main routine for handling a new transaction under the pricing scheme,
 * updates the nvalue and then calls the usual handler
 */
void routerNodePriceScheme::handleTransactionMessage(routerMsg* ttmsg){ 
    int hopcount = ttmsg->getHopCount();
    transactionMsg *transMsg = check_and_cast<transactionMsg *>(ttmsg->getEncapsulatedPacket());
    int nextNode = ttmsg->getRoute()[hopcount + 1];
    nodeToPaymentChannel[nextNode].nValue += transMsg->getAmount();
    routerNodeBase::handleTransactionMessage(ttmsg);
}


/* handler for the trigger message that regularly fires to indicate
 * that it is time to recompute prices for all payment channels 
 * and let your neighbors know about the latest rates of incoming 
 * transactions for every one of them and wait for them to send
 * you the same before recomputing prices
 */
void routerNodePriceScheme::handleTriggerPriceUpdateMessage(routerMsg* ttmsg) {
    // reschedule this to trigger again at intervals of _tUpdate
    if (simTime() > _simulationLength) {
        delete ttmsg;
    }
    else {
        scheduleAt(simTime()+_tUpdate, ttmsg);
    }

    // go through all the payment channels and recompute the arrival rate of 
    // transactions for all of them
    for ( auto it = nodeToPaymentChannel.begin(); it!= nodeToPaymentChannel.end(); it++ ) {
        PaymentChannel *neighborChannel = &(nodeToPaymentChannel[it->first]);      
        neighborChannel->xLocal = neighborChannel->nValue / _tUpdate;
        neighborChannel->updateRate = neighborChannel->numUpdateMessages/_tUpdate;
        if (it->first < 0){
            printNodeToPaymentChannel();
            endSimulation();
        }

        auto firstTransTimes = neighborChannel->serviceArrivalTimeStamps.front();
        auto lastTransTimes =  neighborChannel->serviceArrivalTimeStamps.back();
        double serviceTimeDiff = get<1>(lastTransTimes).dbl() - get<1>(firstTransTimes).dbl(); 
        double arrivalTimeDiff = get<1>(neighborChannel->arrivalTimeStamps.back()).dbl() - 
            get<1>(neighborChannel->arrivalTimeStamps.front()).dbl();

        // correction for really large service/arrival rate initially
        if (neighborChannel->serviceArrivalTimeStamps.size() < 0.3 * _serviceArrivalWindow)
            serviceTimeDiff = 1;
        if (neighborChannel->arrivalTimeStamps.size() < 0.3 * _serviceArrivalWindow)
            arrivalTimeDiff = 1;
        
        neighborChannel->serviceRate = neighborChannel->sumServiceWindowTxns / serviceTimeDiff;
        neighborChannel->arrivalRate = neighborChannel->sumArrivalWindowTxns / arrivalTimeDiff;
        neighborChannel->lastQueueSize = getTotalAmount(it->first);

        routerMsg * priceUpdateMsg = generatePriceUpdateMessage(neighborChannel->nValue, 
                neighborChannel->serviceRate, neighborChannel->arrivalRate, 
            neighborChannel->lastQueueSize, it->first);
        
        neighborChannel->lastNValue = neighborChannel->nValue;
        neighborChannel->nValue = 0;
        neighborChannel->numUpdateMessages = 0;
        forwardMessage(priceUpdateMsg);
    }
}

/* handler that handles the receipt of a priceUpdateMessage from a neighboring 
 * node that causes this node to update its prices for this particular
 * payment channel
 * Nesterov and secondOrderOptimization are two means to speed up the convergence
 */
void routerNodePriceScheme::handlePriceUpdateMessage(routerMsg* ttmsg){
    // compute everything to do with the neighbor
    priceUpdateMsg *puMsg = check_and_cast<priceUpdateMsg *>(ttmsg->getEncapsulatedPacket());
    double nRemote = puMsg->getNLocal();
    double serviceRateRemote = puMsg->getServiceRate();
    double arrivalRateRemote = puMsg->getArrivalRate();
    double qRemote = puMsg->getQueueSize();
    int sender = ttmsg->getRoute()[0];
    tuple<int, int> senderReceiverTuple = (sender < myIndex()) ? make_tuple(sender, myIndex()) :
                    make_tuple(myIndex(), sender);

    PaymentChannel *neighborChannel = &(nodeToPaymentChannel[sender]);
    double inflightRemote = min(getTotalAmountIncomingInflight(sender) + 
        serviceRateRemote * _avgDelay/1000, _capacities[senderReceiverTuple]); 

    // collect all local stats
    double xLocal = neighborChannel->xLocal;
    double updateRateLocal = neighborChannel->updateRate;
    double nLocal = neighborChannel->lastNValue;
    double inflightLocal = min(getTotalAmountOutgoingInflight(sender) + 
        updateRateLocal* _avgDelay/1000.0, _capacities[senderReceiverTuple]);
    double qLocal = neighborChannel->lastQueueSize;
    double serviceRateLocal = neighborChannel->serviceRate;
    double arrivalRateLocal = neighborChannel->arrivalRate;
 
    // collect this payment channel specific details
    tuple<int, int> node1node2Pair = (myIndex() < sender) ? make_tuple(myIndex(), sender) : make_tuple(sender,
            myIndex());
    double cValue = _capacities[node1node2Pair]; 
    double oldLambda = nodeToPaymentChannel[sender].lambda;
    double oldMuLocal = nodeToPaymentChannel[sender].muLocal;
    double oldMuRemote = nodeToPaymentChannel[sender].muRemote;
    
    // compute the new delta to the lambda
    double newLambdaGrad;
    if (_useQueueEquation) {
        newLambdaGrad = inflightLocal*arrivalRateLocal/serviceRateLocal + 
            inflightRemote * arrivalRateRemote/serviceRateRemote + 
            2*_xi*min(qLocal, qRemote) - (_capacityFactor * cValue);
    } else {
        newLambdaGrad = inflightLocal*arrivalRateLocal/serviceRateLocal + 
            inflightRemote * arrivalRateRemote/serviceRateRemote - (_capacityFactor * cValue); 
    }
        
    // compute the new delta to mulocal
    double newMuLocalGrad;
    if (_useQueueEquation) {
        newMuLocalGrad = nLocal - nRemote + qLocal*_tUpdate/_routerQueueDrainTime - 
                        qRemote*_tUpdate/_routerQueueDrainTime;
    } else {
        newMuLocalGrad = nLocal - nRemote;
    }
    
    // use all parts to compute the new mu/lambda taking a step in its direction
    double newLambda = 0.0;
    double newMuLocal = 0.0;
    double newMuRemote = 0.0;
    double myKappa = _kappa /**20.0 /cValue*/;
    double myEta = _eta /** 20.0 / cValue*/;
    
    newLambda = oldLambda +  myEta*newLambdaGrad;
    newMuLocal = oldMuLocal + myKappa*newMuLocalGrad;
    newMuRemote = oldMuRemote - myKappa*newMuLocalGrad; 

    // make all price variable non-negative
    nodeToPaymentChannel[sender].lambda = max(newLambda, 0.0);
    nodeToPaymentChannel[sender].muLocal = max(newMuLocal, 0.0);
    nodeToPaymentChannel[sender].muRemote = max(newMuRemote, 0.0);

    //delete both messages
    ttmsg->decapsulate();
    delete puMsg;
    delete ttmsg;
}


/* handler for a price query message which adds their prices to the
 * probe and then sends it forward to the next node
 */
void routerNodePriceScheme::handlePriceQueryMessage(routerMsg* ttmsg){
    priceQueryMsg *pqMsg = check_and_cast<priceQueryMsg *>(ttmsg->getEncapsulatedPacket());
    bool isReversed = pqMsg->getIsReversed();
    
    if (!isReversed) { 
        int nextNode = ttmsg->getRoute()[ttmsg->getHopCount()+1];
        double zOld = pqMsg->getZValue();
        double lambda = nodeToPaymentChannel[nextNode].lambda;
        double muLocal = nodeToPaymentChannel[nextNode].muLocal;
        double muRemote = nodeToPaymentChannel[nextNode].muRemote;
        double zNew = zOld;

        if (ttmsg->getHopCount() < ttmsg->getRoute().size() - 2) {
            // ignore end host links
            zNew += (2 * lambda) + muLocal  - muRemote;
        }
        pqMsg->setZValue(zNew);
        forwardMessage(ttmsg);
    }
    else {
        forwardMessage(ttmsg);
    }
}
