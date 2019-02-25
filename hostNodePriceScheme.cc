#include "hostNodePriceScheme.h"

// global parameters
double _tokenBucketCapacity = 1.0;
bool _reschedulingEnabled; // whether timers can be rescheduled
bool _nesterov;
bool _secondOrderOptimization;

// parameters for price scheme
double _eta; //for price computation
double _kappa; //for price computation
double _tUpdate; //for triggering price updates at routers
double _tQuery; //for triggering price query probes at hosts
double _alpha;
double _gamma;
double _zeta;
double _delta;
double _avgDelay;
double _minPriceRate;
double _rhoLambda;
double _rhoMu;
double _rho;
double _minWindow;

Define_Module(hostNodePriceScheme);

/* generate the trigger message to initiate price Updates periodically
 */
routerMsg *hostNodePriceScheme::generateTriggerPriceUpdateMessage(){
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
routerMsg * hostNodePriceScheme::generatePriceUpdateMessage(double xLocal, int receiver){
    char msgname[MSGSIZE];
    sprintf(msgname, "tic-%d-to-%d priceUpdateMsg", myIndex(), receiver);
    routerMsg *rMsg = new routerMsg(msgname);
    vector<int> route={myIndex(),receiver};
   
    rMsg->setRoute(route);
    rMsg->setHopCount(0);
    rMsg->setMessageType(PRICE_UPDATE_MSG);
    
    priceUpdateMsg *puMsg = new priceUpdateMsg(msgname);
    puMsg->setXLocal(xLocal);
    rMsg->encapsulate(puMsg);
    return rMsg;
}


/* generate the trigger message to initiate price queries periodically 
 */
routerMsg *hostNodePriceScheme::generateTriggerPriceQueryMessage(){
    char msgname[MSGSIZE];
    sprintf(msgname, "node-%d triggerPriceQueryMsg", myIndex());
    routerMsg *rMsg = new routerMsg(msgname);
    rMsg->setMessageType(TRIGGER_PRICE_QUERY_MSG);
    return rMsg;
}

/* create a price query message to be sent on a particular path identified
 * by the list of hops and the index for that path amongst other paths to 
 * that destination
 */
routerMsg * hostNodePriceScheme::generatePriceQueryMessage(vector<int> path, int pathIdx){
    int destNode = path[path.size()-1];
    char msgname[MSGSIZE];
    sprintf(msgname, "tic-%d-to-%d priceQueryMsg [idx %d]", myIndex(), destNode, pathIdx);
    priceQueryMsg *pqMsg = new priceQueryMsg(msgname);
    pqMsg->setPathIndex(pathIdx);
    pqMsg->setIsReversed(false);
    pqMsg->setZValue(0);

    sprintf(msgname, "tic-%d-to-%d router-priceQueryMsg [idx %d]", myIndex(), destNode, pathIdx);
    routerMsg *rMsg = new routerMsg(msgname);
    rMsg->setRoute(path);

    rMsg->setHopCount(0);
    rMsg->setMessageType(PRICE_QUERY_MSG);
    
    rMsg->encapsulate(pqMsg);
    return rMsg;
}


/* creates a message that is meant to signal the time when a transaction can
 * be sent on the path in context based on the current view of the rate
 */
routerMsg *hostNodePriceScheme::generateTriggerTransactionSendMessage(vector<int> path, 
        int pathIndex, int destNode) {
    char msgname[MSGSIZE];
    sprintf(msgname, "tic-%d-to-%d transactionSendMsg", myIndex(), destNode);
    transactionSendMsg *tsMsg = new transactionSendMsg(msgname);
    tsMsg->setPathIndex(pathIndex);
    tsMsg->setTransactionPath(path);
    tsMsg->setReceiver(destNode);

    routerMsg *rMsg = new routerMsg(msgname);
    rMsg->setMessageType(TRIGGER_TRANSACTION_SEND_MSG);
    rMsg->encapsulate(tsMsg);
    return rMsg;
}






/* check if all the provided rates are non-negative and also verify
 *  that their sum is less than the demand, return false otherwise
 */
bool hostNodePriceScheme::ratesFeasible(vector<PathRateTuple> actualRates, double demand) {
    double sumRates = 0;
    for (auto a : actualRates) {
        double rate = get<1>(a);
        if (rate < (0 - _epsilon))
            return false;
        sumRates += rate;
    }
    return (sumRates <= (demand + _epsilon));
}

/* computes the projection of the given recommended rates onto the demand d_ij per source
 */
vector<PathRateTuple> hostNodePriceScheme::computeProjection(
        vector<PathRateTuple> recommendedRates, double demand) {
    auto compareRates = [](PathRateTuple rate1, PathRateTuple rate2) {
            return (get<1>(rate1) < get<1>(rate2));
    };

    auto nuFeasible = [](double nu, double nuLeft, double nuRight) {
            return (nu >= -1 * _epsilon && nu >= (nuLeft - _epsilon) && 
                    nu <= (nuRight + _epsilon));
    };

    sort(recommendedRates.begin(), recommendedRates.end(), compareRates);
    double nu = 0.0;
    vector<PathRateTuple> actualRates;

    // initialize all rates to zero
    vector<PathRateTuple> zeroRates;
    for (int i = 0; i < recommendedRates.size(); i++) {
        zeroRates.push_back(make_tuple(i, 0.0));
    }

    // if everything is negative (aka the largest is negative), just return 0s
    if (get<1>(recommendedRates[recommendedRates.size() - 1]) < 0 + _epsilon){
        return zeroRates;
    }

    // consider nu  = 0 and see if everything checks out
    actualRates = zeroRates;
    int firstPositiveIndex = -1;
    int i = 0;
    for (auto p : recommendedRates) {
        double rate = get<1>(p);
        int pathIndex = get<0>(p);
        if (rate > 0) {
            if (firstPositiveIndex == -1) 
                firstPositiveIndex = i;
            actualRates[pathIndex] = make_tuple(pathIndex, rate);
        }
        i++;
    }
    if (ratesFeasible(actualRates, demand))
        return actualRates;

    // atleast something should be positive if you got this far
    assert(firstPositiveIndex >= 0);


    // now go through all intervals between adjacent 2* recommended rates and see if any of them
    // can give you a valid assignment of actual rates and nu
    i = firstPositiveIndex; 
    double nuLeft = 0;
    double nuRight = 0;
    while (i < recommendedRates.size()) {
        // start over checking with a new nu interval
        actualRates = zeroRates;
        nuLeft = nuRight; 
        nuRight = 2*get<1>(recommendedRates[i]);

        // find sum of all elements that are to the right of nuRight
        double sumOfRightElements = 0.0;
        for (int j = i; j < recommendedRates.size(); j++)
            sumOfRightElements += get<1>(recommendedRates[j]);     
        nu = (sumOfRightElements - demand) * 2.0/(recommendedRates.size() - i);

        // given this nu, compute the actual rates for those elements to the right of nuRight
        for (auto p : recommendedRates) {
            double rate = get<1>(p);
            int pathIndex = get<0>(p);
            if (2*rate > nuLeft) {
                actualRates[pathIndex] = make_tuple(pathIndex, rate - nu/2.0);
            }
        }

        // check if these rates are feasible and nu actually falls in the right interval 
        if (ratesFeasible(actualRates, demand) && nuFeasible(nu, nuLeft, nuRight)) 
            return actualRates;

        // otherwise move on
        i++;
    }

    // should never be reached
    assert(false);
    return zeroRates;
}


/* overall controller for handling messages that dispatches the right function
 * based on message type in price Scheme
 */
void hostNodePriceScheme::handleMessage(cMessage *msg) {
    routerMsg *ttmsg = check_and_cast<routerMsg *>(msg);
 
    //Radhika TODO: figure out what's happening here
    if (simTime() > _simulationLength){
        auto encapMsg = (ttmsg->getEncapsulatedPacket());
        ttmsg->decapsulate();
        delete ttmsg;
        delete encapMsg;
        return;
    } 

    switch(ttmsg->getMessageType()) {
        case TRIGGER_PRICE_UPDATE_MSG:
             if (_loggingEnabled) cout<< "[HOST "<< myIndex() 
                 <<": RECEIVED TRIGGER_PRICE_UPDATE MSG] "<< ttmsg->getName() << endl;
             handleTriggerPriceUpdateMessage(ttmsg);
             if (_loggingEnabled) cout<< "[AFTER HANDLING:]  "<< endl;
             break;

        case PRICE_UPDATE_MSG:
             if (_loggingEnabled) cout<< "[HOST "<< myIndex() 
                 <<": RECEIVED PRICE_UPDATE MSG] "<< ttmsg->getName() << endl;
             handlePriceUpdateMessage(ttmsg);
             if (_loggingEnabled) cout<< "[AFTER HANDLING:]  "<< endl;
             break;

        case TRIGGER_PRICE_QUERY_MSG:
             if (_loggingEnabled) cout<< "[HOST "<< myIndex() 
                 <<": RECEIVED TRIGGER_PRICE_QUERY MSG] "<< ttmsg->getName() << endl;
             handleTriggerPriceQueryMessage(ttmsg);
             if (_loggingEnabled) cout<< "[AFTER HANDLING:]  "<< endl;
             break;
        
        case PRICE_QUERY_MSG:
             if (_loggingEnabled) cout<< "[HOST "<< myIndex() 
                 <<": RECEIVED PRICE_QUERY MSG] "<< ttmsg->getName() << endl;
             handlePriceQueryMessage(ttmsg);
             if (_loggingEnabled) cout<< "[AFTER HANDLING:]  "<< endl;
             break;

        case TRIGGER_TRANSACTION_SEND_MSG:
             if (_loggingEnabled) cout<< "[HOST "<< myIndex() 
                 <<": RECEIVED TRIGGER_TXN_SEND MSG] "<< ttmsg->getName() << endl;
             handleTriggerTransactionSendMessage(ttmsg);
             if (_loggingEnabled) cout<< "[AFTER HANDLING:]  "<< endl;
             break;

        default:
             hostNodeBase::handleMessage(msg);

    }
}

/* main routine for handling a new transaction under the pricing scheme
 * In particular, initiate price probes, assigns a txn to a path if the 
 * rate for that path allows it, otherwise queues it at the sender
 * until a timer fires on the next path allowing a txn to go through
 */
void hostNodePriceScheme::handleTransactionMessageSpecialized(routerMsg* ttmsg){
    transactionMsg *transMsg = check_and_cast<transactionMsg *>(ttmsg->getEncapsulatedPacket());
    int hopcount = ttmsg->getHopCount();
    int destNode = transMsg->getReceiver();
    int nextNode = ttmsg->getRoute()[hopcount + 1];
    int transactionId = transMsg->getTransactionId();
    
    // first time seeing this transaction so add to d_ij computation
    // count the txn for accounting also
    if (simTime() == transMsg->getTimeSent()) {
        destNodeToNumTransPending[destNode]  += 1;
        nodeToDestInfo[destNode].transSinceLastInterval += 1;
        statNumArrived[destNode] += 1; 
        statRateArrived[destNode] += 1; 
    }

    // initiate price probes if it is a new destination
    if (nodeToShortestPathsMap.count(destNode) == 0 && ttmsg->isSelfMessage()){
        vector<vector<int>> kShortestRoutes = getKShortestRoutes(transMsg->getSender(),destNode, _kValue);
        initializePriceProbes(kShortestRoutes, destNode);
        //TODO: change to a queue implementation
        scheduleAt(simTime() + _maxTravelTime, ttmsg);
        return;
    }
    
    // at destination, add to incoming transUnits and trigger ack
    if (transMsg->getReceiver() == myIndex()) {
        int prevNode = ttmsg->getRoute()[ttmsg->getHopCount() - 1];
        map<Id, double> *incomingTransUnits = &(nodeToPaymentChannel[prevNode].incomingTransUnits);
        (*incomingTransUnits)[make_tuple(transMsg->getTransactionId(), transMsg->getHtlcIndex())] =
            transMsg->getAmount();
        
        if (_timeoutEnabled) {
            auto iter = find_if(canceledTransactions.begin(),
               canceledTransactions.end(),
               [&transactionId](const tuple<int, simtime_t, int, int, int>& p)
               { return get<0>(p) == transactionId; });

            if (iter!=canceledTransactions.end()){
                canceledTransactions.erase(iter);
            }
        }
        routerMsg* newMsg =  generateAckMessage(ttmsg);
        forwardMessage(newMsg);
        return;
    }
    else if (ttmsg->isSelfMessage()) {
        // at sender, either queue up or send on a path that allows you to send
        DestInfo* destInfo = &(nodeToDestInfo[destNode]);
       
        //send on a path if no txns queued up and timer was in the path
        if ((destInfo->transWaitingToBeSent).size() > 0) {
            destInfo->transWaitingToBeSent.push_back(ttmsg);
        } else {
            for (auto p: nodeToShortestPathsMap[destNode]) {
                int pathIndex = p.first;
                PathInfo *pathInfo = &(nodeToShortestPathsMap[destNode][pathIndex]);
                double window = max(pathInfo->rateToSendTrans * pathInfo->rttMin, _minWindow);
                
                if (pathInfo->rateToSendTrans > 0 && simTime() > pathInfo->timeToNextSend &&
                        pathInfo->sumOfTransUnitsInFlight < window) {
                    ttmsg->setRoute(pathInfo->path);
                    int nextNode = pathInfo->path[1];
                    handleTransactionMessage(ttmsg, true /*revisit*/);

                    // record stats on sent units for payment channel, destination and path
                    nodeToPaymentChannel[nextNode].nValue += 1;
                    statRateAttempted[destNode] += 1;
                    pathInfo->nValue += 1;
                    pathInfo->statRateAttempted += 1;
                    pathInfo->sumOfTransUnitsInFlight += transMsg->getAmount();
                    pathInfo->lastTransSize = transMsg->getAmount();
                    pathInfo->lastSendTime = simTime();
                    pathInfo->amtAllowedToSend = max(pathInfo->amtAllowedToSend - transMsg->getAmount(), 0.0);
                
                    // update the "time when the next transaction can be sent"
                    double rateToUse = max(pathInfo->rateToSendTrans, _epsilon);
                    simtime_t newTimeToNextSend =  simTime() + max(transMsg->getAmount()/rateToUse, _epsilon);
                    pathInfo->timeToNextSend = newTimeToNextSend;
                    
                    //generate time out message here, when path is decided
                    if (_timeoutEnabled) {
                        routerMsg *toutMsg = generateTimeOutMessage(ttmsg);
                        scheduleAt(simTime() + transMsg->getTimeOut(), toutMsg );
                    }
                    
                    if (_signalsEnabled) emit(pathPerTransPerDestSignals[destNode], pathIndex);
                    return;
                }
            }
            
            //transaction cannot be sent on any of the paths, queue transaction
            destInfo->transWaitingToBeSent.push_back(ttmsg);
            for (auto p: nodeToShortestPathsMap[destNode]) {
                PathInfo *pInfo = &(nodeToShortestPathsMap[destNode][p.first]);
                if (pInfo->isSendTimerSet == false) {
                    simtime_t timeToNextSend = pInfo->timeToNextSend;
                    if (simTime() > timeToNextSend) {
                        timeToNextSend = simTime() + _epsilon;
                    }               
                    scheduleAt(timeToNextSend, pInfo->triggerTransSendMsg);
                    pInfo->isSendTimerSet = true;
                }
            }
        }
    }
}


/* handler for the statistic message triggered every x seconds to also
 * output the price based scheme stats in addition to the default
 */
void hostNodePriceScheme::handleStatMessage(routerMsg* ttmsg){
    if (_signalsEnabled) {
        // per payment channel stats
        for ( auto it = nodeToPaymentChannel.begin(); it!= nodeToPaymentChannel.end(); it++){ 
            int node = it->first; //key
            PaymentChannel* p = &(nodeToPaymentChannel[node]);

            //statistics for price scheme per payment channel
            simsignal_t nValueSignal;
            simsignal_t xLocalSignal;
            simsignal_t lambdaSignal;
            simsignal_t muLocalSignal;
            simsignal_t muRemoteSignal;

            emit(p->nValueSignal, p->nValue);
            emit(p->xLocalSignal, p->xLocal);
            emit(p->lambdaSignal, p->lambda);
            emit(p->muLocalSignal, p->muLocal);
            emit(p->muRemoteSignal, p->muRemote);
        }
        
        // per destination statistics
        for (auto it = 0; it < _numHostNodes; it++){ 
            if (it != getIndex()) {
                if (nodeToShortestPathsMap.count(it) > 0) {
                    for (auto p: nodeToShortestPathsMap[it]){
                        int pathIndex = p.first;
                        PathInfo *pInfo = &(p.second);

                        //signals for price scheme per path
                        emit(pInfo->rateToSendTransSignal, pInfo->rateToSendTrans);
                        emit(pInfo->rateActuallySentSignal, pInfo->rateSentOnPath);
                        emit(pInfo->timeToNextSendSignal, pInfo->timeToNextSend);
                        emit(pInfo->sumOfTransUnitsInFlightSignal, 
                                pInfo->sumOfTransUnitsInFlight);
                        emit(pInfo->priceLastSeenSignal, pInfo->priceLastSeen);
                        emit(pInfo->isSendTimerSetSignal, pInfo->isSendTimerSet);
                    }
                }
               emit(numTimedOutAtSenderSignals[it], statNumTimedOutAtSender[it]);
               emit(demandEstimatePerDestSignals[it], nodeToDestInfo[it].demand);
               emit(numWaitingPerDestSignals[it], 
                       nodeToDestInfo[it].transWaitingToBeSent.size()); 
            }        
        }
    } 

    // call the base method to output rest of the stats
    hostNodeBase::handleStatMessage(ttmsg);
}


/* specialized ack handler that does the routine if this is a price scheme 
 * algorithm. In particular, collects/updates stats for this path alone
 * NOTE: acks are on the reverse path relative to the original sender
 */
void hostNodePriceScheme::handleAckMessageSpecialized(routerMsg* ttmsg){
    ackMsg *aMsg = check_and_cast<ackMsg*>(ttmsg->getEncapsulatedPacket());
    int pathIndex = aMsg->getPathIndex();
    int destNode = ttmsg->getRoute()[0]; 
    
    if (aMsg->getIsSuccess()==false){
        statNumFailed[destNode] += 1;
        statRateFailed[destNode] += 1;
    }
    else {
        statNumCompleted[destNode] += 1;
        statRateCompleted[destNode] += 1;
        nodeToShortestPathsMap[destNode][pathIndex].statRateCompleted += 1;
    }
    
    nodeToShortestPathsMap[destNode][pathIndex].sumOfTransUnitsInFlight -= aMsg->getAmount();
    destNodeToNumTransPending[destNode] -= 1;     
    hostNodeBase::handleAckMessage(ttmsg);
}

/* handler for the clear state message that deals with
 * transactions that will no longer be completed
 * TODO: add any more specific cleanups 
 */
void hostNodePriceScheme::handleClearStateMessage(routerMsg *ttmsg) {
    // works fine now because timeouts start per transaction only when
    // sent out and no txn splitting
    hostNodeBase::handleClearStateMessage(ttmsg);
}




/* handler for the trigger message that regularly fires to indicate
 * that it is time to recompute prices for all payment channels 
 * and let your neighbors know about the latest rates of incoming 
 * transactions for every one of them and wait for them to send
 * you the same before recomputing prices
 */
void hostNodePriceScheme::handleTriggerPriceUpdateMessage(routerMsg* ttmsg) {
    // reschedule this to trigger again at intervals of _tUpdate
    if (simTime() > _simulationLength) {
        delete ttmsg;
    }
    else {
        scheduleAt(simTime()+_tUpdate, ttmsg);
    }

    // go through all the payment channels and recompute the arrival rate of 
    // transactions for all of them
    for ( auto it = nodeToPaymentChannel.begin(); it!= nodeToPaymentChannel.end(); it++) {
        PaymentChannel *neighborChannel = &(nodeToPaymentChannel[it->first]);      
        neighborChannel->xLocal = neighborChannel->nValue / _tUpdate;
        neighborChannel->nValue = 0;
        
        if (it->first < 0){
            printNodeToPaymentChannel();
            endSimulation();
        }
        
        routerMsg * priceUpdateMsg = generatePriceUpdateMessage(neighborChannel->xLocal, it->first);
        forwardMessage(priceUpdateMsg);
    }
    
    // also update the rate actually Sent for a given destination and path 
    // to be used for your next rate computation
    for ( auto it = nodeToShortestPathsMap.begin(); it != nodeToShortestPathsMap.end(); it++ ) {
        int destNode = it->first;
        for (auto p : nodeToShortestPathsMap[destNode]) {
            PathInfo *pInfo = &(nodeToShortestPathsMap[destNode][p.first]); 
            double latestRate = pInfo->nValue / _tUpdate;
            double oldRate = pInfo->rateSentOnPath; 
            
            pInfo->nValue = 0; 
            pInfo->rateSentOnPath = (1 - _gamma) *oldRate + _gamma * latestRate; 
        }

        // use this information to also update your demand for this destination
        DestInfo* destInfo = &(nodeToDestInfo[destNode]);
        double newDemand = destInfo->transSinceLastInterval/_tUpdate;
        destInfo->demand = (1 - _zeta) * destInfo->demand + _zeta * newDemand;
        destInfo->transSinceLastInterval = 0;
    }
}


/* handler that handles the receipt of a priceUpdateMessage from a neighboring 
 * node that causes this node to update its prices for this particular
 * payment channel
 * Nesterov and secondOrderOptimization are two means to speed up the convergence
 */
void hostNodePriceScheme::handlePriceUpdateMessage(routerMsg* ttmsg){
    priceUpdateMsg *puMsg = check_and_cast<priceUpdateMsg *>(ttmsg->getEncapsulatedPacket());
    double xRemote = puMsg->getXLocal();
    int sender = ttmsg->getRoute()[0];
    PaymentChannel *neighborChannel = &(nodeToPaymentChannel[sender]);

    //Update $\lambda$, $mu_local$ and $mu_remote$
    double xLocal = neighborChannel->xLocal;
    double cValue = neighborChannel->totalCapacity;
    double oldLambda = neighborChannel->lambda;
    double oldMuLocal = neighborChannel->muLocal;
    double oldMuRemote = neighborChannel->muRemote;

     // Nesterov's gradient descent equation
     // and other speeding up mechanisms
     double newLambda = 0.0;
     double newMuLocal = 0.0;
     double newMuRemote = 0.0;
     if (_nesterov) {
         double yLambda = neighborChannel->yLambda;
         double yMuLocal = neighborChannel->yMuLocal;
         double yMuRemote = neighborChannel->yMuRemote;

         double yLambdaNew = oldLambda + _eta*(xLocal + xRemote - (cValue/_delta));
         newLambda = yLambdaNew + _rhoLambda*(yLambdaNew - yLambda); 
         neighborChannel->yLambda = yLambdaNew;

         double yMuLocalNew = oldMuLocal + _kappa*(xLocal - xRemote);
         newMuLocal = yMuLocalNew + _rhoMu*(yMuLocalNew - yMuLocal);
         neighborChannel->yMuLocal = yMuLocalNew;

         double yMuRemoteNew = oldMuRemote + _kappa*(xRemote - xLocal);
         newMuRemote = yMuRemoteNew + _rhoMu*(yMuRemoteNew - yMuRemote);
         neighborChannel->yMuRemote = yMuRemoteNew;
     } 
     else if (_secondOrderOptimization) {
         double lastLambdaGrad = neighborChannel->lastLambdaGrad;
         double newLambdaGrad = xLocal + xRemote - (cValue/_delta);
         newLambda = oldLambda +  _eta*newLambdaGrad + _rhoLambda*(newLambdaGrad - lastLambdaGrad);
         neighborChannel->lastLambdaGrad = newLambdaGrad;

         double lastMuLocalGrad = neighborChannel->lastMuLocalGrad;
         double newMuLocalGrad = xLocal - xRemote;
         newMuLocal = oldMuLocal + _kappa*newMuLocalGrad + 
             _rhoMu*(newMuLocalGrad - lastMuLocalGrad);
         newMuRemote = oldMuRemote - _kappa*newMuLocalGrad - 
             _rhoMu*(newMuLocalGrad - lastMuLocalGrad);
         neighborChannel->lastMuLocalGrad = newMuLocalGrad;
     } 
     else {
         newLambda = oldLambda +  _eta*(xLocal + xRemote - (cValue/_delta));
         newMuLocal = oldMuLocal + _kappa*(xLocal - xRemote);
         newMuRemote = oldMuRemote + _kappa*(xRemote - xLocal); 
     }
     
     neighborChannel->lambda = maxDouble(newLambda, 0);
     neighborChannel->muLocal = maxDouble(newMuLocal, 0);
     neighborChannel->muRemote = maxDouble(newMuRemote, 0);
     
     //delete both messages
     ttmsg->decapsulate();
     delete puMsg;
     delete ttmsg;
}



/* handler for the trigger message that periodically fires indicating
 * that it is time to send a new price query on this path
 */
void hostNodePriceScheme::handleTriggerPriceQueryMessage(routerMsg* ttmsg){
    // reschedule this to trigger again at intervals of _tQuery
    if (simTime() > _simulationLength){
        delete ttmsg;
    } else {
        scheduleAt(simTime()+_tQuery, ttmsg);
    }
    
    // go through all destinations that have pending transactions and send 
    // out a new probe to them
    for (auto it = destNodeToNumTransPending.begin(); it!=destNodeToNumTransPending.end(); it++){
        if (it->first == myIndex()){
            // TODO: shouldn't be happening         
            continue;
        }
        
        if (it->second>0){ 
            //if we have transactions pending
            for (auto p = nodeToShortestPathsMap[it->first].begin() ;
                    p!= nodeToShortestPathsMap[it->first].end(); p++){
                // p is per path in the destNode
                int routeIndex = p->first;
                PathInfo pInfo = nodeToShortestPathsMap[it->first][p->first];
                routerMsg * msg = generatePriceQueryMessage(pInfo.path, routeIndex);
                handlePriceQueryMessage(msg);
            }
        }
    }
}

/* handler for a price query message which when received back at the sender
 * is responsible for adjusting the sending rate on the associated paths
 * at all other points, it just forwards the messgae
 */
void hostNodePriceScheme::handlePriceQueryMessage(routerMsg* ttmsg){
    priceQueryMsg *pqMsg = check_and_cast<priceQueryMsg *>(ttmsg->getEncapsulatedPacket());
    
    bool isReversed = pqMsg->getIsReversed();
    if (!isReversed && ttmsg->getHopCount() == 0){ 
        // at the sender, compute my own price and send
        int nextNode = ttmsg->getRoute()[ttmsg->getHopCount()+1];
        double zOld = pqMsg->getZValue();
        double lambda = nodeToPaymentChannel[nextNode].lambda;
        double muLocal = nodeToPaymentChannel[nextNode].muLocal;
        double muRemote = nodeToPaymentChannel[nextNode].muRemote;
        double zNew =  zOld + (2 * lambda) + muLocal  - muRemote;
        pqMsg->setZValue(zNew);
        forwardMessage(ttmsg);
    }
    else if (!isReversed){ 
        //is at destination
        pqMsg->setIsReversed(true);
        vector<int> route = ttmsg->getRoute();
        reverse(route.begin(), route.end());
        ttmsg->setRoute(route);
        ttmsg->setHopCount(0);
        forwardMessage(ttmsg);
    }
    else { 
        //is back at sender with the price
        double demand;
        double zValue = pqMsg->getZValue();
        int destNode = ttmsg->getRoute()[0];
        int routeIndex = pqMsg->getPathIndex();

        PathInfo *p = &(nodeToShortestPathsMap[destNode][routeIndex]);
        double oldRate = p->rateSentOnPath;

        // Nesterov's gradient descent equation
        // and other speeding up mechanisms
        double preProjectionRate = 0.0;
        if (_nesterov) {
          double yPrev = p->yRateToSendTrans;
          double yNew = oldRate + _alpha*(1 - zValue);
          p->yRateToSendTrans = yNew;
          preProjectionRate = yNew + _rho*(yNew - yPrev);
        } else {
          preProjectionRate  = oldRate + _alpha*(1-zValue);
        }

        p->priceLastSeen = zValue;
        p->rateToSendTrans = maxDouble(preProjectionRate, _minPriceRate);
        updateTimers(destNode, routeIndex, p->rateToSendTrans);

        /* compute the projection of this new rate along with old rates
        vector<PathRateTuple> pathRateTuples;
        for (auto p : nodeToShortestPathsMap[destNode]) {
            int pathIndex = p.first;
            double rate;
            if (pathIndex != routeIndex) {
                rate =  p.second.rateToSendTrans;
            } else {
                rate = preProjectionRate;
            }
              
            PathRateTuple newTuple = make_tuple(pathIndex, rate);
            pathRateTuples.push_back(newTuple);
        }
        vector<PathRateTuple> projectedRates = pathRateTuples;
            //computeProjection(pathRateTuples, nodeToDestInfo[destNode].demand);

        // reassign all path's rates to the projected rates and 
        // make sure it is atleast minPriceRate for every path
        for (auto p : projectedRates) {
            int pathIndex = get<0>(p);
            double newRate = get<1>(p);

            if (_reschedulingEnabled) {
                updateTimers(destNode, pathIndex, newRate);
            }
            else
                nodeToShortestPathsMap[destNode][pathIndex].rateToSendTrans = 
                maxDouble(newRate, _minPriceRate);
        }*/
              
        //delete both messages
        ttmsg->decapsulate();
        delete pqMsg;
        delete ttmsg;
   }
}

/* handles the sending of transactions at a certain rate indirectly via timers 
 * going off to trigger the next send on a particular path. This responds to
 * that trigger and pulls the next transaction off the queue and sends that
 */
void hostNodePriceScheme::handleTriggerTransactionSendMessage(routerMsg* ttmsg){
    transactionSendMsg *tsMsg = 
        check_and_cast<transactionSendMsg *>(ttmsg->getEncapsulatedPacket());

    vector<int> path = tsMsg->getTransactionPath();
    int pathIndex = tsMsg->getPathIndex();
    int destNode = tsMsg->getReceiver();
    PathInfo* p = &(nodeToShortestPathsMap[destNode][pathIndex]);
    double window = max(p->rateToSendTrans * p->rttMin, _minWindow);

    if (nodeToDestInfo[destNode].transWaitingToBeSent.size() > 0 && 
            p->sumOfTransUnitsInFlight < window){
        //remove the transaction $tu$ at the head of the queue
        routerMsg *msgToSend = nodeToDestInfo[destNode].transWaitingToBeSent.front();
        nodeToDestInfo[destNode].transWaitingToBeSent.pop_front();
        transactionMsg *transMsg = 
           check_and_cast<transactionMsg *>(msgToSend->getEncapsulatedPacket());
        
        //Send the transaction $tu$ along the corresponding path.
        transMsg->setPathIndex(pathIndex);
        msgToSend->setRoute(path);
        msgToSend->setHopCount(0);

        // increment the number of units sent on a particular payment channel
        int nextNode = path[1];
        nodeToPaymentChannel[nextNode].nValue = nodeToPaymentChannel[nextNode].nValue + 1;

        if (_signalsEnabled) emit(pathPerTransPerDestSignals[destNode], pathIndex);

        // increment amount in inflght and other info on last transaction on this path
        p->nValue += 1;
        p->sumOfTransUnitsInFlight = p->sumOfTransUnitsInFlight + transMsg->getAmount();
        p->lastTransSize = transMsg->getAmount();
        p->lastSendTime = simTime();
        p->amtAllowedToSend = max(p->amtAllowedToSend - transMsg->getAmount(), 0.0);
        
        //generate time out message here, when path is decided
        if (_timeoutEnabled) {
          routerMsg *toutMsg = generateTimeOutMessage(msgToSend);
          scheduleAt(simTime() + transMsg->getTimeOut(), toutMsg );
        }

        // cannot be cancelled at this point
        handleTransactionMessage(msgToSend, 1/*revisiting*/);

        // update the number attempted to this destination and on this path
        p->statRateAttempted = p->statRateAttempted + 1;
        statRateAttempted[destNode] += 1;

        //Update the  “time when next transaction can be sent” 
        double bound = _reschedulingEnabled ? _epsilon : 1.0;
        double rateToSendTrans = max(p->rateToSendTrans, bound);
        p->timeToNextSend = simTime() + max(transMsg->getAmount()/rateToSendTrans, bound);
        
        //If there are more transactions queued up, reschedule timer
        //if (nodeToDestInfo[destNode].transWaitingToBeSent.size() > 0){
            scheduleAt(p->timeToNextSend, ttmsg);
        /*} else {
            p->isSendTimerSet = false;
        }*/
    }
    else{ 
        //no trans to send
        // don't turn off timers
        PathInfo* p = &(nodeToShortestPathsMap[destNode][pathIndex]);
        double rateToSendTrans = p->rateToSendTrans;
        double lastTxnSize = p->lastTransSize;
        p->timeToNextSend = simTime() +
                max(lastTxnSize/rateToSendTrans, _epsilon);
        scheduleAt(p->timeToNextSend, ttmsg);
        p->amtAllowedToSend = 0.0;
          
        //nodeToShortestPathsMap[destNode][pathIndex].isSendTimerSet = false;
    }
}

/* initialize data for for the paths supplied to the destination node
 * and also fix the paths for susbequent transactions to this destination
 * and register signals that are path specific
 */
void hostNodePriceScheme::initializePriceProbes(vector<vector<int>> kShortestPaths, int destNode){
    for (int pathIdx = 0; pathIdx < kShortestPaths.size(); pathIdx++){
        // initialize pathInfo
        PathInfo temp = {};
        temp.path = kShortestPaths[pathIdx];
        routerMsg * triggerTransSendMsg = 
          generateTriggerTransactionSendMessage(kShortestPaths[pathIdx], pathIdx, destNode);
        temp.triggerTransSendMsg = triggerTransSendMsg;
        temp.rateToSendTrans = _minPriceRate;
        temp.yRateToSendTrans = _minPriceRate;
        // TODO: change this to something sensible
        temp.rttMin = kShortestPaths[pathIdx].size() * _delta;
        nodeToShortestPathsMap[destNode][pathIdx] = temp;

        //initialize signals
        simsignal_t signal;
        signal = registerSignalPerDestPath("rateAttempted", pathIdx, destNode);
        nodeToShortestPathsMap[destNode][pathIdx].rateAttemptedPerDestPerPathSignal = signal;

        signal = registerSignalPerDestPath("rateAttempted", pathIdx, destNode);
        nodeToShortestPathsMap[destNode][pathIdx].rateCompletedPerDestPerPathSignal = signal;

        signal = registerSignalPerDestPath("rateToSendTrans", pathIdx, destNode);
        nodeToShortestPathsMap[destNode][pathIdx].rateToSendTransSignal = signal;

        signal = registerSignalPerDestPath("rateSent", pathIdx, destNode);
        nodeToShortestPathsMap[destNode][pathIdx].rateActuallySentSignal = signal;
        
        signal = registerSignalPerDestPath("timeToNextSend", pathIdx, destNode);
        nodeToShortestPathsMap[destNode][pathIdx].timeToNextSendSignal = signal;

        signal = registerSignalPerDestPath("sumOfTransUnitsInFlight", pathIdx, destNode);
        nodeToShortestPathsMap[destNode][pathIdx].sumOfTransUnitsInFlightSignal = signal;
      
        signal = registerSignalPerDestPath("priceLastSeen", pathIdx, destNode);
        nodeToShortestPathsMap[destNode][pathIdx].priceLastSeenSignal = signal;

        signal = registerSignalPerDestPath("isSendTimerSet", pathIdx, destNode);
        nodeToShortestPathsMap[destNode][pathIdx].isSendTimerSetSignal = signal;
   }
}



/* helper method to reschedule the timer on a given path according to the new rate
 */
void hostNodePriceScheme::updateTimers(int destNode, int pathIndex, double newRate) {
    PathInfo *p = &(nodeToShortestPathsMap[destNode][pathIndex]);
    simtime_t lastSendTime = p->lastSendTime;
    double lastTxnSize = p->lastTransSize;
    simtime_t oldTime = p->timeToNextSend;

    // compute allowed to send
    double oldRate = p->rateToSendTrans; 
    simtime_t lastUpdateTime = p->lastRateUpdateTime; 
    simtime_t timeForThisRate = min(simTime() - lastUpdateTime, simTime() - lastSendTime);
    p->amtAllowedToSend += oldRate * timeForThisRate.dbl();
    p->amtAllowedToSend = min(_tokenBucketCapacity,p->amtAllowedToSend);

    // update the rate
    p->rateToSendTrans = newRate;
    double allowedToSend = p->amtAllowedToSend;
    p->lastRateUpdateTime = simTime();

    // Reschedule timer on this path according to this rate
    double rateToUse = max(newRate, _epsilon);
    simtime_t newTimeToSend = simTime() + max((lastTxnSize - allowedToSend)/ rateToUse, _epsilon);
    cancelEvent(p->triggerTransSendMsg);
    p->timeToNextSend = newTimeToSend;
    /*if (nodeToDestInfo[destNode].transWaitingToBeSent.size() == 0) 
        p->isSendTimerSet = false;
    else {*/
        scheduleAt(newTimeToSend, p->triggerTransSendMsg);
    //}
}

/* additional initalization that has to be done for the price based scheme
 * in particular set price variables to zero, initialize more signals
 * and schedule the first price update and price trigger
 */
void hostNodePriceScheme::initialize() {
    hostNodeBase::initialize();
    
    if (myIndex() == 0) {
        // price scheme parameters         
        _nesterov = false;
        _secondOrderOptimization = true;
        _reschedulingEnabled = true;
        _minWindow = 1.0;

        _eta = par("eta");
        _kappa = par("kappa");
        _tUpdate = par("updateQueryTime");
        _tQuery = par("updateQueryTime");
        _alpha = par("alpha");
        _gamma = 1; // ewma factor to compute per path rates
        _zeta = par("zeta"); // ewma for d_ij every source dest demand
        _minPriceRate = par("minRate");
        _rho = _rhoLambda = _rhoMu = par("rhoValue");
    }

    for(auto iter = nodeToPaymentChannel.begin(); iter != nodeToPaymentChannel.end(); ++iter) {
        int key = iter->first;
        
        nodeToPaymentChannel[key].lambda = nodeToPaymentChannel[key].yLambda = 0;
        nodeToPaymentChannel[key].muLocal = nodeToPaymentChannel[key].yMuLocal = 0;
        nodeToPaymentChannel[key].muRemote = nodeToPaymentChannel[key].yMuRemote = 0;

        // register signals for price per payment channel
        simsignal_t signal;
        signal = registerSignalPerChannel("nValue", key);
        nodeToPaymentChannel[key].nValueSignal = signal;
        
        signal = registerSignalPerChannel("xLocal", key);
        nodeToPaymentChannel[key].xLocalSignal = signal;
        
        signal = registerSignalPerChannel("muLocal", key);
        nodeToPaymentChannel[key].muLocalSignal = signal;

        signal = registerSignalPerChannel("lambda", key);
        nodeToPaymentChannel[key].lambdaSignal = signal;

        signal = registerSignalPerChannel("muRemote", key);
        nodeToPaymentChannel[key].muRemoteSignal = signal;
    }

    //initialize signals with all other nodes in graph
    for (int i = 0; i < _numHostNodes; ++i) {
        simsignal_t signal;
        signal = registerSignalPerDest("pathPerTrans", i, "");
        pathPerTransPerDestSignals.push_back(signal);

        signal = registerSignalPerDest("demandEstimate", i, "");
        demandEstimatePerDestSignals.push_back(signal);
        
        signal = registerSignalPerDest("numWaiting", i, "_Total");
        numWaitingPerDestSignals.push_back(signal);

        signal = registerSignalPerDest("numTimedOutAtSender", i, "_Total");
        numTimedOutAtSenderSignals.push_back(signal);
        statNumTimedOutAtSender.push_back(0);
    }


    // trigger the first set of triggers for price update and queries
    routerMsg *triggerPriceUpdateMsg = generateTriggerPriceUpdateMessage();
    scheduleAt(simTime() + _tUpdate, triggerPriceUpdateMsg );

    routerMsg *triggerPriceQueryMsg = generateTriggerPriceQueryMessage();
    scheduleAt(simTime() + _tQuery, triggerPriceQueryMsg );
    
}
