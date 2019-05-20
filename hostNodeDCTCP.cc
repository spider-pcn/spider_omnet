#include "hostNodeDCTCP.h"

Define_Module(hostNodeDCTCP);

double _windowAlpha;
double _windowBeta;
double _qEcnThreshold;

/* initialization function to initialize parameters */
void hostNodeDCTCP::initialize(){
    hostNodeBase::initialize();
    
    if (myIndex() == 0) {
        // parameters
        _windowAlpha = 0.2;
        _windowBeta = 0.5;
        _qEcnThreshold = 20; 
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

    // window update based on marked or unmarked packet
    if (aMsg->getIsMarked())
        nodeToShortestPathsMap[destNode][pathIndex].window  -= _windowBeta;
    else {
        double sumWindows = 0;
        for (auto p: nodeToShortestPathsMap[destNode])
            sumWindows += p.second.window;
        nodeToShortestPathsMap[destNode][pathIndex].window += _windowAlpha / sumWindows;
    }

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
                statNumCompleted[destNode] += 1; 
                statRateCompleted[destNode] += 1;
                _transactionCompletionBySize[splitInfo->totalAmount] += 1;
                double timeTaken = simTime().dbl() - splitInfo->firstAttemptTime;
                statCompletionTimes[destNode] += timeTaken * 1000;
            }
        }
        nodeToShortestPathsMap[destNode][pathIndex].statRateCompleted += 1;
    }

    //increment transaction amount ack on a path. 
    tuple<int,int> key = make_tuple(transactionId, pathIndex);
    transPathToAckState[key].amtReceived += aMsg->getAmount();
    
    nodeToShortestPathsMap[destNode][pathIndex].sumOfTransUnitsInFlight -= aMsg->getAmount();
    destNodeToNumTransPending[destNode] -= 1;     
    hostNodeBase::handleAckMessage(ttmsg);

    sendNextTransactionOnPath(destNode, pathIndex);
}


/* handler for timeout messages that is responsible for removing messages from 
 * sender queues if they haven't been sent yet and sends explicit time out messages
 * for messages that have been sent on a path already
 */
void hostNodeDCTCP::handleTimeOutMessage(routerMsg* ttmsg) {
    timeOutMsg *toutMsg = check_and_cast<timeOutMsg *>(ttmsg->getEncapsulatedPacket());
    int destination = toutMsg->getReceiver();
    int transactionId = toutMsg->getTransactionId();
    deque<routerMsg*> *transList = &(nodeToDestInfo[destination].transWaitingToBeSent);
    
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
        auto iter = find_if(transList->end(),
           transList->begin(),
           [&transactionId](const routerMsg* p)
           { transactionMsg *transMsg = check_and_cast<transactionMsg *>(p->getEncapsulatedPacket());
             return transMsg->getTransactionId()  == transactionId; });

        if (iter!=transList->begin()) {
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
                // TODO: what if a transaction on two different paths have same next hop?
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
                statNumArrived[destNode] += 1;
                statRateArrived[destNode] += 1; 
            }
        }
    }
    
    // at destination, add to incoming transUnits and trigger ack
    if (transMsg->getReceiver() == myIndex()) {
        int prevNode = ttmsg->getRoute()[ttmsg->getHopCount() - 1];
        map<Id, double> *incomingTransUnits = &(nodeToPaymentChannel[prevNode].incomingTransUnits);
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
       
        //send on a path if no txns queued up and timer was in the path
        if ((destInfo->transWaitingToBeSent).size() > 0) {
            pushIntoSenderQueue(destInfo, ttmsg);
        } else {
            for (auto p: nodeToShortestPathsMap[destNode]) {
                int pathIndex = p.first;
                PathInfo *pathInfo = &(nodeToShortestPathsMap[destNode][pathIndex]);
                
                if (pathInfo->sumOfTransUnitsInFlight + transMsg->getAmount() <= pathInfo->window) {
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
        // per destination statistics
        for (auto it = 0; it < _numHostNodes; it++){ 
            if (it != getIndex() && _destList[myIndex()].count(it) > 0) {
                if (nodeToShortestPathsMap.count(it) > 0) {
                    for (auto p: nodeToShortestPathsMap[it]){
                        int pathIndex = p.first;
                        PathInfo *pInfo = &(p.second);

                        //signals for price scheme per path
                        emit(pInfo->sumOfTransUnitsInFlightSignal, 
                                pInfo->sumOfTransUnitsInFlight);
                        emit(pInfo->windowSignal, pInfo->window);
                    }
                }
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
// TODO: actually maintain state as to how much has been sent and how much received
void hostNodeDCTCP::handleClearStateMessage(routerMsg *ttmsg) {
    for ( auto it = canceledTransactions.begin(); it!= canceledTransactions.end(); it++){
        int transactionId = get<0>(*it);
        simtime_t msgArrivalTime = get<1>(*it);
        int prevNode = get<2>(*it);
        int nextNode = get<3>(*it);
        int destNode = get<4>(*it);
        
        if (simTime() > (msgArrivalTime + _maxTravelTime)){
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

/* helper method to remove a transaction from the sender queue and send it on a particular path
 * to the given destination */
void hostNodeDCTCP::sendNextTransactionOnPath(int destNode, int pathIndex) {
    transactionMsg *transMsg;
    routerMsg *msgToSend;
    //remove the transaction $tu$ at the head of the queue if one exists
    if (nodeToDestInfo[destNode].transWaitingToBeSent.size() > 0) {
        routerMsg *msgToSend = nodeToDestInfo[destNode].transWaitingToBeSent.front();
        transMsg = check_and_cast<transactionMsg *>(msgToSend->getEncapsulatedPacket());
    } else
        return;

    PathInfo *pathInfo = &(nodeToShortestPathsMap[destNode][pathIndex]);
    if (pathInfo->sumOfTransUnitsInFlight + transMsg->getAmount() <= pathInfo->window) {
        // remove the transaction from queue and send it on the path
        nodeToDestInfo[destNode].transWaitingToBeSent.pop_front();
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
}
