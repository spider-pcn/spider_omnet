#include "hostNodePropFairPriceScheme.h"
Define_Module(hostNodePropFairPriceScheme);

// global parameters
/* new variables */
double _cannonicalRTT = 0;
double _totalPaths = 0;
double computeDemandRate = 0.5;
double rateDecreaseFrequency = 5;


/* creates a message that is meant to signal the time when a transaction can
 * be sent on the path in context based on the current view of the rate
 */
routerMsg *hostNodePropFairPriceScheme::generateTriggerTransactionSendMessage(vector<int> path, 
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

/* generate statistic trigger message every x seconds
 * to output all statistics which can then be plotted
 */
routerMsg *hostNodePropFairPriceScheme::generateComputeDemandMessage(){
    char msgname[MSGSIZE];
    sprintf(msgname, "node-%d computeDemandMsg", myIndex());
    routerMsg *rMsg = new routerMsg(msgname);
    rMsg->setMessageType(COMPUTE_DEMAND_MSG);
    return rMsg;
}

/* generate trigger message to compute the fraction of marked packets 
 * on all paths to all destinations every x seconds
 * then do the beta portion for all of those rates
 */
routerMsg *hostNodePropFairPriceScheme::generateTriggerRateDecreaseMessage(){
    char msgname[MSGSIZE];
    sprintf(msgname, "node-%d triggerRateDecreaseMsg", myIndex());
    routerMsg *rMsg = new routerMsg(msgname);
    rMsg->setMessageType(TRIGGER_RATE_DECREASE_MSG);
    return rMsg;
}

/* check if all the provided rates are non-negative and also verify
 *  that their sum is less than the demand, return false otherwise
 */
bool hostNodePropFairPriceScheme::ratesFeasible(vector<PathRateTuple> actualRates, double demand) {
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
vector<PathRateTuple> hostNodePropFairPriceScheme::computeProjection(
        vector<PathRateTuple> recommendedRates, double demand) {
    /* to resolve issues very early on when demand hasn't been computed yet */
    if (demand == 0)
        demand = 1;

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
void hostNodePropFairPriceScheme::handleMessage(cMessage *msg) {
    routerMsg *ttmsg = check_and_cast<routerMsg *>(msg);
    if (simTime() > _simulationLength){
        auto encapMsg = (ttmsg->getEncapsulatedPacket());
        ttmsg->decapsulate();
        delete ttmsg;
        delete encapMsg;
        return;
    } 

    switch(ttmsg->getMessageType()) {
        case TRIGGER_TRANSACTION_SEND_MSG:
             if (_loggingEnabled) cout<< "[HOST "<< myIndex() 
                 <<": RECEIVED TRIGGER_TXN_SEND MSG] "<< ttmsg->getName() << endl;
             handleTriggerTransactionSendMessage(ttmsg);
             if (_loggingEnabled) cout<< "[AFTER HANDLING:]  "<< endl;
             break;
         
        case TRIGGER_RATE_DECREASE_MSG:
             if (_loggingEnabled) cout<< "[HOST "<< myIndex() 
                 <<": RECEIVED TRIGGER_RATE_DECREASE MSG] "<< ttmsg->getName() << endl;
             handleTriggerRateDecreaseMessage(ttmsg);
             if (_loggingEnabled) cout<< "[AFTER HANDLING:]  "<< endl;
             break;   
        
        case COMPUTE_DEMAND_MSG:
             if (_loggingEnabled) cout<< "[HOST "<< myIndex() 
                 <<": RECEIVED COMPUTE_DEMAND MSG] "<< ttmsg->getName() << endl;
             handleComputeDemandMessage(ttmsg);
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
void hostNodePropFairPriceScheme::handleTimeOutMessage(routerMsg* ttmsg) {
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
            statNumTimedOut[destination] += 1;
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
void hostNodePropFairPriceScheme::handleTransactionMessageSpecialized(routerMsg* ttmsg){
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
        initializePathInfo(kShortestRoutes, destNode);
    }
    
    // at destination, add to incoming transUnits and trigger ack
    if (transMsg->getReceiver() == myIndex()) {
        int prevNode = ttmsg->getRoute()[ttmsg->getHopCount() - 1];
        unordered_map<Id, double, hashId> *incomingTransUnits = 
            &(nodeToPaymentChannel[prevNode].incomingTransUnits);
        (*incomingTransUnits)[make_tuple(transMsg->getTransactionId(), transMsg->getHtlcIndex())] =
            transMsg->getAmount();
        
        if (_timeoutEnabled) {
            auto iter = canceledTransactions.find(make_tuple(transactionId, 0, 0, 0, 0));
            if (iter != canceledTransactions.end()){
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

        // use a random ordering on the path indices
        vector<int> pathIndices;
        for (int i = 0; i < nodeToShortestPathsMap[destNode].size(); ++i) pathIndices.push_back(i);
        random_shuffle(pathIndices.begin(), pathIndices.end());
       
        //send on a path if no txns queued up and timer was in the path
        if ((destInfo->transWaitingToBeSent).size() > 0) {
            pushIntoSenderQueue(destInfo, ttmsg);
        } else {
            for (auto p: pathIndices) {
                int pathIndex = p;
                PathInfo *pathInfo = &(nodeToShortestPathsMap[destNode][pathIndex]);
                
                if (pathInfo->rateToSendTrans > 0 && simTime() > pathInfo->timeToNextSend && 
                        pathInfo->sumOfTransUnitsInFlight + transMsg->getAmount() <= pathInfo->window) {
                    ttmsg->setRoute(pathInfo->path);
                    ttmsg->setHopCount(0);
                    transMsg->setPathIndex(pathIndex);
                    handleTransactionMessage(ttmsg, true /*revisit*/);

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
                    
                    // update stats
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
            
            // transaction cannot be sent on any of the paths, queue transaction
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

/* handler for compute demand message triggered every y seconds simply to comput the demand per destination
 */
void hostNodePropFairPriceScheme::handleComputeDemandMessage(routerMsg* ttmsg){
    // reschedule this message to be sent again
    if (simTime() > _simulationLength){
        delete ttmsg;
    }
    else {
        scheduleAt(simTime() + computeDemandRate, ttmsg);
    }

    for (auto it = 0; it < _numHostNodes; it++){ 
        if (it != getIndex() && _destList[myIndex()].count(it) > 0) {
            //nodeToDestInfo[it].demand = (myIndex() == 0) ? 100 : 250 ;
            DestInfo* destInfo = &(nodeToDestInfo[it]);
            double newDemand = destInfo->transSinceLastInterval / computeDemandRate;
            destInfo->demand = (1 - _zeta) * destInfo->demand + _zeta * newDemand;
            destInfo->transSinceLastInterval = 0;
        }
    }
}


/* handler for trigger rate decrease message triggered every y seconds
 * to compute the fraction of marked packets on all paths to the destinations
 * and perform the decrease portion of the control algorithm
 */
void hostNodePropFairPriceScheme::handleTriggerRateDecreaseMessage(routerMsg* ttmsg){
    // reschedule this message to be sent again
    if (simTime() > _simulationLength){
        delete ttmsg;
    }
    else {
        scheduleAt(simTime() + rateDecreaseFrequency, ttmsg);
    }

    // go through all destinations that have pending transactions and 
    // update the fraction of marked packets over the last y seconds
    for (auto it = destNodeToNumTransPending.begin(); it!=destNodeToNumTransPending.end(); it++){
        if (it->first == myIndex()){
            continue;
        }
        
        if (it->second>0){ 
            //if we have transactions pending
            for (auto p = nodeToShortestPathsMap[it->first].begin() ;
                    p!= nodeToShortestPathsMap[it->first].end(); p++){
                // p is per path in the destNode
                int routeIndex = p->first;
                PathInfo *pInfo = &(nodeToShortestPathsMap[it->first][p->first]);
                pInfo->lastMarkedFraction = pInfo->totalMarkedPacketsForInterval/pInfo->totalPacketsForInterval;

                pInfo->totalMarkedPacketsForInterval = 0;
                pInfo->totalPacketsForInterval = 0;
                pInfo->rateToSendTrans  -= _windowBeta * pInfo->lastMarkedFraction * pInfo->rateToSendTrans;
                pInfo->rateToSendTrans = max(_minPriceRate, pInfo->rateToSendTrans);
            }
        }
    }
}


/* handler for the statistic message triggered every x seconds to also
 * output the price based scheme stats in addition to the default
 */
void hostNodePropFairPriceScheme::handleStatMessage(routerMsg* ttmsg){
    if (_signalsEnabled) {
        // per destination statistics
        for (auto it = 0; it < _numHostNodes; it++){ 
            if (it != getIndex() && _destList[myIndex()].count(it) > 0) {
                if (nodeToShortestPathsMap.count(it) > 0) {
                    for (auto& p: nodeToShortestPathsMap[it]){
                        int pathIndex = p.first;
                        PathInfo *pInfo = &(p.second);

                        //signals for price scheme per path
                        emit(pInfo->rateToSendTransSignal, pInfo->rateToSendTrans);
                        emit(pInfo->rateActuallySentSignal, pInfo->nValue /_statRate);
                        emit(pInfo->sumOfTransUnitsInFlightSignal, 
                                pInfo->sumOfTransUnitsInFlight);
                        emit(pInfo->windowSignal, pInfo->window);
                        emit(pInfo->rateOfAcksSignal, pInfo->amtAcked/_statRate);
                        emit(pInfo->measuredRTTSignal, pInfo->measuredRTT);
                        emit(pInfo->fractionMarkedSignal, 
                                pInfo->markedPackets/(pInfo->markedPackets + pInfo->unmarkedPackets));
                        emit(pInfo->smoothedFractionMarkedSignal, pInfo->lastMarkedFraction);
                        pInfo->amtAcked = 0;
                        pInfo->unmarkedPackets = 0;
                        pInfo->markedPackets = 0;
                        pInfo->nValue = 0;
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
void hostNodePropFairPriceScheme::handleAckMessageSpecialized(routerMsg* ttmsg){
    ackMsg *aMsg = check_and_cast<ackMsg*>(ttmsg->getEncapsulatedPacket());
    int pathIndex = aMsg->getPathIndex();
    int destNode = ttmsg->getRoute()[0];
    int transactionId = aMsg->getTransactionId();
    double largerTxnId = aMsg->getLargerTxnId();
    PathInfo* thisPath = &(nodeToShortestPathsMap[destNode][pathIndex]);

    // rate update based on marked or unmarked packet
    if (aMsg->getIsMarked()) {
        thisPath->window  -= _windowBeta;
        thisPath->window = max(_minWindow, thisPath->window);
        thisPath->markedPackets += 1; 
        thisPath->totalMarkedPacketsForInterval += 1;
    }
    else {
        thisPath->unmarkedPackets += 1; 
        double sumWindows = 0; 
        for (auto p : nodeToShortestPathsMap[destNode]) 
            sumWindows += p.second.window;
        thisPath->window += _windowAlpha / sumWindows;
    }
    thisPath->totalPacketsForInterval += 1;
    double preProjectionRate = thisPath->window/(0.9 * thisPath->measuredRTT);
    
    vector<PathRateTuple> pathRateTuples;
    double sumRates = 0;
    for (auto p : nodeToShortestPathsMap[destNode]) {
        sumRates += p.second.rateToSendTrans;
        double rate;
        if (p.first != pathIndex)
            rate = p.second.rateToSendTrans;
        else
            rate = preProjectionRate;
        PathRateTuple newTuple = make_tuple(p.first, rate);
        pathRateTuples.push_back(newTuple);
    }
    vector<PathRateTuple> projectedRates = pathRateTuples; 

    // reassign all path's rates to the projected rates and 
    // make sure it is atleast minPriceRate for every path
    for (auto p : projectedRates) {
        int index = get<0>(p);
        double newRate = max(get<1>(p), _minPriceRate);
        updateTimers(destNode, index, newRate);
    }

    // react to success or failure
    if (aMsg->getIsSuccess() == false) {
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
        thisPath->statRateCompleted += 1;
        thisPath->amtAcked += aMsg->getAmount();
        double newRTT = simTime().dbl() - aMsg->getTimeAttempted();
        thisPath->measuredRTT = 0.01 * newRTT + 0.99 * thisPath->measuredRTT;
    }

    // increment transaction amount ack on a path. 
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
void hostNodePropFairPriceScheme::handleClearStateMessage(routerMsg *ttmsg) {
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
                PathInfo* thisPath = &(nodeToShortestPathsMap[destNode][pathIndex]);
                tuple<int,int> key = make_tuple(transactionId, pathIndex);
                if (transPathToAckState.count(key) != 0) {
                    thisPath->sumOfTransUnitsInFlight -= 
                        (transPathToAckState[key].amtSent - transPathToAckState[key].amtReceived);
                    transPathToAckState.erase(key);

                    // treat this basiscally as one marked packet
                    thisPath->window  -= _windowBeta;
                    thisPath->window = max(_minWindow, thisPath->window);
                    thisPath->markedPackets += 1; 
                    thisPath->totalMarkedPacketsForInterval += 1;
                    thisPath->rateToSendTrans = thisPath->window/(0.9 * thisPath->measuredRTT);
                }
            }
        }
    }

    // works fine now because timeouts start per transaction only when
    // sent out and no txn splitting
    hostNodeBase::handleClearStateMessage(ttmsg);
}


/* handles the sending of transactions at a certain rate indirectly via timers 
 * going off to trigger the next send on a particular path. This responds to
 * that trigger and pulls the next transaction off the queue and sends that
 */
void hostNodePropFairPriceScheme::handleTriggerTransactionSendMessage(routerMsg* ttmsg){
    transactionSendMsg *tsMsg = 
        check_and_cast<transactionSendMsg *>(ttmsg->getEncapsulatedPacket());

    vector<int> path = tsMsg->getTransactionPath();
    int pathIndex = tsMsg->getPathIndex();
    int destNode = tsMsg->getReceiver();
    PathInfo* p = &(nodeToShortestPathsMap[destNode][pathIndex]);

    bool sentSomething = false;
    if (nodeToDestInfo[destNode].transWaitingToBeSent.size() > 0) {
        auto firstPosition = nodeToDestInfo[destNode].transWaitingToBeSent.begin();
        routerMsg *msgToSend = *firstPosition;
        transactionMsg *transMsg = 
           check_and_cast<transactionMsg *>(msgToSend->getEncapsulatedPacket());
        
        if (p->sumOfTransUnitsInFlight + transMsg->getAmount() <= p->window){
            //remove the transaction $tu$ at the head of the queue
            nodeToDestInfo[destNode].transWaitingToBeSent.erase(firstPosition);
            sentSomething = true;
            
            //Send the transaction $tu$ along the corresponding path.
            transMsg->setPathIndex(pathIndex);
            msgToSend->setRoute(path);
            msgToSend->setHopCount(0);

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

            //If there are more transactions queued up, reschedule timer
            cancelEvent(ttmsg);
            scheduleAt(p->timeToNextSend, ttmsg);
        }
    }

    if (!sentSomething) {
        // something shady here TODO 
        // no trans sendable, just reschedule timer in a little bit as if it had never happened
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
void hostNodePropFairPriceScheme::initializePathInfo(vector<vector<int>> kShortestPaths, int destNode){
    for (int pathIdx = 0; pathIdx < kShortestPaths.size(); pathIdx++){
        // initialize pathInfo
        PathInfo temp = {};
        temp.path = kShortestPaths[pathIdx];
        routerMsg * triggerTransSendMsg = 
          generateTriggerTransactionSendMessage(kShortestPaths[pathIdx], pathIdx, destNode);
        temp.triggerTransSendMsg = triggerTransSendMsg;
        temp.rateToSendTrans = _minPriceRate;
        temp.window = _minWindow;
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
              
        signal = registerSignalPerDestPath("fractionMarked", pathIdx, destNode);
        nodeToShortestPathsMap[destNode][pathIdx].fractionMarkedSignal = signal;
  
        signal = registerSignalPerDestPath("smoothedFractionMarked", pathIdx, destNode);
        nodeToShortestPathsMap[destNode][pathIdx].smoothedFractionMarkedSignal = signal;      
        
        signal = registerSignalPerDestPath("rateOfAcks", pathIdx, destNode);
        nodeToShortestPathsMap[destNode][pathIdx].rateOfAcksSignal = signal;

        signal = registerSignalPerDestPath("measuredRTT", pathIdx, destNode);
        nodeToShortestPathsMap[destNode][pathIdx].measuredRTTSignal = signal;
   }
}



/* helper method to reschedule the timer on a given path according to the new rate
 */
void hostNodePropFairPriceScheme::updateTimers(int destNode, int pathIndex, double newRate) {
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
void hostNodePropFairPriceScheme::initialize() {
    hostNodeBase::initialize();
    
    if (myIndex() == 0) {
        // price scheme parameters         
        _reschedulingEnabled = true;
        _minWindow = par("minDCTCPWindow");
        _windowEnabled = true;
        _windowAlpha = par("windowAlpha");
        _windowBeta = par("windowBeta");
        _qEcnThreshold = par("queueThreshold");
        _balEcnThreshold = par("balanceThreshold");
        _zeta = par("zeta"); // ewma for d_ij every source dest demand
        _minPriceRate = par("minRate");
        rateDecreaseFrequency = par("rateDecreaseFrequency");
    }

    //initialize signals with all other nodes in graph
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

    // trigger the message to compute demand to all destinations periodically
    routerMsg *computeDemandMsg = generateComputeDemandMessage();
    scheduleAt(simTime() + 0, computeDemandMsg);
}
