#include "hostNodeCeler.h"
Define_Module(hostNodeCeler);

// global debt queue and beta value
unordered_map<int, unordered_map<int, double>> _nodeToDebtQueue;
double _celerBeta;
int _maxImbalanceWindow = 5;

/* initialization function to initialize parameters */
void hostNodeCeler::initialize(){
    hostNodeBase::initialize();
    if (myIndex() == 0) {
        _celerBeta = 0.2;
        _nodeToDebtQueue = {};
        for (int i = 0; i < _numNodes; ++i) { 
            _nodeToDebtQueue[i] = {};
        }
    }

    // initialize debt queues values to 0 for each dest/host node
    for (int i = 0; i < _numHostNodes; ++i) {         
        _nodeToDebtQueue[myIndex()][i] = 0;
    }

    for ( auto it = nodeToPaymentChannel.begin(); it!= nodeToPaymentChannel.end(); it++){
        PaymentChannel *p = &(it->second);
        int id = it->first;
        p->kStarSignal = registerSignalPerChannel("kStar", id); 

        for (int destNode = 0; destNode < _numHostNodes; destNode++){
            simsignal_t signal;
            signal = registerSignalPerChannelPerDest("cpi", id, destNode);
            p->destToCPISignal[destNode] = signal;
            p->destToCPIValue[destNode] = -1;
        }
    }

    for (int i = 0; i < _numHostNodes; ++i) {
        if (_destList[myIndex()].count(i) > 0) {
            nodeToDestNodeStruct[i].destQueueSignal = registerSignalPerDest("destQueue", i, ""); 
            nodeToDestNodeStruct[i].queueTimedOutSignal = registerSignalPerDest("queueTimedOut", i, "");
        }
    }
}

/* end routine to get rid of messages in per router queues to every destination */
void hostNodeCeler::finish() {
    // go through per dest queues if any 
    for (int i = 0; i < _numHostNodes; ++i) {
        vector<tuple<int, double, routerMsg*,  Id, simtime_t >> *q = 
            &(nodeToDestNodeStruct[i].queuedTransUnits);
        for (auto temp = q->begin(); temp != q->end(); ){
            routerMsg * rMsg = get<2>(*temp);
            auto tMsg = rMsg->getEncapsulatedPacket();
            rMsg->decapsulate();
            delete tMsg;
            delete rMsg;
            temp = q->erase(temp);
        }
    }
    hostNodeBase::finish();
}


/* handler for the statistic message triggered every x seconds
 * emits kstar for every payment channel and queue sizes to every destination
 */
void hostNodeCeler::handleStatMessage(routerMsg* ttmsg){
    if (_signalsEnabled) {
        for ( auto it = nodeToPaymentChannel.begin(); it!= nodeToPaymentChannel.end(); it++){
            int node = it->first; 
            PaymentChannel* p = &(nodeToPaymentChannel[node]);
            unordered_set<int> exclude;
            emit(p->kStarSignal, findKStar(node, exclude));

            for (auto destNode = 0; destNode < _numHostNodes; destNode++){
                emit(p->destToCPISignal[destNode], p->destToCPIValue[destNode]);
            }
        }
        for (auto destNode = 0; destNode < _numHostNodes; destNode++) {
            if (_destList[myIndex()].count(destNode) > 0) {
                DestNodeStruct *destNodeInfo = &(nodeToDestNodeStruct[destNode]);
                emit(destNodeInfo->destQueueSignal, destNodeInfo->totalAmtInQueue);
                emit(destNodeInfo->queueTimedOutSignal, destNodeInfo->totalNumTimedOut);
            }
        }
    }

    // call the base method to output rest of the stats
    hostNodeBase::handleStatMessage(ttmsg);
}

/* handler for clearing state associated with timed out transaction
 * from per dest queues 
 */
