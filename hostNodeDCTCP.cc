#include "hostNodeDCTCP.h"

Define_Module(hostNodeDCTCP);

double _windowAlpha;
double _windowBeta;
double _qEcnThreshold;
double _qDelayEcnThreshold;
double _minDCTCPWindow;
double _balEcnThreshold;
bool _qDelayVersion;
bool _tcpVersion;
bool _isCubic;
double _cubicScalingConstant;

// knobs for enabling changing of paths
bool _changingPathsEnabled;
double _windowThresholdForChange;
int _maxPathsToConsider;
double _monitorRate;

/* generate path change trigger message every x seconds
 * that goes through all the paths and replaces the ones
 * with tiny windows
 */
routerMsg *hostNodeDCTCP::generateMonitorPathsMessage(){
    char msgname[MSGSIZE];
    sprintf(msgname, "node-%d monitorPathsMsg", myIndex());
    routerMsg *rMsg = new routerMsg(msgname);
    rMsg->setMessageType(MONITOR_PATHS_MSG);
    return rMsg;
}

/* initialization function to initialize parameters */
void hostNodeDCTCP::initialize(){
    hostNodeBase::initialize();
    
    if (myIndex() == 0) {
        // parameters
        _windowAlpha = par("windowAlpha");
        _windowBeta = par("windowBeta");
        _qEcnThreshold = par("queueThreshold");
        _qDelayEcnThreshold = par("queueDelayEcnThreshold");
        _qDelayVersion = par("DCTCPQEnabled");
        _tcpVersion = par("TCPEnabled");
        _balEcnThreshold = par("balanceThreshold");
        _minDCTCPWindow = par("minDCTCPWindow");
        _isCubic = par("cubicEnabled");
        _cubicScalingConstant = 0.4;

        // changing paths related
        _changingPathsEnabled = par("changingPathsEnabled");
        _maxPathsToConsider = par("maxPathsToConsider");
        _windowThresholdForChange = par("windowThresholdForChange");
        _monitorRate = par("pathMonitorRate");
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

    //generate monitor paths messag to start a little later in the experiment
    if (_changingPathsEnabled) {
        routerMsg *monitorMsg = generateMonitorPathsMessage();
        scheduleAt(simTime() + 150, monitorMsg);
    }
}

/* overall controller for handling messages that dispatches the right function
 * based on message type in price Scheme
 */
void hostNodeDCTCP::handleMessage(cMessage *msg) {
    routerMsg *ttmsg = check_and_cast<routerMsg *>(msg);
    if (simTime() > _simulationLength){
        auto encapMsg = (ttmsg->getEncapsulatedPacket());
        ttmsg->decapsulate();
        delete ttmsg;
        delete encapMsg;
        return;
    } 

    switch(ttmsg->getMessageType()) {
        case MONITOR_PATHS_MSG:
             if (_loggingEnabled) cout<< "[HOST "<< myIndex() 
                 <<": RECEIVED MONITOR_PATHS_MSG] "<< ttmsg->getName() << endl;
             handleMonitorPathsMessage(ttmsg);
             if (_loggingEnabled) cout<< "[AFTER HANDLING:]  "<< endl;
             break;

        default:
             hostNodeBase::handleMessage(msg);

    }
}


/* specialized ack handler that does the routine if this is DCTCP
 * algorithm. In particular, collects/updates stats for this path alone
 * and updates the window for this path based on whether the packet is marked 
 * or not
 * NOTE: acks are on the reverse path relative to the original sender
 */
void hostNodeDCTCP::handleAckMessageSpecialized(routerMsg* ttmsg) {
    ackMsg *aMsg = check_and_cast<ackMsg*>(ttmsg->getEncapsulatedPacket());
    int pathIndex = aMsg->getPathIndex();
    int destNode = ttmsg->getRoute()[0];
    int transactionId = aMsg->getTransactionId();
    double largerTxnId = aMsg->getLargerTxnId();
    PathInfo *thisPathInfo = &(nodeToShortestPathsMap[destNode][pathIndex]);

    // window update based on marked or unmarked packet for non-cubic approach
    if (!_isCubic) {
        if (aMsg->getIsMarked()) {
            thisPathInfo->window  -= _windowBeta;
            thisPathInfo->window = max(_minDCTCPWindow, thisPathInfo->window);
            thisPathInfo->markedPackets += 1;
            thisPathInfo->inSlowStart = false;
        }
        else {
            double sumWindows = 0;
            for (auto p: nodeToShortestPathsMap[destNode])
                sumWindows += p.second.window;

            if (thisPathInfo->inSlowStart) {
                thisPathInfo->window += 1;
                if (thisPathInfo->window > thisPathInfo->windowThreshold) 
                    thisPathInfo->inSlowStart = false;
            }
            else if (!_tcpVersion)
                thisPathInfo->window += _windowAlpha / sumWindows;
            else if (_tcpVersion)
                thisPathInfo->window += _windowAlpha / thisPathInfo->window;
            thisPathInfo->unmarkedPackets += 1; 
        }
    }

    // cubic's window update based on successful or unsuccessful transmission
    if (_isCubic) {
        if (aMsg->getIsSuccess() == true) {
            if (thisPathInfo->inSlowStart) {
                thisPathInfo->window += 1;
                if (thisPathInfo->window > thisPathInfo->windowThreshold) 
                    thisPathInfo->inSlowStart = false;
            } else {
                double K = cbrt(thisPathInfo->windowMax * _windowBeta / _cubicScalingConstant);
                double timeElapsed = simTime().dbl() - thisPathInfo->lastWindowReductionTime;
                double timeTerm = (timeElapsed - K) * (timeElapsed - K) * (timeElapsed - K);
                thisPathInfo->window = _cubicScalingConstant * timeTerm + thisPathInfo->windowMax;
            }
        } else {
            /* lost packet */
            if (thisPathInfo->window < thisPathInfo->windowMax) {
                thisPathInfo->windowMax = thisPathInfo->window * ( 1 - _windowBeta/2.0);
            } else {
                thisPathInfo->windowMax = thisPathInfo->window;
            }
            thisPathInfo->lastWindowReductionTime = simTime().dbl();
            thisPathInfo->window *= (1 - _windowBeta);
            thisPathInfo->inSlowStart = false;
        }
    }
    thisPathInfo->window = min(thisPathInfo->window, thisPathInfo->windowThreshold);

    // general bookkeeping to track completion state
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
                statRateCompleted[destNode] += 1;
                _transactionCompletionBySize[splitInfo->totalAmount] += 1;
                double timeTaken = simTime().dbl() - splitInfo->firstAttemptTime;
                statCompletionTimes[destNode] += timeTaken * 1000;
                _txnAvgCompTimeBySize[splitInfo->totalAmount] += timeTaken * 1000;
                recordTailCompletionTime(aMsg->getTimeSent(), splitInfo->totalAmount, timeTaken * 1000);
            }
        }
        if (splitInfo->numTotal == splitInfo->numReceived) 
            statNumCompleted[destNode] += 1; 
        thisPathInfo->statRateCompleted += 1;
        thisPathInfo->amtAcked += aMsg->getAmount();
    }

    //increment transaction amount ack on a path. 
    tuple<int,int> key = make_tuple(transactionId, pathIndex);
    if (transPathToAckState.count(key) > 0) {
        transPathToAckState[key].amtReceived += aMsg->getAmount();
        thisPathInfo->sumOfTransUnitsInFlight -= aMsg->getAmount();
    }
   
    destNodeToNumTransPending[destNode] -= 1;     
    hostNodeBase::handleAckMessage(ttmsg);
    sendMoreTransactionsOnPath(destNode, pathIndex);
}


