#include "hostNodeWaterfilling.h"

// global parameters
// set to 1 to report exact instantaneous balances
double _ewmaFactor;

// parameters for smooth waterfilling
double _Tau;
double _Normalizer;
bool _smoothWaterfillingEnabled;
#define SMALLEST_INDIVISIBLE_UNIT 1 

Define_Module(hostNodeWaterfilling);

/* initialization function to initialize parameters */
void hostNodeWaterfilling::initialize(){
    hostNodeBase::initialize();
    
    if (myIndex() == 0) {
        // smooth waterfilling parameters
        _Tau = par("tau");
        _Normalizer = par("normalizer"); // TODO: C from discussion with Mohammad)
        _ewmaFactor = 1; // EWMA factor for balance information on probes
        _smoothWaterfillingEnabled = par("smoothWaterfillingEnabled");
    }

    //initialize signals with all other nodes in graph
    // that there is demand for
    for (int i = 0; i < _numHostNodes; ++i) {
        if (_destList[myIndex()].count(i) > 0) {
            simsignal_t signal;
            signal = registerSignalPerDest("numWaiting", i, "_Total");
            numWaitingPerDestSignals[i] = signal;
        }
    }
}

/* generates the probe message for a particular destination and a particur path
 * identified by the list of hops and the path index
 */
routerMsg* hostNodeWaterfilling::generateProbeMessage(int destNode, int pathIdx, vector<int> path){
    char msgname[MSGSIZE];
    sprintf(msgname, "tic-%d-to-%d probeMsg [idx %d]", myIndex(), destNode, pathIdx);
    probeMsg *pMsg = new probeMsg(msgname);
    pMsg->setSender(myIndex());
    pMsg->setPathIndex(pathIdx);
    pMsg->setReceiver(destNode);
    pMsg->setIsReversed(false);
    vector<double> pathBalances;
    pMsg->setPathBalances(pathBalances);
    pMsg->setPath(path);

    sprintf(msgname, "tic-%d-to-%d router-probeMsg [idx %d]", myIndex(), destNode, pathIdx);
    routerMsg *rMsg = new routerMsg(msgname);
    rMsg->setRoute(path);
    rMsg->setHopCount(0);
    rMsg->setMessageType(PROBE_MSG);
    rMsg->encapsulate(pMsg);
    return rMsg;
}



/* overall controller for handling messages that dispatches the right function
 * based on message type in waterfilling
 */
void hostNodeWaterfilling::handleMessage(cMessage *msg) {
    routerMsg *ttmsg = check_and_cast<routerMsg *>(msg);
    if (simTime() > _simulationLength){
        auto encapMsg = (ttmsg->getEncapsulatedPacket());
        ttmsg->decapsulate();
        delete ttmsg;
        delete encapMsg;
        return;
    } 

    switch(ttmsg->getMessageType()) {
        case PROBE_MSG:
             if (_loggingEnabled) cout<< "[HOST "<< myIndex() 
                 <<": RECEIVED PROBE MSG] "<< ttmsg->getName() << endl;
             handleProbeMessage(ttmsg);
             if (_loggingEnabled) cout<< "[AFTER HANDLING:]  "<< endl;
             break;
        default:
             hostNodeBase::handleMessage(msg);
    }
}

/* main routine for handling transaction messages for waterfilling
 * that initiates probes and splits transactions according to latest probes
 */