void hostNodeCeler::handleClearStateMessage(routerMsg* ttmsg) {
    for ( auto it = nodeToPaymentChannel.begin(); it!= nodeToPaymentChannel.end(); it++){
        PaymentChannel *p = &(it->second);
        int id = it->first;
        p->channelImbalance.push_back(p->deltaAmtReceived - p->deltaAmtSent);
        p->deltaAmtReceived = 0;
        p->deltaAmtSent = 0;
        if (p->channelImbalance.size() > _maxImbalanceWindow){ 
            p->channelImbalance.erase(p->channelImbalance.begin());
        }
    }

     for ( auto it = canceledTransactions.begin(); it!= canceledTransactions.end(); ++it) {
         int transactionId = get<0>(*it);
         simtime_t msgArrivalTime = get<1>(*it);
         int prevNode = get<2>(*it);
         int nextNode = get<3>(*it);
         int destNode = get<4>(*it);
         vector<tuple<int, double, routerMsg*,  Id, simtime_t >> *transList = 
         &(nodeToDestNodeStruct[destNode].queuedTransUnits);
         
         // if grace period has passed
         if (simTime() > (msgArrivalTime + _maxTravelTime)){  
             // check if txn is still in just sender queue, just delete and return then
             auto iter = find_if(transList->begin(),
                 transList->end(),
                 [&transactionId](tuple<int, double, routerMsg*,  Id, simtime_t> p)
                 { return get<0>(get<3>(p)) == transactionId; });

             if (iter != transList->end()) {
                 deleteTransaction(get<2>(*iter));
                 double amount = get<1>(*iter);
                 transList->erase(iter);
                 make_heap((*transList).begin(), (*transList).end(), _schedulingAlgorithm);
                 nodeToDestNodeStruct[destNode].totalAmtInQueue -= amount;
                 nodeToDestNodeStruct[destNode].totalNumTimedOut = 
                     nodeToDestNodeStruct[destNode].totalNumTimedOut + 1;
                 _nodeToDebtQueue[myIndex()][destNode] -= amount;
             }
         }
     } 
     hostNodeBase::handleClearStateMessage(ttmsg);
}


/* handler for timeout messages that is responsible for removing messages from 
 * per-dest queues if they haven't been sent yet and sends explicit time out messages
 * for messages that have been sent on a path already
 * uses a structure to find the next hop and sends the time out there
 */
void hostNodeCeler::handleTimeOutMessage(routerMsg* ttmsg) {
    timeOutMsg *toutMsg = check_and_cast<timeOutMsg *>(ttmsg->getEncapsulatedPacket());
    int destination = toutMsg->getReceiver();
    int transactionId = toutMsg->getTransactionId();
    vector<tuple<int, double, routerMsg*,  Id, simtime_t >> *transList = 
        &(nodeToDestNodeStruct[destination].queuedTransUnits);
    
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
           [&transactionId](tuple<int, double, routerMsg*,  Id, simtime_t> p)
           { return get<0>(get<3>(p)) == transactionId; });

        if (iter != transList->end()) {
            double amount = get<1>(*iter);
            deleteTransaction(get<2>(*iter));
            transList->erase(iter);
            push_heap((*transList).begin(), (*transList).end(), _schedulingAlgorithm);
            nodeToDestNodeStruct[destination].totalAmtInQueue -= amount;
            _nodeToDebtQueue[myIndex()][destination] -= amount;
        }

        int nextNode = -1;
        if (transToNextHop.count(transactionId) > 0) {
            nextNode = transToNextHop[transactionId];
            vector<int> twoHopRoute;
            twoHopRoute.push_back(myIndex());
            twoHopRoute.push_back(nextNode);
            routerMsg* ctMsg = generateTimeOutMessageForPath(
                   twoHopRoute, transactionId, destination);
            forwardMessage(ctMsg);
            transToNextHop.erase(transactionId);
        }
    
        CanceledTrans ct = make_tuple(toutMsg->getTransactionId(), 
        simTime(), -1, nextNode, destination);
        canceledTransactions.insert(ct);

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