/* handler for timeout messages that is responsible for removing messages from 
 * sender queues if they haven't been sent yet and sends explicit time out messages
 * for messages that have been sent on a path already
 */
void hostNodeDCTCP::handleTimeOutMessage(routerMsg* ttmsg) {
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

        if (iter != transList->end()) {
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

/* initialize data for a particular path with path index to the dest supplied in the arguments
 * and also fix the paths for susbequent transactions to this destination
 * and register signals that are path specific
 */
void hostNodeDCTCP::initializeThisPath(vector<int> thisPath, int pathIdx, int destNode) {
    // initialize pathInfo
    PathInfo temp = {};
    temp.path = thisPath;
    temp.window = _minDCTCPWindow;
    temp.inSlowStart = true;
    temp.sumOfTransUnitsInFlight = 0;
    temp.inUse = true;
    temp.timeStartedUse = simTime().dbl();
    temp.windowThreshold = bottleneckCapacityOnPath(thisPath); 
    temp.rttMin = (thisPath.size() - 1) * 2 * _avgDelay/1000.0;
    nodeToShortestPathsMap[destNode][pathIdx] = temp;

    // update the index of the highest path found, if you've circled back to 0, then refresh
    // the max index back to 0
    if (pathIdx > nodeToDestInfo[destNode].maxPathId || pathIdx == 0)
        nodeToDestInfo[destNode].maxPathId = pathIdx;

    // initialize signals
    simsignal_t signal;
    signal = registerSignalPerDestPath("sumOfTransUnitsInFlight", pathIdx, destNode);
    nodeToShortestPathsMap[destNode][pathIdx].sumOfTransUnitsInFlightSignal = signal;

    signal = registerSignalPerDestPath("window", pathIdx, destNode);
    nodeToShortestPathsMap[destNode][pathIdx].windowSignal = signal;

    signal = registerSignalPerDestPath("rateOfAcks", pathIdx, destNode);
    nodeToShortestPathsMap[destNode][pathIdx].rateOfAcksSignal = signal;
    
    signal = registerSignalPerDestPath("fractionMarked", pathIdx, destNode);
    nodeToShortestPathsMap[destNode][pathIdx].fractionMarkedSignal = signal;
}


/* initialize data for for the paths supplied to the destination node
 * and also fix the paths for susbequent transactions to this destination
 * and register signals that are path specific
 */
void hostNodeDCTCP::initializePathInfo(vector<vector<int>> kShortestPaths, int destNode){
    for (int pathIdx = 0; pathIdx < kShortestPaths.size(); pathIdx++){
        initializeThisPath(kShortestPaths[pathIdx], pathIdx, destNode);
    }
}

/* get maximum window size from amongs the paths considered
 */
double hostNodeDCTCP::getMaxWindowSize(unordered_map<int, PathInfo> pathList) {
    double maxWindowSize = 0;
    for (auto p = pathList.begin(); p != pathList.end(); p++) {
        int pathIndex = p->first;
        PathInfo *pInfo = &(p->second);

        if (pInfo->inUse && pInfo->windowSum/_monitorRate > maxWindowSize)
            maxWindowSize = pInfo->windowSum/_monitorRate;
    }
    return max(maxWindowSize, _minDCTCPWindow);
}

/* routine to monitor paths periodically and change them out if need be
 * in particular, marks the path as "candidate" for changing if its window is small
 * next time, it actually changes the path out if its window is still small
 * next interval it tears down the old path altogether
 */
void hostNodeDCTCP::handleMonitorPathsMessage(routerMsg* ttmsg) {
    // reschedule this message to be sent again
    if (simTime() > _simulationLength || !_changingPathsEnabled){
        delete ttmsg;
    }
    else {
        scheduleAt(simTime() + _monitorRate, ttmsg);
    }

    for (auto it = 0; it < _numHostNodes; it++){ 
        if (it != getIndex() && _destList[myIndex()].count(it) > 0) {
            if (nodeToShortestPathsMap.count(it) > 0) {
                double maxWindowSize = getMaxWindowSize(nodeToShortestPathsMap[it]);

                for (auto p = nodeToShortestPathsMap[it].begin(); p != nodeToShortestPathsMap[it].end();){
                    int pathIndex = p->first;
                    PathInfo *pInfo = &(p->second);

                    // if path is in use currently and has had a very small window for a long period
                    // replace it unless you've gone through all K paths and have no options
                    if (pInfo->inUse) {
                        double timeSincePathUse = simTime().dbl() - pInfo->timeStartedUse;
                        if (pInfo->windowSum/_monitorRate <= _windowThresholdForChange * maxWindowSize / 100.0 
                                && nodeToDestInfo[it].sumTxnsWaiting/_monitorRate > 0 
                                && timeSincePathUse > 10.0) {
                            int maxK = nodeToDestInfo[it].maxPathId;
                            if (pInfo->candidate) {
                                tuple<int, vector<int>> nextPath;

                                while (true) {
                                    nextPath =  getNextPath(getIndex(), it, maxK);
                                    int nextPathIndex = get<0>(nextPath);
                                    maxK =  (nextPathIndex == 0) ? 0 : maxK + 1;
                                    
                                    // valid new path found
                                    if (nodeToShortestPathsMap[it].count(nextPathIndex) == 0)
                                       break;
                                    
                                    // implies we came a full circle, no eligible path
                                    if (nextPathIndex == pathIndex) {
                                        pInfo->candidate = false;
                                        break;
                                    }
                                }

                                // if this path is still a candidate, then find a new path
                                if (pInfo->candidate) {
                                    initializeThisPath(get<1>(nextPath), get<0>(nextPath), it);
                                    pInfo->inUse = false;
                                }
                            }
                            else {
                                pInfo->candidate = true;
                            }
                        } else {
                            pInfo->candidate = false;
                        }
                        pInfo->windowSum = 0;
                        p++;
                    } else {
                        p = nodeToShortestPathsMap[it].erase(p);
                    }
                } 
            }
            nodeToDestInfo[it].sumTxnsWaiting = 0;
        }
    } 
}

/* handles forwarding of  transactions out of the queue
 * the way other schemes' routers do except that it marks the packet
 * if the queue is larger than the threshold, therfore mostly similar to the base code */ 
int hostNodeDCTCP::forwardTransactionMessage(routerMsg *msg, int dest, simtime_t arrivalTime) {
    transactionMsg *transMsg = check_and_cast<transactionMsg *>(msg->getEncapsulatedPacket());
    int nextDest = msg->getRoute()[msg->getHopCount()+1];
    PaymentChannel *neighbor = &(nodeToPaymentChannel[nextDest]);
    
    //don't mark yet if the packet can't be sent out
    if (neighbor->balance <= 0 || transMsg->getAmount() > neighbor->balance){
        return 0;
    }
 
    // else mark before forwarding if need be
    vector<tuple<int, double , routerMsg *, Id, simtime_t>> *q;
    q = &(neighbor->queuedTransUnits);
    if (_qDelayVersion) {
        simtime_t curQueueingDelay = simTime()  - arrivalTime;
        if (curQueueingDelay.dbl() > _qDelayEcnThreshold) {
            transMsg->setIsMarked(true); 
        }
    } 
    else {
        if (getTotalAmount(nextDest) > _qEcnThreshold) {
            transMsg->setIsMarked(true); 
        }
    }
    return hostNodeBase::forwardTransactionMessage(msg, dest, arrivalTime);
}


/* main routine for handling a new transaction under DCTCP
 * In particular, only sends out transactions if the window permits it */
void hostNodeDCTCP::handleTransactionMessageSpecialized(routerMsg* ttmsg){
    transactionMsg *transMsg = check_and_cast<transactionMsg *>(ttmsg->getEncapsulatedPacket());
    int hopcount = ttmsg->getHopCount();
    int destNode = transMsg->getReceiver();
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
                statRateArrived[destNode] += 1; 
            }
        }
        if (splitInfo->numArrived == 1) 
            statNumArrived[destNode] += 1;
    }

    // initiate paths and windows if it is a new destination
    if (nodeToShortestPathsMap.count(destNode) == 0 && ttmsg->isSelfMessage()){
        int kForPaths = _kValue;
        vector<vector<int>> kShortestRoutes = getKPaths(transMsg->getSender(),destNode, kForPaths);
        initializePathInfo(kShortestRoutes, destNode);
    }


    // at destination, add to incoming transUnits and trigger ack
    if (transMsg->getReceiver() == myIndex()) {
       handleTransactionMessage(ttmsg, false); 
    }
    else if (ttmsg->isSelfMessage()) {
        // at sender, either queue up or send on a path that allows you to send
        DestInfo* destInfo = &(nodeToDestInfo[destNode]);
            
        // use a random ordering on the path indices
        vector<int> pathIndices;
        for (auto p: nodeToShortestPathsMap[destNode]) pathIndices.push_back(p.first);
        random_shuffle(pathIndices.begin(), pathIndices.end());
       
        //send on a path if no txns queued up and timer was in the path
        if ((destInfo->transWaitingToBeSent).size() > 0) {
            pushIntoSenderQueue(destInfo, ttmsg);
            sendMoreTransactionsOnPath(destNode, -1);
        } else {
            // try the first path in this random ordering
            for (auto p : pathIndices) {
                int pathIndex = p;
                PathInfo *pathInfo = &(nodeToShortestPathsMap[destNode][pathIndex]);
                
                if (pathInfo->sumOfTransUnitsInFlight + transMsg->getAmount() <= pathInfo->window &&
                        pathInfo->inUse) {
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
                    pathInfo->statRateAttempted += 1;
                    pathInfo->sumOfTransUnitsInFlight += transMsg->getAmount();

                    // necessary for knowing what path to remove transaction in flight funds from
                    tuple<int,int> key = make_tuple(transMsg->getTransactionId(), pathIndex); 
                    transPathToAckState[key].amtSent += transMsg->getAmount();
                    
                    return;
                }
            }
            
            //transaction cannot be sent on any of the paths, queue transaction
            pushIntoSenderQueue(destInfo, ttmsg);
        }
    }
}