void hostNodeWaterfilling::handleTransactionMessageSpecialized(routerMsg* ttmsg){
    transactionMsg *transMsg = check_and_cast<transactionMsg *>(ttmsg->getEncapsulatedPacket());
    int hopCount = ttmsg->getHopCount();
    int destNode = transMsg->getReceiver();
    int nextNode = ttmsg->getRoute()[hopCount+1];
    int transactionId = transMsg->getTransactionId();
    double waitTime = _maxTravelTime;
    
    // txn at receiver
    if (ttmsg->getRoute()[hopCount] == destNode) {
       handleTransactionMessage(ttmsg, false); 
    }
    else { 
        // transaction received at sender
        // If transaction seen for first time, update stats.
        if (simTime() == transMsg->getTimeSent()) {
            SplitState* splitInfo = &(_numSplits[myIndex()][transMsg->getLargerTxnId()]);
            splitInfo->numArrived += 1;
            
            if (transMsg->getTimeSent() >= _transStatStart && transMsg->getTimeSent() <= _transStatEnd) {
                if (transMsg->getIsAttempted() == false)
                    statAmtArrived[destNode] += transMsg->getAmount();
                if (splitInfo->numArrived == 1) {
                    statRateArrived[destNode] += 1;
                }
            }
            destNodeToNumTransPending[destNode] += 1; 
            if (splitInfo->numArrived == 1) 
                statNumArrived[destNode] += 1; 
            
            AckState * s = new AckState();
            s->amtReceived = 0;
            s->amtSent = transMsg->getAmount();
            transToAmtLeftToComplete[transMsg->getTransactionId()] = *s;
        }
        
        // Compute paths and initialize probes if destination hasn't been encountered
        if (nodeToShortestPathsMap.count(destNode) == 0 ){
            vector<vector<int>> kShortestRoutes = getKPaths(transMsg->getSender(),destNode, _kValue);
            initializeProbes(kShortestRoutes, destNode);
            scheduleAt(simTime() + waitTime, ttmsg);
            return;
        }
        else {
            // if all probes from destination are recent enough and txn hasn't timed out, 
            // send transaction on one or more paths.
            bool recent = probesRecent(nodeToShortestPathsMap[destNode]);
            if (recent){
                if ((!_timeoutEnabled) || (simTime() < (transMsg->getTimeSent() + transMsg->getTimeOut()))) { 
                    attemptTransactionOnBestPath(ttmsg, !transMsg->getIsAttempted());
                    double amtRemaining = transMsg->getAmount();
                    if (amtRemaining > 0 + _epsilon) {
                        pushIntoSenderQueue(&(nodeToDestInfo[destNode]), ttmsg);
                    }
                    else {
                        ttmsg->decapsulate();
                        delete transMsg;
                        delete ttmsg;
                    }
                }
                else {
                    // transaction timed out
                    statNumTimedOut[destNode] += 1;
                    statNumTimedOutAtSender[destNode] += 1; 
                    ttmsg->decapsulate();
                    delete transMsg;
                    delete ttmsg;
                }
                return;
            }
            else { 
                // need more recent probes
                if (destNodeToNumTransPending[destNode] > 0) {
                    restartProbes(destNode);
                }
                pushIntoSenderQueue(&(nodeToDestInfo[destNode]), ttmsg);
                return;
            }
        }
    }
}

/* handles the special time out mechanism for waterfilling which is responsible
 * for sending time out messages on all paths that may have seen this txn and 
 * marking the txn as cancelled
 */
void hostNodeWaterfilling::handleTimeOutMessage(routerMsg* ttmsg){
    timeOutMsg *toutMsg = check_and_cast<timeOutMsg *>(ttmsg->getEncapsulatedPacket());
    if (ttmsg->isSelfMessage()) { 
        //is at the sender
        int transactionId = toutMsg->getTransactionId();
        int destination = toutMsg->getReceiver();
        multiset<routerMsg*, transCompare>* transList = &(nodeToDestInfo[destination].transWaitingToBeSent);

        // if transaction was successful don't do anything more
        if (successfulDoNotSendTimeOut.count(transactionId) > 0) {
            successfulDoNotSendTimeOut.erase(toutMsg->getTransactionId());
            ttmsg->decapsulate();
            delete toutMsg;
            delete ttmsg;
            return;
        }

        // check if txn is still in just sender queue
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
       
        for (auto p : (nodeToShortestPathsMap[destination])){
            int pathIndex = p.first;
            tuple<int,int> key = make_tuple(transactionId, pathIndex);
            
            if (transPathToAckState.count(key) > 0 && 
                    transPathToAckState[key].amtSent != transPathToAckState[key].amtReceived) {
                routerMsg* waterTimeOutMsg = generateTimeOutMessageForPath(
                    nodeToShortestPathsMap[destination][p.first].path, 
                    transactionId, destination);
                int nextNode = (waterTimeOutMsg->getRoute())[waterTimeOutMsg->getHopCount()+1];
                CanceledTrans ct = make_tuple(toutMsg->getTransactionId(), 
                        simTime(), -1, nextNode, destination);
                canceledTransactions.insert(ct);
                forwardMessage(waterTimeOutMsg);
            }
            else {
                transPathToAckState.erase(key);
            }
        }
        delete ttmsg;
    }
    else{
        // at the receiver
        CanceledTrans ct = make_tuple(toutMsg->getTransactionId(),simTime(),
                (ttmsg->getRoute())[ttmsg->getHopCount()-1], -1, toutMsg->getReceiver());
        canceledTransactions.insert(ct);
        ttmsg->decapsulate();
        delete toutMsg;
        delete ttmsg;
    }
}



