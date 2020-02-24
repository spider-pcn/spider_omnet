#include "routerNodeCeler.h"

Define_Module(routerNodeCeler);

/* initialization function to initialize debt queues to 0 */
void routerNodeCeler::initialize(){
    routerNodeBase::initialize();

    for (int i = 0; i < _numHostNodes; ++i) { 
        _nodeToDebtQueue[myIndex()][i] = 0;
        nodeToDestNodeStruct[i].queueTimedOutSignal = registerSignalPerDest("queueTimedOut", i, "");
        nodeToDestNodeStruct[i].destQueueSignal = registerSignalPerDest("destQueue", i, "");
    }

    for ( auto it = nodeToPaymentChannel.begin(); it!= nodeToPaymentChannel.end(); it++){
        PaymentChannel *p = &(it->second);
        int id = it->first;

        if (id < _numHostNodes){
            adjacentHostNodes.insert(id);
        }
        p->kStarSignal = registerSignalPerChannel("kStar", id); 
        for (int destNode = 0; destNode < _numHostNodes; destNode++){
            simsignal_t signal;
            signal = registerSignalPerChannelPerDest("cpi", id, destNode);
            p->destToCPISignal[destNode] = signal;
            p->destToCPIValue[destNode] = -1;
        }
    }
}

/* end routine to get rid of messages in per router queues to every destination */
void routerNodeCeler::finish() {
    for (int i = 0; i < _numHostNodes; ++i) {
        if (nodeToDestNodeStruct.count(i) > 0) {
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
    }
    routerNodeBase::finish();
}

/* handler for the statistic message triggered every x seconds
 * emits kstar for every payment channel and queue sizes to every destination
 */
void routerNodeCeler::handleStatMessage(routerMsg* ttmsg){
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
            DestNodeStruct *destNodeInfo = &(nodeToDestNodeStruct[destNode]);
            emit(destNodeInfo->queueTimedOutSignal, destNodeInfo->totalNumTimedOut);
            emit(destNodeInfo->destQueueSignal, destNodeInfo->totalAmtInQueue);
        }
    }

    // call the base method to output rest of the stats
    routerNodeBase::handleStatMessage(ttmsg);
}



/* handler for timeout messages that is responsible for removing messages from 
 * per-dest queues if they haven't been sent yet and sends explicit time out messages
 * for messages that have been sent on a path already
 * uses a structure to find the next hop and sends the time out there
 */
void routerNodeCeler::handleTimeOutMessage(routerMsg* ttmsg) {
    timeOutMsg *toutMsg = check_and_cast<timeOutMsg *>(ttmsg->getEncapsulatedPacket());
    int destination = toutMsg->getReceiver();
    int transactionId = toutMsg->getTransactionId();
    int prevNode = (ttmsg->getRoute())[ttmsg->getHopCount()-1];

    // check where to send timeout message next if a next hop exists 
    int nextNode = -1;
    if (transToNextHop.count(transactionId) > 0) {
        nextNode = transToNextHop[transactionId].front();
        transToNextHop[transactionId].pop_front();
        if (transToNextHop[transactionId].size() == 0)
            transToNextHop.erase(transactionId);
        appendNextHopToTimeOutMessage(ttmsg, nextNode);
        forwardMessage(ttmsg);
    }
    if (nextNode == -1)     
        updateMaxTravelTime(ttmsg->getRoute());

    // if there's an old entry, update with current time
    // otherwise insert transactions into canceled trans list
    auto iter = canceledTransactions.find(make_tuple(transactionId, 0, 0, 0, 0));
    if (iter != canceledTransactions.end()){
        canceledTransactions.erase(iter);
    } 
    CanceledTrans ct = make_tuple(toutMsg->getTransactionId(), 
    simTime(), prevNode, nextNode, destination);
    canceledTransactions.insert(ct);
    
    if (nextNode == -1) {
        ttmsg->decapsulate();
        delete toutMsg;
        delete ttmsg;
    }
}


/* handler for clearing state associated with timed out transaction
 * from per dest queues 
 */