/* handler for the statistic message triggered every x seconds to also
 * output DCTCP scheme stats in addition to the default
 */
void hostNodeDCTCP::handleStatMessage(routerMsg* ttmsg){
    if (_signalsEnabled) {
        for (auto it = 0; it < _numHostNodes; it++){ 
            if (it != getIndex() && _destList[myIndex()].count(it) > 0) {
                if (nodeToShortestPathsMap.count(it) > 0) {
                    for (auto& p: nodeToShortestPathsMap[it]){
                        int pathIndex = p.first;
                        PathInfo *pInfo = &(p.second);
                        emit(pInfo->sumOfTransUnitsInFlightSignal, 
                                pInfo->sumOfTransUnitsInFlight);
                        emit(pInfo->windowSignal, pInfo->window);
                        emit(pInfo->rateOfAcksSignal, pInfo->amtAcked/_statRate);
                        emit(pInfo->fractionMarkedSignal, 
                                pInfo->markedPackets/(pInfo->markedPackets + pInfo->unmarkedPackets));
                        pInfo->amtAcked = 0;
                        pInfo->unmarkedPackets = 0;
                        pInfo->markedPackets = 0;
                    }
                }
                emit(numWaitingPerDestSignals[it], 
                    nodeToDestInfo[it].transWaitingToBeSent.size());
                emit(demandEstimatePerDestSignals[it], nodeToDestInfo[it].transSinceLastInterval/_statRate);
                nodeToDestInfo[it].transSinceLastInterval = 0;
            }        
        }
    } 

    // call the base method to output rest of the stats
    hostNodeBase::handleStatMessage(ttmsg);
}