/* handle Waterfilling probe Message
 * if it back at the sender, then update the bottleneck balances for this path 
 * otherwise, reverse and send back to sender
 */
void hostNodeWaterfilling::handleProbeMessage(routerMsg* ttmsg){
    probeMsg *pMsg = check_and_cast<probeMsg *>(ttmsg->getEncapsulatedPacket());
    if (simTime()> _simulationLength ){
       ttmsg->decapsulate();
       delete pMsg;
       delete ttmsg;
       return;
    }

    bool isReversed = pMsg->getIsReversed();
    int nextDest = ttmsg->getRoute()[ttmsg->getHopCount()+1];
    
    if (isReversed == true) { 
        int pathIdx = pMsg->getPathIndex();
        int destNode = pMsg->getReceiver();
        vector<double> pathBalances = pMsg->getPathBalances();
        double bottleneck = minVectorElemDouble(pathBalances);

        PathInfo* p = &(nodeToShortestPathsMap[destNode][pathIdx]);
        assert(p->path == pMsg->getPath());
        
        // update state for this path -time of probe, balance
        p->lastUpdated = simTime();
        p->bottleneck = bottleneck;
        p->pathBalances = pathBalances;
        p->isProbeOutstanding = false;

        // see if this is the path with the new max available balance
        double availBal = p->bottleneck - p->sumOfTransUnitsInFlight;
        if (availBal > nodeToDestInfo[destNode].highestBottleneckBalance) {
            nodeToDestInfo[destNode].highestBottleneckBalance = availBal;
            nodeToDestInfo[destNode].highestBottleneckPathIndex = pathIdx;
        }
        
        if (destNodeToNumTransPending[destNode] > 0){
            // service first transaction on path
            if (nodeToDestInfo[destNode].transWaitingToBeSent.size() > 0 && availBal > 0) {
                auto firstPosition = nodeToDestInfo[destNode].transWaitingToBeSent.begin();
                routerMsg *nextTrans = *firstPosition;
                nodeToDestInfo[destNode].transWaitingToBeSent.erase(firstPosition);
                handleTransactionMessageSpecialized(nextTrans);
            }
            
            //reset the probe message to send again
           nodeToShortestPathsMap[destNode][pathIdx].isProbeOutstanding = true;
           vector<int> route = ttmsg->getRoute();
           reverse(route.begin(), route.end());
           vector<double> tempPathBal = {};

           ttmsg->setRoute(route);
           ttmsg->setHopCount(0);
           pMsg->setIsReversed(false); 
           pMsg->setPathBalances(tempPathBal);
           forwardProbeMessage(ttmsg);
        }
        else{
           ttmsg->decapsulate();
           delete pMsg;
           delete ttmsg;
        }
    }
    else {
        //reverse and send message again
        pMsg->setIsReversed(true);
        ttmsg->setHopCount(0);
        vector<int> route = ttmsg->getRoute();
        reverse(route.begin(), route.end());
        ttmsg->setRoute(route);
        forwardProbeMessage(ttmsg);
    }
}

/* handler that clears additional state particular to waterfilling 
 * when a cancelled transaction is deemed no longer completeable
 * in particular it clears the state that tracks how much of a
 * transaction is still pending
 * calls the base class's handler after its own handler
 */
void hostNodeWaterfilling::handleClearStateMessage(routerMsg *ttmsg) {
    for ( auto it = canceledTransactions.begin(); it!= canceledTransactions.end(); it++){
        int transactionId = get<0>(*it);
        simtime_t msgArrivalTime = get<1>(*it);
        int prevNode = get<2>(*it);
        int nextNode = get<3>(*it);
        int destNode = get<4>(*it);
        
        if (simTime() > (msgArrivalTime + _maxTravelTime + 1)){
            for (auto p : nodeToShortestPathsMap[destNode]) {
                int pathIndex = p.first;
                tuple<int,int> key = make_tuple(transactionId, pathIndex);
                if (transPathToAckState.count(key) != 0) {
                    PathInfo *pathInfo = &(nodeToShortestPathsMap[destNode][pathIndex]);
                    pathInfo->sumOfTransUnitsInFlight -= 
                        (transPathToAckState[key].amtSent - transPathToAckState[key].amtReceived);
                    transPathToAckState.erase(key);
                    
                    double availBal = pathInfo->bottleneck - pathInfo->sumOfTransUnitsInFlight;
                    if (availBal > nodeToDestInfo[destNode].highestBottleneckBalance) {
                        nodeToDestInfo[destNode].highestBottleneckBalance = availBal;
                        nodeToDestInfo[destNode].highestBottleneckPathIndex = pathIndex;
                    }
                }
            }
        }
    }
    hostNodeBase::handleClearStateMessage(ttmsg);
}


