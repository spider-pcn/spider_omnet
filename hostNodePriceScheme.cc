#include "hostNodePriceScheme.h"

// global parameters
double _tokenBucketCapacity = 1.0;
bool _reschedulingEnabled; // whether timers can be rescheduled
bool _nesterov;
bool _secondOrderOptimization;
bool _useQueueEquation;

// parameters for price scheme
double _eta; //for price computation
double _kappa; //for price computation
double _capacityFactor; //for price computation
double _tUpdate; //for triggering price updates at routers
double _tQuery; //for triggering price query probes at hosts
double _alpha;
double _zeta;
double _delta;
double _avgDelay;
double _minPriceRate;
double _rhoLambda;
double _rhoMu;
double _rho;
double _minWindow;
double _xi;
double _routerQueueDrainTime;
double _smallRate = pow(10, -6); // ensuring that timers are no more than 1000 seconds away


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
routerMsg * hostNodePriceScheme::generatePriceUpdateMessage(double nLocal, double serviceRate, 
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
    sprintf(msgname, "tic-%d-to-%d (path %d) transactionSendMsg", myIndex(), destNode, pathIndex);
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

/* handler for timeout messages that is responsible for removing messages from 
 * sender queues if they haven't been sent yet and sends explicit time out messages
 * for messages that have been sent on a path already
 */
void hostNodePriceScheme::handleTimeOutMessage(routerMsg* ttmsg) {
    timeOutMsg *toutMsg = check_and_cast<timeOutMsg *>(ttmsg->getEncapsulatedPacket());
    int destination = toutMsg->getReceiver();
    int transactionId = toutMsg->getTransactionId();
    multiset<routerMsg*, transCompare> *transList = &(nodeToDestInfo[destination].transWaitingToBeSent);
    
    if (ttmsg->isSelfMessage()) {
        // if transaction was successful don't do anything more
        if (successfulDoNotSendTimeOut.count(transactionId) > 0) {
            successfulDoNotSendTimeOut.erase(toutMsg->getTransactionId());
            ttmsg->decapsulate();
            delete toutMsg;
            delete ttmsg;
            return;
        }

        // check if txn is still in just sender queue, just delete and return then
        auto iter = find_if(transList->begin(),
           transList->end(),
           [&transactionId](const routerMsg* p)
           { transactionMsg *transMsg = check_and_cast<transactionMsg *>(p->getEncapsulatedPacket());
             return transMsg->getTransactionId()  == transactionId; });

        if (iter!=transList->end()) {
            deleteTransaction(*iter);
            transList->erase(iter);
            ttmsg->decapsulate();
            delete toutMsg;
            delete ttmsg;
            return;
        }

        // send out a time out message on the path that hasn't acked all of it
        for (auto p : (nodeToShortestPathsMap[destination])){
            int pathIndex = p.first;
            tuple<int,int> key = make_tuple(transactionId, pathIndex);
                        
            if (transPathToAckState.count(key) > 0 && 
                    transPathToAckState[key].amtSent != transPathToAckState[key].amtReceived) {
                routerMsg* psMsg = generateTimeOutMessageForPath(
                    nodeToShortestPathsMap[destination][p.first].path, 
                    transactionId, destination);
                int nextNode = (psMsg->getRoute())[psMsg->getHopCount()+1];
                CanceledTrans ct = make_tuple(toutMsg->getTransactionId(), 
                        simTime(), -1, nextNode, destination);
                canceledTransactions.insert(ct);
                forwardMessage(psMsg);
            }
            else {
                transPathToAckState.erase(key);
            }
        }
        ttmsg->decapsulate();
        delete toutMsg;
        delete ttmsg;
    }
    else {
        // at the receiver
        CanceledTrans ct = make_tuple(toutMsg->getTransactionId(),simTime(),
                (ttmsg->getRoute())[ttmsg->getHopCount()-1], -1, toutMsg->getReceiver());
        canceledTransactions.insert(ct);
        ttmsg->decapsulate();
        delete toutMsg;
        delete ttmsg;
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

    SplitState* splitInfo = &(_numSplits[myIndex()][transMsg->getLargerTxnId()]);
    splitInfo->numArrived += 1;
    
    // first time seeing this transaction so add to d_ij computation
    // count the txn for accounting also
    if (simTime() == transMsg->getTimeSent()) {
        destNodeToNumTransPending[destNode]  += 1;
        nodeToDestInfo[destNode].transSinceLastInterval += transMsg->getAmount();
        if (splitInfo->numArrived == 1)
            splitInfo->firstAttemptTime = simTime().dbl();

        if (transMsg->getTimeSent() >= _transStatStart && 
            transMsg->getTimeSent() <= _transStatEnd) {
            statAmtArrived[destNode] += transMsg->getAmount();
            
            if (splitInfo->numArrived == 1) {       
                statNumArrived[destNode] += 1;
                statRateArrived[destNode] += 1; 
            }
        }
    }

    // initiate price probes if it is a new destination
    if (nodeToShortestPathsMap.count(destNode) == 0 && ttmsg->isSelfMessage()){
        vector<vector<int>> kShortestRoutes = getKPaths(transMsg->getSender(),destNode, _kValue);
        initializePriceProbes(kShortestRoutes, destNode);
        scheduleAt(simTime() + _maxTravelTime, ttmsg);
        return;
    }
    
    // at destination, add to incoming transUnits and trigger ack
    if (transMsg->getReceiver() == myIndex()) {
       handleTransactionMessage(ttmsg, false); 
    }
    else if (ttmsg->isSelfMessage()) {
        // at sender, either queue up or send on a path that allows you to send
        DestInfo* destInfo = &(nodeToDestInfo[destNode]);
       
        //send on a path if no txns queued up and timer was in the path
        if ((destInfo->transWaitingToBeSent).size() > 0) {
            pushIntoSenderQueue(destInfo, ttmsg);
        } else {
            for (auto p: nodeToShortestPathsMap[destNode]) {
                int pathIndex = p.first;
                PathInfo *pathInfo = &(nodeToShortestPathsMap[destNode][pathIndex]);
                pathInfo->window = max(pathInfo->rateToSendTrans * pathInfo->rttMin, _minWindow);
                
                if (pathInfo->rateToSendTrans > 0 && simTime() > pathInfo->timeToNextSend && 
                        (!_windowEnabled || 
                         (_windowEnabled && pathInfo->sumOfTransUnitsInFlight < pathInfo->window))) {
                    
                    ttmsg->setRoute(pathInfo->path);
                    ttmsg->setHopCount(0);
                    transMsg->setPathIndex(pathIndex);
                    handleTransactionMessage(ttmsg, true /*revisit*/);

                    // record stats on sent units for payment channel, destination and path
                    int nextNode = pathInfo->path[1];
                    nodeToPaymentChannel[nextNode].nValue += transMsg->getAmount();

                    // first attempt of larger txn
                    SplitState* splitInfo = &(_numSplits[myIndex()][transMsg->getLargerTxnId()]);
                    if (splitInfo->numAttempted == 0) {
                        splitInfo->numAttempted += 1;
                        if (transMsg->getTimeSent() >= _transStatStart && 
                            transMsg->getTimeSent() <= _transStatEnd) 
                            statRateAttempted[destNode] += 1;
                    }
                    
                    if (transMsg->getTimeSent() >= _transStatStart && 
                            transMsg->getTimeSent() <= _transStatEnd) {
                        statAmtAttempted[destNode] += transMsg->getAmount();
                    }
                    
                    pathInfo->nValue += transMsg->getAmount();
                    pathInfo->statRateAttempted += 1;
                    pathInfo->sumOfTransUnitsInFlight += transMsg->getAmount();
                    pathInfo->lastTransSize = transMsg->getAmount();
                    pathInfo->lastSendTime = simTime();
                    pathInfo->amtAllowedToSend = max(pathInfo->amtAllowedToSend - transMsg->getAmount(), 0.0);

                    // necessary for knowing what path to remove transaction in flight funds from
                    tuple<int,int> key = make_tuple(transMsg->getTransactionId(), pathIndex); 
                    transPathToAckState[key].amtSent += transMsg->getAmount();
                
                    // update the "time when the next transaction can be sent"
                    double rateToUse = max(pathInfo->rateToSendTrans, _smallRate);
                    double additional = min(max((transMsg->getAmount())/ rateToUse, _epsilon), 10000.0);
                    simtime_t newTimeToNextSend =  simTime() + additional;
                    pathInfo->timeToNextSend = newTimeToNextSend;
                    return;
                }
            }
            
            //transaction cannot be sent on any of the paths, queue transaction
            pushIntoSenderQueue(destInfo, ttmsg);

            for (auto p: nodeToShortestPathsMap[destNode]) {
                PathInfo *pInfo = &(nodeToShortestPathsMap[destNode][p.first]);
                if (pInfo->isSendTimerSet == false) {
                    simtime_t timeToNextSend = pInfo->timeToNextSend;
                    if (simTime() > timeToNextSend) {
                        timeToNextSend = simTime() + 0.0001;
                    }
                    cancelEvent(pInfo->triggerTransSendMsg);            
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
            int node = it->first; 
            PaymentChannel* p = &(nodeToPaymentChannel[node]);

            emit(p->nValueSignal, p->nValue);
            emit(p->lambdaSignal, p->lambda);
            emit(p->muLocalSignal, p->muLocal);
            emit(p->muRemoteSignal, p->muRemote);
        }
        
        // per destination statistics
        for (auto it = 0; it < _numHostNodes; it++){ 
            if (it != getIndex() && _destList[myIndex()].count(it) > 0) {
                if (nodeToShortestPathsMap.count(it) > 0) {
                    for (auto& p: nodeToShortestPathsMap[it]){
                        int pathIndex = p.first;
                        PathInfo *pInfo = &(p.second);
                        emit(pInfo->rateToSendTransSignal, pInfo->rateToSendTrans);
                        emit(pInfo->rateActuallySentSignal, pInfo->rateSentOnPath);
                        emit(pInfo->sumOfTransUnitsInFlightSignal, 
                                pInfo->sumOfTransUnitsInFlight);
                        emit(pInfo->windowSignal, pInfo->window);
                        emit(pInfo->priceLastSeenSignal, pInfo->priceLastSeen);
                        emit(pInfo->rateOfAcksSignal, pInfo->amtAcked/_statRate);
                        pInfo->amtAcked = 0;
                    }
                }

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
    int transactionId = aMsg->getTransactionId();
    double largerTxnId = aMsg->getLargerTxnId();

    if (aMsg->getIsSuccess() == false) {
        // make sure transaction isn't cancelled yet
        auto iter = canceledTransactions.find(make_tuple(transactionId, 0, 0, 0, 0));
    
        if (iter != canceledTransactions.end()) {
            if (aMsg->getTimeSent() >= _transStatStart && aMsg->getTimeSent() <= _transStatEnd)
                statAmtFailed[destNode] += aMsg->getAmount();
        } 
        else {
            // requeue transaction
            routerMsg *duplicateTrans = generateDuplicateTransactionMessage(aMsg);
            pushIntoSenderQueue(&(nodeToDestInfo[destNode]), duplicateTrans);
        }
 
    }
    else {
        SplitState* splitInfo = &(_numSplits[myIndex()][largerTxnId]);
        splitInfo->numReceived += 1;

        if (aMsg->getTimeSent() >= _transStatStart && 
                aMsg->getTimeSent() <= _transStatEnd) {
            statAmtCompleted[destNode] += aMsg->getAmount();

            if (splitInfo->numTotal == splitInfo->numReceived) {
                statNumCompleted[destNode] += 1; 
                statRateCompleted[destNode] += 1;
                _transactionCompletionBySize[splitInfo->totalAmount] += 1;
                double timeTaken = simTime().dbl() - splitInfo->firstAttemptTime;
                statCompletionTimes[destNode] += timeTaken * 1000;
                _txnAvgCompTimeBySize[splitInfo->totalAmount] += timeTaken * 1000;
                recordTailCompletionTime(aMsg->getTimeSent(), splitInfo->totalAmount, timeTaken * 1000);
            }
        }
        nodeToShortestPathsMap[destNode][pathIndex].statRateCompleted += 1;
        nodeToShortestPathsMap[destNode][pathIndex].amtAcked += aMsg->getAmount();
    }

    //increment transaction amount ack on a path. 
    tuple<int,int> key = make_tuple(transactionId, pathIndex);
    transPathToAckState[key].amtReceived += aMsg->getAmount();
    
    nodeToShortestPathsMap[destNode][pathIndex].sumOfTransUnitsInFlight -= aMsg->getAmount();
    destNodeToNumTransPending[destNode] -= 1;     
    hostNodeBase::handleAckMessage(ttmsg);
}

/* handler for the clear state message that deals with
 * transactions that will no longer be completed
 * In particular clears out the amount inn flight on the path
 */
void hostNodePriceScheme::handleClearStateMessage(routerMsg *ttmsg) {
    for ( auto it = canceledTransactions.begin(); it!= canceledTransactions.end(); it++){
        int transactionId = get<0>(*it);
        simtime_t msgArrivalTime = get<1>(*it);
        int prevNode = get<2>(*it);
        int nextNode = get<3>(*it);
        int destNode = get<4>(*it);
        
        if (simTime() > (msgArrivalTime + _maxTravelTime + 1)){
            // ack was not received,safely can consider this txn done
            for (auto p : nodeToShortestPathsMap[destNode]) {
                int pathIndex = p.first;
                tuple<int,int> key = make_tuple(transactionId, pathIndex);
                if (transPathToAckState.count(key) != 0) {
                    nodeToShortestPathsMap[destNode][pathIndex].sumOfTransUnitsInFlight -= 
                        (transPathToAckState[key].amtSent - transPathToAckState[key].amtReceived);
                    transPathToAckState.erase(key);
                }
            }
        }
    }

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
    
    // also update the rate actually Sent for a given destination and path 
    // to be used for your next rate computation
    for ( auto it = nodeToShortestPathsMap.begin(); it != nodeToShortestPathsMap.end(); it++ ) {
        int destNode = it->first;
        for (auto p : nodeToShortestPathsMap[destNode]) {
            PathInfo *pInfo = &(nodeToShortestPathsMap[destNode][p.first]); 
            double latestRate = pInfo->nValue / _tUpdate;
            double oldRate = pInfo->rateSentOnPath; 
            
            pInfo->nValue = 0; 
            pInfo->rateSentOnPath = latestRate; 
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
    double updateRateLocal = neighborChannel->updateRate;
    double nLocal = neighborChannel->lastNValue;
    double inflightLocal = min(getTotalAmountOutgoingInflight(sender) + 
        updateRateLocal* _avgDelay/1000.0, _capacities[senderReceiverTuple]);
    double qLocal = neighborChannel->lastQueueSize; 
    double serviceRateLocal = neighborChannel->serviceRate;
    double arrivalRateLocal = neighborChannel->arrivalRate;

    tuple<int, int> node1node2Pair = (myIndex() < sender) ? make_tuple(myIndex(), sender) : make_tuple(sender,
            myIndex());
    double cValue = _capacities[node1node2Pair]; 
    double oldLambda = neighborChannel->lambda;
    double oldMuLocal = neighborChannel->muLocal;
    double oldMuRemote = neighborChannel->muRemote;
 
    double newLambdaGrad;
    double newMuLocalGrad;
    if (_useQueueEquation) {
        newLambdaGrad	= inflightLocal*arrivalRateLocal/serviceRateLocal + 
            inflightRemote * arrivalRateRemote/serviceRateRemote + 
            2*_xi*min(qLocal, qRemote) - (_capacityFactor * cValue);
        
        newMuLocalGrad	= nLocal - nRemote + 
            qLocal*_tUpdate/_routerQueueDrainTime - 
            qRemote*_tUpdate/_routerQueueDrainTime;
    } else {
        newLambdaGrad = inflightLocal*arrivalRateLocal/serviceRateLocal + 
            inflightRemote * arrivalRateRemote/serviceRateRemote - (_capacityFactor * cValue);
        newMuLocalGrad	= nLocal - nRemote;
    }

    if (_rebalancingEnabled) {
        double amtAdded = (oldMuLocal - _gamma);
        double newSize = neighborChannel->fakeRebalancingQueue + amtAdded;
        neighborChannel->fakeRebalancingQueue = max(0.0, newSize);
        if (newSize > _maxGammaImbalanceQueueSize) {
            neighborChannel->balance += amtAdded;
            tuple<int, int> senderReceiverTuple = 
                    (sender < myIndex()) ? make_tuple(sender, myIndex()) :
                    make_tuple(myIndex(), sender);
            _capacities[senderReceiverTuple] += amtAdded;
            neighborChannel->amtAdded += amtAdded;
            neighborChannel->numRebalanceEvents += 1;
            neighborChannel->fakeRebalancingQueue = 0;
        }
    }

    // Nesterov's gradient descent equation
    // and other speeding up mechanisms
    double newLambda = 0.0;
    double newMuLocal = 0.0;
    double newMuRemote = 0.0;
    double myKappa = _kappa;
    double myEta = _eta;
    if (_nesterov) {
        double yLambda = neighborChannel->yLambda;
        double yMuLocal = neighborChannel->yMuLocal;
        double yMuRemote = neighborChannel->yMuRemote;

        double yLambdaNew = oldLambda + myEta*newLambdaGrad;
        newLambda = yLambdaNew + _rhoLambda*(yLambdaNew - yLambda); 
        neighborChannel->yLambda = yLambdaNew;

        double yMuLocalNew = oldMuLocal + myKappa*newMuLocalGrad;
        newMuLocal = yMuLocalNew + _rhoMu*(yMuLocalNew - yMuLocal);
        neighborChannel->yMuLocal = yMuLocalNew;

        double yMuRemoteNew = oldMuRemote - myKappa*newMuLocalGrad;
        newMuRemote = yMuRemoteNew + _rhoMu*(yMuRemoteNew - yMuRemote);
        neighborChannel->yMuRemote = yMuRemoteNew;
    } 
    else {
        newLambda = oldLambda +  myEta*newLambdaGrad;
        newMuLocal = oldMuLocal + myKappa*newMuLocalGrad;
        newMuRemote = oldMuRemote - myKappa*newMuLocalGrad; 
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
    if (simTime() > _simulationLength){
        delete ttmsg;
    } else {
        scheduleAt(simTime()+_tQuery, ttmsg);
    }
    
    // go through all destinations that have pending transactions and send 
    // out a new probe to them
    for (auto it = destNodeToNumTransPending.begin(); it!=destNodeToNumTransPending.end(); it++){
        if (it->first == myIndex()){
            continue;
        }
        
        if (it->second>0){ 
            //if we have transactions pending
            for (auto p = nodeToShortestPathsMap[it->first].begin() ;
                    p!= nodeToShortestPathsMap[it->first].end(); p++){
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
        double zNew =  zOld;// + (2 * lambda) + muLocal  - muRemote;
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
        double oldRate = p->rateToSendTrans;

        // Nesterov's gradient descent equation
        // and other speeding up mechanisms
        double sumOfRates = 0.0;
        double preProjectionRate = 0.0;
        if (_nesterov) {
          double yPrev = p->yRateToSendTrans;
          double yNew = oldRate + _alpha*(1 - zValue);
          p->yRateToSendTrans = yNew;
          preProjectionRate = yNew + _rho*(yNew - yPrev);
        } else if (_secondOrderOptimization) {
            for (auto curPath : nodeToShortestPathsMap[destNode]) {
                sumOfRates += curPath.second.rateToSendTrans;
            }
            double delta = _alpha*(1 - zValue) + _rho*(p->priceLastSeen - zValue);
            preProjectionRate = oldRate + delta ;
        }
        else {
          preProjectionRate  = oldRate + _alpha*(1 - zValue);
        }

        p->priceLastSeen = zValue;
        p->oldSumOfRates = sumOfRates;

        // compute the projection of this new rate along with old rates
        double sumRates = 0.0;
        vector<PathRateTuple> pathRateTuples;
        for (auto p : nodeToShortestPathsMap[destNode]) {
            int pathIndex = p.first;
            double rate;
            if (pathIndex != routeIndex) {
                rate =  p.second.rateToSendTrans;
            } else {
                rate = preProjectionRate;
            }
            sumRates += p.second.rateToSendTrans;
              
            PathRateTuple newTuple = make_tuple(pathIndex, rate);
            pathRateTuples.push_back(newTuple);
        }

        int queueSize = nodeToDestInfo[destNode].transWaitingToBeSent.size();
        double drainTime = 100.0; // seconds
        double bias = nodeToDestInfo[destNode].demand - sumRates; 
        vector<PathRateTuple> projectedRates = 
            computeProjection(pathRateTuples, 1.1*nodeToDestInfo[destNode].demand);

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
        }
              
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
    p->window = max(p->rateToSendTrans * p->rttMin, _minWindow);

    if (nodeToDestInfo[destNode].transWaitingToBeSent.size() > 0 && (!_windowEnabled || 
            (_windowEnabled && p->sumOfTransUnitsInFlight < p->window))){
        //remove the transaction $tu$ at the head of the queue
        auto firstPosition = nodeToDestInfo[destNode].transWaitingToBeSent.begin();
        routerMsg *msgToSend = *firstPosition;
        nodeToDestInfo[destNode].transWaitingToBeSent.erase(firstPosition);
        transactionMsg *transMsg = 
           check_and_cast<transactionMsg *>(msgToSend->getEncapsulatedPacket());
        
        // Send the transaction treansaction unit along the corresponding path.
        transMsg->setPathIndex(pathIndex);
        msgToSend->setRoute(path);
        msgToSend->setHopCount(0);

        // increment the number of units sent on a particular payment channel
        int nextNode = path[1];
        nodeToPaymentChannel[nextNode].nValue = nodeToPaymentChannel[nextNode].nValue + transMsg->getAmount();

        // increment amount in inflght and other info on last transaction on this path
        p->nValue += transMsg->getAmount();
        p->sumOfTransUnitsInFlight = p->sumOfTransUnitsInFlight + transMsg->getAmount();
        p->lastTransSize = transMsg->getAmount();
        p->lastSendTime = simTime();
        p->amtAllowedToSend = max(p->amtAllowedToSend - transMsg->getAmount(), 0.0);
        
        // necessary for knowing what path to remove transaction in flight funds from
        tuple<int,int> key = make_tuple(transMsg->getTransactionId(), pathIndex); 
        transPathToAckState[key].amtSent += transMsg->getAmount();

        // cannot be cancelled at this point
        handleTransactionMessage(msgToSend, 1/*revisiting*/);
        p->statRateAttempted = p->statRateAttempted + 1;

        // first attempt of larger txn
        SplitState* splitInfo = &(_numSplits[myIndex()][transMsg->getLargerTxnId()]);
        if (splitInfo->numAttempted == 0) {
            splitInfo->numAttempted += 1;
            if (transMsg->getTimeSent() >= _transStatStart && 
                transMsg->getTimeSent() <= _transStatEnd)
                statRateAttempted[destNode] += 1;
        }

        if (transMsg->getTimeSent() >= _transStatStart && 
            transMsg->getTimeSent() <= _transStatEnd){
            statAmtAttempted[destNode] += transMsg->getAmount();
        }

        //Update the  “time when next transaction can be sent” 
        double bound = _reschedulingEnabled ? _smallRate  : 1.0;
        double rateToSendTrans = max(p->rateToSendTrans, bound);
        double additional =  min(transMsg->getAmount()/rateToSendTrans, 10000.0);
        p->timeToNextSend = simTime() + additional; 
        cancelEvent(ttmsg);
        scheduleAt(p->timeToNextSend, ttmsg);
    }
    else { 
        //no trans to send
        // don't turn off timers
        PathInfo* p = &(nodeToShortestPathsMap[destNode][pathIndex]);
        double rateToSendTrans = max(p->rateToSendTrans, _smallRate);
        double lastTxnSize = p->lastTransSize;
        double additional = min(max(lastTxnSize/rateToSendTrans, _epsilon), 10000.0);
        p->timeToNextSend = simTime() + additional;
        cancelEvent(ttmsg);
        scheduleAt(p->timeToNextSend, ttmsg);
        p->amtAllowedToSend = 0.0;
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
        temp.rttMin = (kShortestPaths[pathIdx].size() - 1) * 2 * _avgDelay/1000.0;
        nodeToShortestPathsMap[destNode][pathIdx] = temp;

        //initialize signals
        simsignal_t signal;
        signal = registerSignalPerDestPath("rateToSendTrans", pathIdx, destNode);
        nodeToShortestPathsMap[destNode][pathIdx].rateToSendTransSignal = signal;

        signal = registerSignalPerDestPath("rateSent", pathIdx, destNode);
        nodeToShortestPathsMap[destNode][pathIdx].rateActuallySentSignal = signal;
        
        signal = registerSignalPerDestPath("timeToNextSend", pathIdx, destNode);
        nodeToShortestPathsMap[destNode][pathIdx].timeToNextSendSignal = signal;

        signal = registerSignalPerDestPath("sumOfTransUnitsInFlight", pathIdx, destNode);
        nodeToShortestPathsMap[destNode][pathIdx].sumOfTransUnitsInFlightSignal = signal;

        signal = registerSignalPerDestPath("window", pathIdx, destNode);
        nodeToShortestPathsMap[destNode][pathIdx].windowSignal = signal;
      
        signal = registerSignalPerDestPath("priceLastSeen", pathIdx, destNode);
        nodeToShortestPathsMap[destNode][pathIdx].priceLastSeenSignal = signal;

        signal = registerSignalPerDestPath("rateOfAcks", pathIdx, destNode);
        nodeToShortestPathsMap[destNode][pathIdx].rateOfAcksSignal = signal;
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
    p->amtAllowedToSend = min(lastTxnSize, p->amtAllowedToSend);

    // update the rate
    p->rateToSendTrans = newRate;
    double allowedToSend = p->amtAllowedToSend;
    p->lastRateUpdateTime = simTime();

    // Reschedule timer on this path according to this rate
    double rateToUse = max(newRate, _smallRate);
    double additional = min(max((lastTxnSize - allowedToSend)/ rateToUse, _epsilon), 10000.0);
    simtime_t newTimeToSend = simTime() + additional;

    cancelEvent(p->triggerTransSendMsg);
    p->timeToNextSend = newTimeToSend;
    scheduleAt(newTimeToSend, p->triggerTransSendMsg);
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
        _capacityFactor = par("capacityFactor");
        _useQueueEquation = par("useQueueEquation");
        _tUpdate = par("updateQueryTime");
        _tQuery = par("updateQueryTime");
        _alpha = par("alpha");
        _zeta = par("zeta"); // ewma for d_ij every source dest demand
        _minPriceRate = par("minRate");
        _rho = _rhoLambda = _rhoMu = par("rhoValue");
        _xi = par("xi");
        _routerQueueDrainTime = par("routerQueueDrainTime"); // seconds
        _windowEnabled = par("windowEnabled");
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
        
        signal = registerSignalPerChannel("muLocal", key);
        nodeToPaymentChannel[key].muLocalSignal = signal;

        signal = registerSignalPerChannel("lambda", key);
        nodeToPaymentChannel[key].lambdaSignal = signal;
    }

    // initialize signals with all other nodes in graph
    // that there is demand for
    for (int i = 0; i < _numHostNodes; ++i) {
        if (_destList[myIndex()].count(i) > 0) {
            simsignal_t signal;
            signal = registerSignalPerDest("demandEstimate", i, "");
            demandEstimatePerDestSignals[i] = signal;
        
            signal = registerSignalPerDest("numWaiting", i, "_Total");
            numWaitingPerDestSignals[i] = signal;
        }
    }
    // trigger the first set of triggers for price update and queries
    routerMsg *triggerPriceUpdateMsg = generateTriggerPriceUpdateMessage();
    scheduleAt(simTime() + _tUpdate, triggerPriceUpdateMsg );

    routerMsg *triggerPriceQueryMsg = generateTriggerPriceQueryMessage();
    scheduleAt(simTime() + _tQuery, triggerPriceQueryMsg );
}