void routerNodeCeler::handleClearStateMessage(routerMsg* ttmsg) {
    //reschedule for the next interval
    if (simTime() > _simulationLength){
        delete ttmsg;
    }
    else{
        scheduleAt(simTime()+_clearRate, ttmsg);
    }

    for ( auto it = nodeToPaymentChannel.begin(); it!= nodeToPaymentChannel.end(); it++){
        PaymentChannel *p = &(it->second);
        int id = it->first;
        p->channelImbalance.push_back(p->deltaAmtReceived - p->deltaAmtSent);
        emit(p->numSentPerChannelSignal, p->deltaAmtSent);
	p->deltaAmtReceived = 0;
        p->deltaAmtSent = 0;
        if (p->channelImbalance.size() > _maxImbalanceWindow){ 
            p->channelImbalance.erase(p->channelImbalance.begin());
        }
    }
    
    for ( auto it = canceledTransactions.begin(); it!= canceledTransactions.end(); ) {      
        int transactionId = get<0>(*it);
        simtime_t msgArrivalTime = get<1>(*it);
        int prevNode = get<2>(*it);
        int nextNode = get<3>(*it);
        int destNode = get<4>(*it);
        vector<tuple<int, double, routerMsg*,  Id, simtime_t >> *transList = 
        &(nodeToDestNodeStruct[destNode].queuedTransUnits);
        
        // if grace period has passed
        if (simTime() > msgArrivalTime + _maxTravelTime){  
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
                _nodeToDebtQueue[myIndex()][destNode] -= amount;
                nodeToDestNodeStruct[destNode].totalNumTimedOut = 
                    nodeToDestNodeStruct[destNode].totalNumTimedOut + 1;
            }
            
            // go through all payment channels and remove from incoming and outgoing if present
            for (auto pIt = nodeToPaymentChannel.begin(); pIt != nodeToPaymentChannel.end(); ++pIt) {
                PaymentChannel* channel  = &(pIt->second);
                unordered_map<Id, double, hashId> *incomingTransUnits = 
                    &(channel->incomingTransUnits);
                auto iterIncoming = find_if((*incomingTransUnits).begin(),
                  (*incomingTransUnits).end(),
                  [&transactionId](const pair<tuple<int, int >, double> & p)
                  { return get<0>(p.first) == transactionId; });
                
                if (iterIncoming != (*incomingTransUnits).end()){
                    channel->totalAmtIncomingInflight -= iterIncoming->second;
                    iterIncoming = (*incomingTransUnits).erase(iterIncoming);
                }
            }
        }
               
       // remove from outgoing transUnits if present
       if (simTime() > msgArrivalTime + _maxTravelTime + _maxOneHopDelay){  
           for (auto pIt = nodeToPaymentChannel.begin(); pIt != nodeToPaymentChannel.end(); ++pIt) {
               PaymentChannel* channel  = &(pIt->second);
               unordered_map<Id, double, hashId> *outgoingTransUnits = 
                   &(channel->outgoingTransUnits);
               
               auto iterOutgoing = find_if((*outgoingTransUnits).begin(),
                 (*outgoingTransUnits).end(),
                 [&transactionId](const pair<tuple<int, int >, double> &q)
                 { return get<0>(q.first) == transactionId; });
               
               if (iterOutgoing != (*outgoingTransUnits).end()){
                   double amount = iterOutgoing->second;
                   channel->totalAmtOutgoingInflight -= amount;
                   iterOutgoing = (*outgoingTransUnits).erase(iterOutgoing);
                   
                   double updatedBalance = channel->balance + amount;
                   setPaymentChannelBalanceByNode(pIt->first, updatedBalance);
                   channel->balanceEWMA = (1 -_ewmaFactor) * channel->balanceEWMA + 
                       (_ewmaFactor) * updatedBalance;
               }
           }
           // all done, can remove txn and update stats
           it = canceledTransactions.erase(it);
       }
       else{
           it++;
       }
    }
}

/* main routine for handling transaction messages for celer
 * first adds transactions to the appropriate per destination queue at a router
 * and then processes transactions in order of the destination with highest CPI
 */