/* handles to logic for ack messages in the presence of timeouts
 * in particular, removes the transaction from the cancelled txns
 * to mark that it has been received 
 * it uses the transAmtSent vs Received to detect if txn is complete
 * and therefore is different from the base class 
 */
void hostNodeWaterfilling::handleAckMessageTimeOut(routerMsg* ttmsg){
    ackMsg *aMsg = check_and_cast<ackMsg *>(ttmsg->getEncapsulatedPacket());
    int transactionId = aMsg->getTransactionId();

    if (aMsg->getIsSuccess()) {
        double totalAmtReceived = (transToAmtLeftToComplete[transactionId]).amtReceived +
            aMsg->getAmount();
        if (totalAmtReceived != transToAmtLeftToComplete[transactionId].amtSent) 
            return;
        auto iter = canceledTransactions.find(make_tuple(transactionId, 0, 0, 0, 0));
        if (iter != canceledTransactions.end()) {
            canceledTransactions.erase(iter);
        }
        successfulDoNotSendTimeOut.insert(aMsg->getTransactionId());
    }
}

/* specialized ack handler that does the routine if this is waterfilling 
 * algorithm. In particular, collects/updates stats for this path alone
 * NOTE: acks are on the reverse path relative to the original sender
 */
void hostNodeWaterfilling::handleAckMessageSpecialized(routerMsg* ttmsg) {
    ackMsg *aMsg = check_and_cast<ackMsg *>(ttmsg->getEncapsulatedPacket());
    int receiver = aMsg->getReceiver();
    int pathIndex = aMsg->getPathIndex();
    int transactionId = aMsg->getTransactionId();
    double largerTxnId = aMsg->getLargerTxnId();
    PathInfo *pathInfo = &(nodeToShortestPathsMap[receiver][pathIndex]);
    
    if (transToAmtLeftToComplete.count(transactionId) == 0){
        cout << "error, transaction " << transactionId 
          <<" htlc index:" << aMsg->getHtlcIndex() 
          << " acknowledged at time " << simTime() 
          << " wasn't written to transToAmtLeftToComplete" << endl;
    }
    else {
        // update stats if successful
        if (aMsg->getIsSuccess()) { 
            pathInfo->statRateCompleted += 1;
            (transToAmtLeftToComplete[transactionId]).amtReceived += aMsg->getAmount();
            if (aMsg->getTimeSent() >= _transStatStart 
                    && aMsg->getTimeSent() <= _transStatEnd) { 
                statAmtCompleted[receiver] += aMsg->getAmount();
            }

            if (transToAmtLeftToComplete[transactionId].amtReceived > 
                    transToAmtLeftToComplete[transactionId].amtSent - _epsilon) {
                SplitState* splitInfo = &(_numSplits[myIndex()][largerTxnId]);
                splitInfo->numReceived += 1;

                if (aMsg->getTimeSent() >= _transStatStart && 
                        aMsg->getTimeSent() <= _transStatEnd) {
                    if (splitInfo->numTotal == splitInfo->numReceived) {
                        statRateCompleted[receiver] += 1;
                        _transactionCompletionBySize[splitInfo->totalAmount] += 1;
                        double timeTaken = simTime().dbl() - splitInfo->firstAttemptTime;
                        statCompletionTimes[receiver] += timeTaken * 1000;
                        _txnAvgCompTimeBySize[splitInfo->totalAmount] += timeTaken * 1000;
                        recordTailCompletionTime(aMsg->getTimeSent(), splitInfo->totalAmount, timeTaken * 1000);
                    }
                }
                if (splitInfo->numTotal == splitInfo->numReceived) 
                    statNumCompleted[receiver] += 1;
                
                // erase transaction from map 
                // NOTE: still keeping it in the per path map (transPathToAckState)
                // to identify that timeout needn't be sent
                transToAmtLeftToComplete.erase(aMsg->getTransactionId());
                destNodeToNumTransPending[receiver] -= 1;
            }
        } 
        else {
            // make sure transaction isn't cancelled yet
            auto iter = canceledTransactions.find(make_tuple(transactionId, 0, 0, 0, 0));
            if (iter != canceledTransactions.end()) {
                if (aMsg->getTimeSent() >= _transStatStart && aMsg->getTimeSent() <= _transStatEnd)
                    statAmtFailed[receiver] += aMsg->getAmount();
            } 
            else {
                // requeue transaction
                routerMsg *duplicateTrans = generateDuplicateTransactionMessage(aMsg);
                pushIntoSenderQueue(&(nodeToDestInfo[receiver]), duplicateTrans);
            }
        }
       
        //increment transaction amount acked on a path, so that we know not to send timeouts 
        // if nothing is in excess on the path
        tuple<int,int> key = make_tuple(transactionId, pathIndex);
        transPathToAckState[key].amtReceived += aMsg->getAmount();
        pathInfo->sumOfTransUnitsInFlight -= aMsg->getAmount();

        // update highest bottleneck balance path
        double thisPathAvailBal = pathInfo->bottleneck - pathInfo->sumOfTransUnitsInFlight;
        if (thisPathAvailBal > nodeToDestInfo[receiver].highestBottleneckBalance) {
            nodeToDestInfo[receiver].highestBottleneckBalance = thisPathAvailBal;
            nodeToDestInfo[receiver].highestBottleneckPathIndex = pathIndex;
        }
    }
    hostNodeBase::handleAckMessage(ttmsg);
}