/* main routine for handling transaction messages for celer
 * first adds transactions to the appropriate per destination queue at a router
 * and then processes transactions in order of the destination with highest CPI
 */
void hostNodeCeler::handleTransactionMessageSpecialized(routerMsg* ttmsg){
    transactionMsg *transMsg = check_and_cast<transactionMsg *>(ttmsg->getEncapsulatedPacket());
    int hopCount = ttmsg->getHopCount();
    int destNode = transMsg->getReceiver();
    int transactionId = transMsg->getTransactionId();

    // process transaction at sender/receiver
    if (myIndex() == destNode) { 
        //if at destination, increment stats and generate ack
        int prevNode = ttmsg->getRoute()[ttmsg->getHopCount() - 1];
        PaymentChannel *prevNeighbor = &(nodeToPaymentChannel[prevNode]);
        prevNeighbor->deltaAmtReceived += transMsg->getAmount();
        handleTransactionMessage(ttmsg, false); 
    }
    else {
        // transaction received at sender
        // add transaction to appropriate queue (sorted based on dest node)
        int destNode = transMsg->getReceiver();

        // ignore if txn is already cancelled
        auto iter = canceledTransactions.find(make_tuple(transactionId, 0, 0, 0, 0));
        if ( iter != canceledTransactions.end() ){
            //delete yourself, message won't be encountered again
            ttmsg->decapsulate();
            delete transMsg;
            delete ttmsg;
            return;
        }

        // gather stats for completion time
        SplitState* splitInfo = &(_numSplits[myIndex()][transMsg->getLargerTxnId()]);
        splitInfo->numArrived += 1;
        
        // first time seeing this transaction so add to d_ij computation
        // count the txn for accounting also
        if (simTime() == transMsg->getTimeSent()) {
            transMsg->setTimeAttempted(simTime().dbl());
            destNodeToNumTransPending[destNode]  += 1;
            nodeToDestInfo[destNode].transSinceLastInterval += transMsg->getAmount();
            if (splitInfo->numArrived == 1)
                splitInfo->firstAttemptTime = simTime().dbl();

            if (transMsg->getTimeSent() >= _transStatStart && 
                transMsg->getTimeSent() <= _transStatEnd) {
                statAmtArrived[destNode] += transMsg->getAmount();
                statAmtAttempted[destNode] += transMsg->getAmount();
            
                if (splitInfo->numArrived == 1) {       
                    statRateArrived[destNode] += 1;
                    statRateAttempted[destNode] += 1;
                }
            }
            if (splitInfo->numArrived == 1) 
                statNumArrived[destNode] += 1;
        }

        // add to queue, udpate debt queue and process in order of queue
        pushIntoPerDestQueue(ttmsg, destNode);        
        celerProcessTransactions();
    }
}

/* specialized ack handler that removes transaction information
 * from the transToNextHop map and updates stats
 * NOTE: acks are on the reverse path relative to the original sender
 */
void hostNodeCeler::handleAckMessageSpecialized(routerMsg* ttmsg) {
    ackMsg *aMsg = check_and_cast<ackMsg*>(ttmsg->getEncapsulatedPacket());
    int transactionId = aMsg->getTransactionId();
    int destNode = ttmsg->getRoute()[0];
    double largerTxnId = aMsg->getLargerTxnId();
    
    // aggregate stats
    transToNextHop.erase(transactionId);
    updateMaxTravelTime(ttmsg->getRoute());
    SplitState* splitInfo = &(_numSplits[myIndex()][largerTxnId]);
    splitInfo->numReceived += 1;

    if (aMsg->getTimeSent() >= _transStatStart && 
            aMsg->getTimeSent() <= _transStatEnd) {
        if (aMsg->getIsSuccess()) {
            statAmtCompleted[destNode] += aMsg->getAmount();
            if (splitInfo->numTotal == splitInfo->numReceived) {
                statRateCompleted[destNode] += 1;
                _transactionCompletionBySize[splitInfo->totalAmount] += 1;
                double timeTaken = simTime().dbl() - splitInfo->firstAttemptTime;
                statCompletionTimes[destNode] += timeTaken * 1000;
            }
            if (splitInfo->numTotal == splitInfo->numReceived) 
                statNumCompleted[destNode] += 1;
        }
    }

    // retry transaction by placing it in queue if it hasn't timed out
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
            pushIntoPerDestQueue(duplicateTrans, destNode);
        }
    }

    hostNodeBase::handleAckMessage(ttmsg);
}