void routerNodeCeler::handleTransactionMessage(routerMsg* ttmsg){
    transactionMsg *transMsg = check_and_cast<transactionMsg *>(ttmsg->getEncapsulatedPacket());
    int hopcount = ttmsg->getHopCount();
    int destNode = transMsg->getReceiver();
    int transactionId = transMsg->getTransactionId();

    // ignore if txn is already cancelled
    auto iter = canceledTransactions.find(make_tuple(transactionId, 0, 0, 0, 0));
    if (iter != canceledTransactions.end() ){
        //delete yourself, message won't be encountered again
        ttmsg->decapsulate();
        delete transMsg;
        delete ttmsg;
        return;
    }

    // add to incoming trans units
    int prevNode = ttmsg->getRoute()[ttmsg->getHopCount()-1];
    unordered_map<Id, double, hashId> *incomingTransUnits =
            &(nodeToPaymentChannel[prevNode].incomingTransUnits);
    (*incomingTransUnits)[make_tuple(transMsg->getTransactionId(), transMsg->getHtlcIndex())] =
            transMsg->getAmount();
    nodeToPaymentChannel[prevNode].totalAmtIncomingInflight += transMsg->getAmount();
    
    // queue the transaction
    DestNodeStruct *destStruct = &(nodeToDestNodeStruct[destNode]);
    vector<tuple<int, double , routerMsg *, Id, simtime_t>> *q = &(destStruct->queuedTransUnits);
    tuple<int,int > key = make_tuple(transMsg->getTransactionId(), transMsg->getHtlcIndex());

    // add to queue and process in order of queue
    (*q).push_back(make_tuple(transMsg->getPriorityClass(), transMsg->getAmount(),
            ttmsg, key, simTime()));
    push_heap((*q).begin(), (*q).end(), _schedulingAlgorithm);
    
    // update debt queues and process according to celer
    destStruct->totalAmtInQueue += transMsg->getAmount();
    _nodeToDebtQueue[myIndex()][destNode] += transMsg->getAmount();
    celerProcessTransactions();
}

/* specialized ack handler that removes transaction information
 * from the transToNextHop map
 * NOTE: acks are on the reverse path relative to the original sender
 */
void routerNodeCeler::handleAckMessage(routerMsg* ttmsg) {
    ackMsg *aMsg = check_and_cast<ackMsg*>(ttmsg->getEncapsulatedPacket());
    int transactionId = aMsg->getTransactionId();
    int nextNode = ttmsg->getRoute()[ttmsg->getHopCount()+1];
    transToNextHop[transactionId].pop_back();
    if (transToNextHop[transactionId].size() == 0)
        transToNextHop.erase(transactionId);
    if (aMsg->getIsSuccess()) {
        nodeToPaymentChannel[nextNode].deltaAmtReceived +=  aMsg->getAmount();
        nodeToPaymentChannel[nextNode].totalAmtReceived +=  aMsg->getAmount();
    }
    routerNodeBase::handleAckMessage(ttmsg);
}

/* special type of time out message for celer designed for a specific path 
 * that is contructed dynamically or one hop at a time, until the transaction
 * is deleted at the router itself and then the message needs to go 
 * no further
 */
void routerNodeCeler::appendNextHopToTimeOutMessage(routerMsg* ttmsg, int nextNode) {
    vector<int> newRoute = ttmsg->getRoute();
    newRoute.push_back(nextNode);
    ttmsg->setRoute(newRoute);
}

/* helper function to process transactions to the neighboring node if there are transactions to 
 * be sent on this payment channel, if one is passed in
 * otherwise use any payment channel to send out transactions
 */