/* handler for the statistic message triggered every x seconds to also
 * output the wf stats in addition to the default
 */
void hostNodeWaterfilling::handleStatMessage(routerMsg* ttmsg){
    if (_signalsEnabled) {     
        // per destination statistics
        for (auto it = 0; it < _numHostNodes; it++){ 
            if (it != getIndex() && _destList[myIndex()].count(it) > 0) {
                if (nodeToShortestPathsMap.count(it) > 0) {
                    for (auto p: nodeToShortestPathsMap[it]){
                        int pathIndex = p.first;
                        PathInfo *pInfo = &(p.second);
                        emit(pInfo->bottleneckPerDestPerPathSignal, pInfo->bottleneck);
                        emit(pInfo->windowSignal, pInfo->bottleneck);
                        if (_smoothWaterfillingEnabled)
                            emit(pInfo->probabilityPerDestPerPathSignal, pInfo->probability);
                        emit(pInfo->sumOfTransUnitsInFlightSignal, 
                                pInfo->sumOfTransUnitsInFlight);
                    }
                }
                emit(numWaitingPerDestSignals[it], 
                    nodeToDestInfo[it].transWaitingToBeSent.size());
            }        
        }
    } 
    hostNodeBase::handleStatMessage(ttmsg);
}


/* initialize probes along the paths specified to the destination node
 * and set up all the state in the table that maintains bottleneck balance
 * information across all paths to all destinations
 * also responsible for initializing signals
 */
void hostNodeWaterfilling::initializeProbes(vector<vector<int>> kShortestPaths, int destNode){ 
    destNodeToLastMeasurementTime[destNode] = 0.0;

    for (int pathIdx = 0; pathIdx < kShortestPaths.size(); pathIdx++){
        PathInfo temp = {};
        nodeToShortestPathsMap[destNode][pathIdx] = temp;
        nodeToShortestPathsMap[destNode][pathIdx].path = kShortestPaths[pathIdx];
        nodeToShortestPathsMap[destNode][pathIdx].probability = 1.0 / kShortestPaths.size();

        //initialize signals
        if (_signalsEnabled) {
            simsignal_t signal;
            signal = registerSignalPerDestPath("rateCompleted", pathIdx, destNode);
            nodeToShortestPathsMap[destNode][pathIdx].rateCompletedPerDestPerPathSignal = signal;

            if (_smoothWaterfillingEnabled) {
                signal = registerSignalPerDestPath("probability", pathIdx, destNode);
                nodeToShortestPathsMap[destNode][pathIdx].probabilityPerDestPerPathSignal = signal;
            }

            signal = registerSignalPerDestPath("rateAttempted", pathIdx, destNode);
            nodeToShortestPathsMap[destNode][pathIdx].rateAttemptedPerDestPerPathSignal = signal;
    
            signal = registerSignalPerDestPath("sumOfTransUnitsInFlight", pathIdx, destNode);
            nodeToShortestPathsMap[destNode][pathIdx].sumOfTransUnitsInFlightSignal = signal;
    
            signal = registerSignalPerDestPath("window", pathIdx, destNode);
            nodeToShortestPathsMap[destNode][pathIdx].windowSignal = signal;
            
            signal = registerSignalPerDestPath("bottleneck", pathIdx, destNode);
            nodeToShortestPathsMap[destNode][pathIdx].bottleneckPerDestPerPathSignal = signal;
        }

        // generate a probe message on this path
        routerMsg * msg = generateProbeMessage(destNode, pathIdx, kShortestPaths[pathIdx]);
        nodeToShortestPathsMap[destNode][pathIdx].isProbeOutstanding = true;
        forwardProbeMessage(msg);
    }
}