/* handler for the clear state message that deals with
 * transactions that will no longer be completed
 * In particular clears out the amount inn flight on the path
 */
void hostNodeDCTCP::handleClearStateMessage(routerMsg *ttmsg) {
    // average windows over the last second
    for (auto it = 0; it < _numHostNodes; it++){ 
        if (it != getIndex() && _destList[myIndex()].count(it) > 0) {
            nodeToDestInfo[it].sumTxnsWaiting += nodeToDestInfo[it].transWaitingToBeSent.size();
            if (nodeToShortestPathsMap.count(it) > 0) {
                for (auto& p: nodeToShortestPathsMap[it]){
                    int pathIndex = p.first;
                    PathInfo *pInfo = &(p.second);
                    pInfo->windowSum += pInfo->window;
                }
            }
        }
    }

    // handle cancellations
    for ( auto it = canceledTransactions.begin(); it!= canceledTransactions.end(); it++){
        int transactionId = get<0>(*it);
        simtime_t msgArrivalTime = get<1>(*it);
        int prevNode = get<2>(*it);
        int nextNode = get<3>(*it);
        int destNode = get<4>(*it);
        
        if (simTime() > (msgArrivalTime + _maxTravelTime + 0.001)){
            // ack was not received,safely can consider this txn done
            for (auto p : nodeToShortestPathsMap[destNode]) {
                int pathIndex = p.first;
                tuple<int,int> key = make_tuple(transactionId, pathIndex);
                if (transPathToAckState.count(key) != 0) {
                    nodeToShortestPathsMap[destNode][pathIndex].sumOfTransUnitsInFlight -= 
                        (transPathToAckState[key].amtSent - transPathToAckState[key].amtReceived);
                    transPathToAckState.erase(key);

                    // treat this basiscally as one marked packet
                    nodeToShortestPathsMap[destNode][pathIndex].window  -= _windowBeta;
                    nodeToShortestPathsMap[destNode][pathIndex].window = max(_minDCTCPWindow, 
                         nodeToShortestPathsMap[destNode][pathIndex].window);
                    nodeToShortestPathsMap[destNode][pathIndex].markedPackets += 1; 
                }
            }
        }
    }

    // works fine now because timeouts start per transaction only when
    // sent out and no txn splitting
    hostNodeBase::handleClearStateMessage(ttmsg);
}