void routerNodeCeler::celerProcessTransactions(int neighborNode){
    unordered_set<int> exclude;
    if (neighborNode != -1){
        int kStar = findKStar(neighborNode, exclude);
        while (kStar >= 0){
            vector<tuple<int, double , routerMsg *, Id, simtime_t>> *q;
            q = &(nodeToDestNodeStruct[kStar].queuedTransUnits);
            if (!processTransUnits(neighborNode, *q))
                break;
            exclude.insert(kStar); // ignore this dest because queue is empty
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
            exclude.clear();
            if (positiveKey.size() == 0)
               break;
            
            //generate random channel with positive balance to process
            int randIdx = rand() % positiveKey.size();
            int key = positiveKey[randIdx]; //node
            positiveKey.erase(positiveKey.begin() + randIdx);
            
            if (key < _numHostNodes){ //adjacent host node payment channel
                vector<tuple<int, double, routerMsg *, Id, simtime_t>> *k;
                k = &(nodeToDestNodeStruct[key].queuedTransUnits);
                processTransUnits(key, *k);
            }
            else {
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
}

/* helper function to calculate the destination with the maximum CPI weight
 * that we should send transactions to on this payment channel
 */
int routerNodeCeler::findKStar(int neighborNode, unordered_set<int> exclude){
    int destNode = -1;
    int highestCPI = -1000000;
    for (int i = 0; i < _numHostNodes; ++i) { //initialize debt queues map
        if (adjacentHostNodes.count(i) == 0 && nodeToDestNodeStruct.count(i) > 0 && exclude.count(i) == 0) {
            double CPI = calculateCPI(i, neighborNode); 
            if (destNode == -1 || (CPI > highestCPI)){
                destNode = i;
                highestCPI = CPI;
            }
        }
    }
    return destNode;
}

/* helper function to calculate congestion plus imbalance price 
 */
double routerNodeCeler::calculateCPI(int destNode, int neighborNode){
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
int routerNodeCeler::forwardTransactionMessage(routerMsg *msg, int nextNode, simtime_t arrivalTime) {
    transactionMsg *transMsg = check_and_cast<transactionMsg *>(msg->getEncapsulatedPacket());
    int transactionId = transMsg->getTransactionId();
    PaymentChannel *neighbor = &(nodeToPaymentChannel[nextNode]);
    int dest = transMsg->getReceiver();
    int amt = transMsg->getAmount();
    Id thisTrans = make_tuple(transactionId, transMsg->getHtlcIndex());

    if (neighbor->balance <= 0 || transMsg->getAmount() > neighbor->balance){
        return 0;
    }
    else if (neighbor->incomingTransUnits.count(thisTrans) > 0 ||
            neighbor->outgoingTransUnits.count(thisTrans) > 0 || 
            (nextNode < _numHostNodes && nextNode != dest)) {
        // don't cause cycles, don't send to end host that's not destination
        return -1;
    }
    else {
        // if cancelled, remove it from queue calculations 
        auto iter = canceledTransactions.find(make_tuple(transactionId, 0, 0, 0, 0));
        if (iter != canceledTransactions.end()) {
            nodeToDestNodeStruct[dest].totalAmtInQueue -= transMsg->getAmount();
            _nodeToDebtQueue[myIndex()][dest] -= transMsg->getAmount();
            return routerNodeBase::forwardTransactionMessage(msg, nextNode, arrivalTime);
        }

        //append next node to the route of the routerMsg
        vector<int> newRoute = msg->getRoute();
        newRoute.push_back(nextNode);
        transToNextHop[transactionId].push_back(nextNode);
        msg->setRoute(newRoute);

        //decrement the total amount in queue to the destination
        nodeToDestNodeStruct[dest].totalAmtInQueue -= transMsg->getAmount();
        _nodeToDebtQueue[myIndex()][dest] -= transMsg->getAmount();

        //increment statAmtSent for channel in-balance calculations
        neighbor->deltaAmtSent+=  transMsg->getAmount();
        neighbor->totalAmtSent+=  transMsg->getAmount();
        return routerNodeBase::forwardTransactionMessage(msg, nextNode, arrivalTime);
    }
    return 1;
}


/* set balance of a payment channel to the passed in amount and if funds were added process
 * more payments that can be sent via celer
 */
void routerNodeCeler::setPaymentChannelBalanceByNode(int node, double amt){
    bool addedFunds = false;
    if (amt > nodeToPaymentChannel[node].balance){
        addedFunds = true;
    }
    nodeToPaymentChannel[node].balance = amt;
    if (addedFunds){
        celerProcessTransactions(node);
    }
}