/* restart waterfilling probes once they have been stopped to a particular destination
 * TODO: might end up leaving multiple probes in flight to some destinations, but that's okay 
 * for now.
 */
void hostNodeWaterfilling::restartProbes(int destNode) {
    for (auto p: nodeToShortestPathsMap[destNode] ){
        PathInfo pathInformation = p.second;
        if (nodeToShortestPathsMap[destNode][p.first].isProbeOutstanding == false) {
            nodeToShortestPathsMap[destNode][p.first].isProbeOutstanding = true;
            routerMsg * msg = generateProbeMessage(destNode, p.first, p.second.path);
            forwardProbeMessage(msg);
        }
    }
}


/* forwards probe messages for waterfilling alone that appends the current balance
 * to the list of balances
 */
void hostNodeWaterfilling::forwardProbeMessage(routerMsg *msg){
    // Increment hop count.
    msg->setHopCount(msg->getHopCount()+1);
    //use hopCount to find next destination
    int nextDest = msg->getRoute()[msg->getHopCount()];

    probeMsg *pMsg = check_and_cast<probeMsg *>(msg->getEncapsulatedPacket());
    if (pMsg->getIsReversed() == false && !_rebalancingEnabled){
        vector<double> *pathBalances = & ( pMsg->getPathBalances());
        (*pathBalances).push_back(nodeToPaymentChannel[nextDest].balanceEWMA);
    }

   if (_loggingEnabled) cout << "forwarding " << msg->getMessageType() << " at " 
       << simTime() << endl;
   send(msg, nodeToPaymentChannel[nextDest].gate);
}

/* simplified waterfilling logic that assumes transaction is already split and sends it fully only on the 
 * path with maximum bottleneck - inflight balance */
void hostNodeWaterfilling::attemptTransactionOnBestPath(routerMsg * ttmsg, bool firstAttempt){
    transactionMsg *transMsg = check_and_cast<transactionMsg *>(ttmsg->getEncapsulatedPacket());
    int destNode = transMsg->getReceiver();
    double remainingAmt = transMsg->getAmount();
    transMsg->setIsAttempted(true);
    
    // fill up priority queue of balances
    double highestBal = -1;
    int highestBalIdx = -1;
    for (auto iter: nodeToShortestPathsMap[destNode] ){
        int key = iter.first;
        double bottleneck = (iter.second).bottleneck;
        double inflight = (iter.second).sumOfTransUnitsInFlight;
        double availBal = bottleneck - inflight;

        if (availBal >= highestBal) {
            highestBal = availBal;
            highestBalIdx = key;
        }
    }

    // accounting
    SplitState* splitInfo = &(_numSplits[myIndex()][transMsg->getLargerTxnId()]);
    bool firstLargerAttempt = (splitInfo->firstAttemptTime == -1);
    if (splitInfo->firstAttemptTime == -1) {
        splitInfo->firstAttemptTime = simTime().dbl();
    }
    if (transMsg->getTimeSent() >= _transStatStart && 
            transMsg->getTimeSent() <= _transStatEnd) {
        if (firstAttempt && firstLargerAttempt) { 
            statRateAttempted[destNode] = statRateAttempted[destNode] + 1;
        }
        if (firstAttempt) {
            statAmtAttempted[destNode] += transMsg->getAmount();
        }
    }

    if (highestBal <= 0) 
        return;

    // update state and send this out on the path with higest bal
    tuple<int,int> key = make_tuple(transMsg->getTransactionId(), highestBalIdx);
            
    //update the data structure keeping track of how much sent and received on each path
    if (transPathToAckState.count(key) == 0) {
        AckState temp = {};
        temp.amtSent = remainingAmt;
        temp.amtReceived = 0;
        transPathToAckState[key] = temp;
    }
    else {
        transPathToAckState[key].amtSent =  transPathToAckState[key].amtSent + remainingAmt;
    }
            
    PathInfo *pathInfo = &(nodeToShortestPathsMap[destNode][highestBalIdx]);
    routerMsg* waterMsg = generateTransactionMessageForPath(remainingAmt, 
         pathInfo->path, highestBalIdx, transMsg);
    
    // increment numAttempted per path
    pathInfo->statRateAttempted += 1;
    handleTransactionMessage(waterMsg, true/*revisit*/);
    
    // incrementInFlight balance and update highest bal index
    pathInfo->sumOfTransUnitsInFlight += remainingAmt;
    nodeToDestInfo[destNode].highestBottleneckBalance -= remainingAmt;
    transMsg->setAmount(0);
}