/* helper method to remove a transaction from the sender queue and send it on a particular path
 * to the given destination (or multiplexes across all paths) */
void hostNodeDCTCP::sendMoreTransactionsOnPath(int destNode, int pathId) {
    transactionMsg *transMsg;
    routerMsg *msgToSend;
    int randomIndex;

    // construct a vector with the path indices
    vector<int> pathIndices;
    for (auto p: nodeToShortestPathsMap[destNode]) pathIndices.push_back(p.first);

    //remove the transaction $tu$ at the head of the queue if one exists
    while (nodeToDestInfo[destNode].transWaitingToBeSent.size() > 0) {
        auto firstPosition = nodeToDestInfo[destNode].transWaitingToBeSent.begin();
        routerMsg *msgToSend = *firstPosition;
        transMsg = check_and_cast<transactionMsg *>(msgToSend->getEncapsulatedPacket());

        // pick a path at random to try and send on unless a path is given
        int pathIndex;
        if (pathId != -1)
            pathIndex = pathId;
        else {
            randomIndex = rand() % pathIndices.size();
            pathIndex = pathIndices[randomIndex];
        }

        PathInfo *pathInfo = &(nodeToShortestPathsMap[destNode][pathIndex]);
        if (pathInfo->sumOfTransUnitsInFlight + transMsg->getAmount() <= pathInfo->window && pathInfo->inUse) {
            // remove the transaction from queue and send it on the path
            nodeToDestInfo[destNode].transWaitingToBeSent.erase(firstPosition);
            msgToSend->setRoute(pathInfo->path);
            msgToSend->setHopCount(0);
            transMsg->setPathIndex(pathIndex);
            handleTransactionMessage(msgToSend, true /*revisit*/);

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
            pathInfo->statRateAttempted += 1;
            pathInfo->sumOfTransUnitsInFlight += transMsg->getAmount();

            // necessary for knowing what path to remove transaction in flight funds from
            tuple<int,int> key = make_tuple(transMsg->getTransactionId(), pathIndex); 
            transPathToAckState[key].amtSent += transMsg->getAmount();
        }
        else {
            // if this path is the only path you are trying and it is exhausted, return
            if (pathId != -1)
                return;

            // if no paths left to multiplex, return
            pathIndices.erase(pathIndices.begin() + randomIndex);
            if (pathIndices.size() == 0)
                return;
        }
    }
}