/* helper function to push things into per destination queue and update debt queue 
 * at the sender 
 */
void hostNodeCeler::pushIntoPerDestQueue(routerMsg* rMsg, int destNode) {
    transactionMsg *transMsg = check_and_cast<transactionMsg *>(rMsg->getEncapsulatedPacket());
    DestNodeStruct *destStruct = &(nodeToDestNodeStruct[destNode]);
    tuple<int,int > key = make_tuple(transMsg->getTransactionId(), transMsg->getHtlcIndex());
    vector<tuple<int, double , routerMsg *, Id, simtime_t>> *q;
    q = &(destStruct->queuedTransUnits);
    
    (*q).push_back(make_tuple(transMsg->getPriorityClass(), transMsg->getAmount(),
            rMsg, key, simTime()));
    destStruct->totalAmtInQueue += transMsg->getAmount();
    _nodeToDebtQueue[myIndex()][destNode] += transMsg->getAmount();
    push_heap((*q).begin(), (*q).end(), _schedulingAlgorithm); 
}

/* helper function to process transactions to the neighboring node if there are transactions to 
 * be sent on this payment channel, if one is passed in
 * otherwise use any payment channel to send out transactions
 */
void hostNodeCeler::celerProcessTransactions(int neighborNode){
    unordered_set<int> exclude;
    // -1 denotes you can send on any of the links and not a particualr one
    if (neighborNode != -1){ 
        int kStar = findKStar(neighborNode, exclude);
        while (kStar >= 0){
            vector<tuple<int, double , routerMsg *, Id, simtime_t>> *q;
            q = &(nodeToDestNodeStruct[kStar].queuedTransUnits);
            if (!processTransUnits(neighborNode, *q))
                break;
            exclude.insert(kStar);
            kStar = findKStar(neighborNode, exclude);
        }
    }
    else{
        // get all paymentChannels with positive balance
        vector<int> positiveKey = {};
        for (auto iter = nodeToPaymentChannel.begin(); iter != nodeToPaymentChannel.end(); ++iter){
            if (iter->second.balance > 0){
                positiveKey.push_back(iter->first);
            }
        }
        while (true){
            if (positiveKey.size() == 0)
               break;
            
            //generate random channel with positie balance to process
            int randIdx = rand() % positiveKey.size();
            int key = positiveKey[randIdx]; //node
            positiveKey.erase(positiveKey.begin() + randIdx);
            
            // for each payment channel (nextNode), choose a k* or 
            // destNode queue to use as q*, and send as much as possible to that dest
            // if no more transactions left, keep finding the next kStar for that channel
            // until it is exhausted or no more transactions in any dest queue
            int kStar = findKStar(key, exclude);
            while (kStar >= 0) {
                vector<tuple<int, double, routerMsg *, Id, simtime_t>> *k;
                k = &(nodeToDestNodeStruct[kStar].queuedTransUnits);
                if (!processTransUnits(key, *k)) 
                    break; 
                exclude.insert(kStar); // ignore this dest because queue is empty
                kStar = findKStar(key, exclude);
            }
            if (kStar == -1) // no more transactions in any dest queue
                break;
        }
    }
}


/* helper function to find the destination that has the maximum CPI weight for the 
 * payment channel with passed in neighbor node so that you process its transactions next
 */