/* core waterfilling logic in deciding how to split a transaction across paths
 * based on the bottleneck balances on each of those paths
 * For now, in hte absence of splitting, transaction is sent in entirety either
 * on the path with highest bottleneck balance or the paths are sampled with a
 * probabilities based on the bottleneck balances in the smooth waterfilling 
 * case
 */
void hostNodeWaterfilling::splitTransactionForWaterfilling(routerMsg * ttmsg, bool firstAttempt){
    transactionMsg *transMsg = check_and_cast<transactionMsg *>(ttmsg->getEncapsulatedPacket());
    int destNode = transMsg->getReceiver();
    double remainingAmt = transMsg->getAmount();
    transMsg->setIsAttempted(true);
    
    // if you want to choose at random between paths
    bool randomChoice = false; 
    
    unordered_map<int, double> pathMap = {}; //key is pathIdx, double is amt
    vector<double> bottleneckList;
   
    // fill up priority queue of balances
    priority_queue<pair<double,int>> pq;
    if (_loggingEnabled) cout << "bottleneck for node " <<  getIndex();
    for (auto iter: nodeToShortestPathsMap[destNode] ){
        int key = iter.first;
        double bottleneck = (iter.second).bottleneck;
        double inflight = (iter.second).sumOfTransUnitsInFlight;
        bottleneckList.push_back(bottleneck);
        if (_loggingEnabled) cout << bottleneck - inflight << " (" << key  << "," 
            << iter.second.lastUpdated<<"), ";
        
        pq.push(make_pair(bottleneck - inflight, key)); 
    }
    if (_loggingEnabled) cout << endl;

    double highestBal;
    double secHighestBal;
    double diffToSend;
    double amtToSend;
    int highestBalIdx;
    int numPaths = nodeToShortestPathsMap[destNode].size();

    if (randomChoice) {
        vector<double> probabilities (numPaths, 1.0/numPaths);
        int pathIndex = sampleFromDistribution(probabilities);
        pathMap[pathIndex] = pathMap[pathIndex] + remainingAmt;
        remainingAmt = 0;
    } 
    else if (_smoothWaterfillingEnabled) {
        highestBal = get<0>(pq.top());
        if (highestBal >= 0) {
            int pathIndex = updatePathProbabilities(bottleneckList, destNode);
            pathMap[pathIndex] = pathMap[pathIndex] + remainingAmt;
            remainingAmt = 0;
        }
    }
    else {
        // normal waterfilling - start filling with the path
        // with highest bottleneck balance and fill it till you get to 
        // the next path and so on
        if (pq.size() == 0) {
            cout << "PATHS NOT FOUND to " << destNode << "WHEN IT SHOULD HAVE BEEN";
            throw std::exception();
        }
        
        while(pq.size() > 0 && remainingAmt >= SMALLEST_INDIVISIBLE_UNIT){
            highestBal = get<0>(pq.top());
            if (highestBal <= 0) 
                break;
            
            highestBalIdx = get<1>(pq.top());
            pq.pop();

            if (pq.size() == 0) {
                secHighestBal = 0;
            }
            else {
                secHighestBal = get<0>(pq.top());
            }
            diffToSend = highestBal - secHighestBal;
            pathMap[highestBalIdx] = 0.0;
            
            double amtAddedInThisRound = 0.0;
            double maxForThisRound = pathMap.size() * diffToSend;
            while (remainingAmt > _epsilon && amtAddedInThisRound < maxForThisRound) {
                for (auto p: pathMap){
                    pathMap[p.first] += SMALLEST_INDIVISIBLE_UNIT;
                    remainingAmt = remainingAmt - SMALLEST_INDIVISIBLE_UNIT;
                    amtAddedInThisRound += SMALLEST_INDIVISIBLE_UNIT;
                    if (remainingAmt < _epsilon || amtAddedInThisRound >= maxForThisRound) {
                        break;
                    }
                }
            }
        }
   
        // send all of the remaining amount beyond the indivisible unit on one path
        // the highest bal path as long as it has non zero balance
        if (remainingAmt > _epsilon  && pq.size()>0 ) {
            highestBal = get<0>(pq.top());
            highestBalIdx = get<1>(pq.top());
               
            if (highestBal > 0) {
                pathMap[highestBalIdx] = pathMap[highestBalIdx] + remainingAmt;
                remainingAmt = 0;
            }
        }

        if (remainingAmt < _epsilon) 
            remainingAmt = 0;
    }

    // accounting
    SplitState* splitInfo = &(_numSplits[myIndex()][transMsg->getLargerTxnId()]);
    bool firstLargerAttempt = (splitInfo->firstAttemptTime == -1);
    if (splitInfo->firstAttemptTime == -1) {
        splitInfo->firstAttemptTime = simTime().dbl();
    }
    if (transMsg->getTimeSent() >= _transStatStart && 
            transMsg->getTimeSent() <= _transStatEnd) {
        if (firstAttempt && firstLargerAttempt) { 
            statRateAttempted[destNode] = statRateAttempted[destNode] + 1;
        }
        if (firstAttempt) {
            statAmtAttempted[destNode] += transMsg->getAmount();
        }
    }

    if (remainingAmt > transMsg->getAmount()) {
        cout << "remaining amount magically became higher than original amount" << endl;
        throw std::exception();
    }
    transMsg->setAmount(remainingAmt);
    
    for (auto p: pathMap){
        int pathIndex = p.first;
        double amtOnPath = p.second;
        if (amtOnPath > 0){
            tuple<int,int> key = make_tuple(transMsg->getTransactionId(), pathIndex);
            //update the data structure keeping track of how much sent and received on each path
            if (transPathToAckState.count(key) == 0) {
                AckState temp = {};
                temp.amtSent = amtOnPath;
                temp.amtReceived = 0;
                transPathToAckState[key] = temp;
            }
            else {
                transPathToAckState[key].amtSent =  transPathToAckState[key].amtSent + amtOnPath;
            }
            PathInfo *pathInfo = &(nodeToShortestPathsMap[destNode][pathIndex]);
            routerMsg* waterMsg = generateTransactionMessageForPath(amtOnPath, 
                 pathInfo->path, pathIndex, transMsg);
            pathInfo->statRateAttempted += 1;
            pathInfo->sumOfTransUnitsInFlight += p.second;

            handleTransactionMessage(waterMsg, true/*revisit*/);
        }
    }
}


/* computes the updated path probabilities based on the current state of 
 * bottleneck link balances and returns the next path index to send the transaction 
 * on in accordance to the latest rate
 * acts as a helper for smooth waterfilling
 */
int hostNodeWaterfilling::updatePathProbabilities(vector<double> bottleneckBalances, int destNode) {
    double averageBottleneck = accumulate(bottleneckBalances.begin(), 
            bottleneckBalances.end(), 0.0)/bottleneckBalances.size(); 
                
    double timeSinceLastTxn = simTime().dbl() - destNodeToLastMeasurementTime[destNode];
    destNodeToLastMeasurementTime[destNode] = simTime().dbl();

    // compute new porbabailities based on adjustment factor and expression
    vector<double> probabilities;
    int i = 0;
    for (auto b : bottleneckBalances) {
        probabilities.push_back(nodeToShortestPathsMap[destNode][i].probability + 
            (1 - exp(-1 * timeSinceLastTxn/_Tau))*(b - averageBottleneck)/_Normalizer);
        probabilities[i] = max(0.0, probabilities[i]);
        i++;
    }
    double sumProbabilities = accumulate(probabilities.begin(), probabilities.end(), 0.0); 
    
    // normalize them to 1 and update the stored probabilities
    for (i = 0; i < probabilities.size(); i++) {
        probabilities[i] /= sumProbabilities;
        nodeToShortestPathsMap[destNode][i].probability = probabilities[i];
    }
    return sampleFromDistribution(probabilities);
}