int hostNodeCeler::findKStar(int neighborNode, unordered_set<int> exclude){
    int destNode = -1;
    int highestCPI = -10000000;
    for (int i = 0; i < _numHostNodes; ++i) { //initialize debt queues map
        if (nodeToDestNodeStruct.count(i) > 0 && exclude.count(i) == 0){
            double CPI = calculateCPI(i, neighborNode);
            if (destNode == -1 || (CPI > highestCPI)){
                destNode = i;
                highestCPI = CPI;
            }
        }
    }
    return destNode;
}

/* helper function to calculate the congestion plus imbalance weight associated with a 
 * payment channel and a particular destination 
 */
double hostNodeCeler::calculateCPI(int destNode, int neighborNode){
    PaymentChannel *neighbor = &(nodeToPaymentChannel[neighborNode]);
    double channelImbalance = 0;
    accumulate(neighbor->channelImbalance.begin(), neighbor->channelImbalance.end(), channelImbalance);

    double Q_ik = _nodeToDebtQueue[myIndex()][destNode];
    double Q_jk = _nodeToDebtQueue[neighborNode][destNode];

    double W_ijk = Q_ik - Q_jk + _celerBeta*channelImbalance;
    neighbor->destToCPIValue[destNode] = W_ijk;
    return W_ijk;
}

/* updates debt queue information (removing from it) before performing the regular
 * routine of forwarding a transction only if there's balance on the payment channel
 */
int hostNodeCeler::forwardTransactionMessage(routerMsg *msg, int nextNode, simtime_t arrivalTime) {
    transactionMsg *transMsg = check_and_cast<transactionMsg *>(msg->getEncapsulatedPacket());
    int transactionId = transMsg->getTransactionId();
    PaymentChannel *neighbor = &(nodeToPaymentChannel[nextNode]);
    int amt = transMsg->getAmount();
    int dest = transMsg->getReceiver();
    Id thisTrans = make_tuple(transactionId, transMsg->getHtlcIndex());

    if (neighbor->balance <= 0 || transMsg->getAmount() > neighbor->balance){
        return 0;
    }
    else if (neighbor->incomingTransUnits.count(thisTrans) > 0 || 
            neighbor->outgoingTransUnits.count(thisTrans) > 0 ) {
        // don't cause cycles
        return -1;
    }
    else {
        // if cancelled, remove it from queue calculations 
        auto iter = canceledTransactions.find(make_tuple(transactionId, 0, 0, 0, 0));
        if (iter != canceledTransactions.end()) {
            nodeToDestNodeStruct[dest].totalAmtInQueue -= transMsg->getAmount();
            _nodeToDebtQueue[myIndex()][dest] -= transMsg->getAmount();
            return hostNodeBase::forwardTransactionMessage(msg, nextNode, arrivalTime);
        }

        //append next destination to the route of the routerMsg
        vector<int> newRoute = msg->getRoute();
        if (newRoute.size() == 0) {
            newRoute.push_back(myIndex());
        }
        transToNextHop[transactionId] = nextNode;
        newRoute.push_back(nextNode);
        msg->setRoute(newRoute);

        //decrement debt queue stats and payment stats
        nodeToDestNodeStruct[dest].totalAmtInQueue -= transMsg->getAmount();
        _nodeToDebtQueue[myIndex()][dest] -= transMsg->getAmount();
        neighbor->deltaAmtSent +=  transMsg->getAmount();
        return hostNodeBase::forwardTransactionMessage(msg, nextNode, arrivalTime);
    }
    return 1;
}

/* set balance of a payment channel to the passed in amount and if funds were added process
 * more payments that can be sent via celer
 */
void hostNodeCeler::setPaymentChannelBalanceByNode(int node, double amt){
    bool addedFunds = false;
    if (amt > nodeToPaymentChannel[node].balance){
        addedFunds = true;
    }
    nodeToPaymentChannel[node].balance = amt;
    if (addedFunds){
        celerProcessTransactions(node);
    }
}
