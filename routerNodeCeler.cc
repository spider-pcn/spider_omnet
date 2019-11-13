#include "routerNodeCeler.h"

Define_Module(routerNodeCeler);

/* initialization function to initialize debt queues to 0 */
void routerNodeCeler::initialize(){
    routerNodeBase::initialize();

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
            //naming: signal_(paymentChannel endNode)destNode
            p->destToCPISignal[destNode] = signal;
            p->destToCPIValue[destNode] = -1;
        }
    }
    
    for (int i = 0; i < _numHostNodes; ++i) {
        nodeToDestNodeStruct[i].destQueueSignal = registerSignalPerDest("destQueue", i, ""); 
    }

}


/* handler for the statistic message triggered every x seconds
 */
void routerNodeCeler::handleStatMessage(routerMsg* ttmsg){
    if (_signalsEnabled) {
        for ( auto it = nodeToPaymentChannel.begin(); it!= nodeToPaymentChannel.end(); it++){
            int node = it->first; //key
            PaymentChannel* p = &(nodeToPaymentChannel[node]);
            emit(p->kStarSignal, findKStar(node));

            for (auto destNode = 0; destNode < _numHostNodes; destNode++){
                emit(p->destToCPISignal[destNode], p->destToCPIValue[destNode]);
            }
        }

        for (auto destNode = 0; destNode < _numHostNodes; destNode++) {
            DestNodeStruct *destNodeInfo = &(nodeToDestNodeStruct[destNode]);
            emit(destNodeInfo->destQueueSignal, destNodeInfo->totalAmtInQueue);
        }
    }

    // call the base method to output rest of the stats
    routerNodeBase::handleStatMessage(ttmsg);
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

/* handler for timeout messages that is responsible for removing messages from 
 * per-dest queues if they haven't been sent yet and sends explicit time out messages
 * for messages that have been sent on a path already
 * uses a structure to find the next hop and sends the time out there
 */
void routerNodeCeler::handleTimeOutMessage(routerMsg* ttmsg) {
    timeOutMsg *toutMsg = check_and_cast<timeOutMsg *>(ttmsg->getEncapsulatedPacket());
    int destination = toutMsg->getReceiver();
    int transactionId = toutMsg->getTransactionId();
    vector<tuple<int, double, routerMsg*,  Id, simtime_t >> *transList = 
        &(nodeToDestNodeStruct[destination].queuedTransUnits);
    
    // check if txn is still in just sender queue, just delete and return then
    auto iter = find_if(transList->begin(),
       transList->end(),
       [&transactionId](tuple<int, double, routerMsg*,  Id, simtime_t> p)
       { return get<0>(get<3>(p)) == transactionId; });

    if (iter != transList->end()) {
        deleteTransaction(get<2>(*iter));
        transList->erase(iter);
        ttmsg->decapsulate();
        delete toutMsg;
        delete ttmsg;
        push_heap((*transList).begin(), (*transList).end(), _schedulingAlgorithm); 
        return;
    }

    // check where to send transaction next if a next hop exists 
    if (transToNextHop.count(transactionId) > 0) {
        int nextNode = transToNextHop[transactionId];
        appendNextHopToTimeOutMessage(ttmsg, nextNode);
        CanceledTrans ct = make_tuple(toutMsg->getTransactionId(), 
                simTime(), -1, nextNode, destination);
        canceledTransactions.insert(ct);
        forwardMessage(ttmsg);
        transToNextHop.erase(nextNode);
    }
    else {
        ttmsg->decapsulate();
        delete toutMsg;
        delete ttmsg;
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
    if ( iter != canceledTransactions.end() ){
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

    //add transaction to appropriate queue (sorted based on dest node)
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
    nodeToPaymentChannel[prevNode].statAmtReceived +=  transMsg->getAmount();
    celerProcessTransactions();
}

/* specialized ack handler that removes transaction information
 * from the transToNextHop map
 * NOTE: acks are on the reverse path relative to the original sender
 */
void routerNodeCeler::handleAckMessage(routerMsg* ttmsg) {
    ackMsg *aMsg = check_and_cast<ackMsg*>(ttmsg->getEncapsulatedPacket());
    int transactionId = aMsg->getTransactionId();
    transToNextHop.erase(transactionId);
    routerNodeBase::handleAckMessage(ttmsg);
}

/* helper function to process transactions to the neighboring node if there are transactions to 
 * be sent on this payment channel, if one is passed in
 * otherwise use any payment channel to send out transactions
 */
void routerNodeCeler::celerProcessTransactions(int neighborNode){
    if (neighborNode != -1){
        int kStar = findKStar(neighborNode);
        if (kStar >= 0){
            vector<tuple<int, double , routerMsg *, Id, simtime_t>> *q;
            q = &(nodeToDestNodeStruct[kStar].queuedTransUnits);
            processTransUnits(neighborNode, *q);
        }
    }
    else{
        // for each payment channel (nextNode), choose a k*/
        // destNode queue to use as q*, and send as much as possible
        // TODO: maybe randomize this ordering so you don't deterministically flood the first
        // payment channel
        while (true){

               //get all paymentChannels with positive balance
               vector<int> positiveKey = {};
               for (auto iter = nodeToPaymentChannel.begin(); iter != nodeToPaymentChannel.end(); ++iter){
                   if (iter->second.balance > 0){
                       positiveKey.push_back(iter->first);

                   }
               }

               if (positiveKey.size() == 0)
                   break;

               //generate random channel to process
               int randIdx = rand() % positiveKey.size();

               int key = positiveKey[randIdx]; //node
               int kStar = findKStar(key);
               if (kStar >= 0) {
                   vector<tuple<int, double, routerMsg *, Id, simtime_t>> *k;
                   k = &(nodeToDestNodeStruct[kStar].queuedTransUnits);
                   processTransUnits(key, *k);
               }
               else{
                   //if kStar == -1, means no transactions left in queue
                   break;
               }
           }
        // TODO: processTransUnits should keep going till the balance is exhausted or queue is empty - you want
        // to identify separately the two cases and either move on to the next link or stop processing 
        // newer transactions only if balances on all payment channels are exhausted or all dest queues
        // are empty
    }
}

/* helper function to calculate the destination with the maximum CPI weight
 * that we should send transactions to on this payment channel
 */
int routerNodeCeler::findKStar(int neighborNode){
    int destNode = -1;
    int highestCPI = -1;
    for (int i = 0; i < _numHostNodes; ++i) { //initialize debt queues map
        if (nodeToDestNodeStruct[i].queuedTransUnits.size()> 0){
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

    double channelInbalance = neighbor->statAmtReceived - neighbor->statAmtSent; //received - sent
    double Q_ik = _nodeToDebtQueue[myIndex()][destNode];
    double Q_jk = _nodeToDebtQueue[neighborNode][destNode];

    double W_ijk = Q_ik - Q_jk + _celerBeta*channelInbalance;
    neighbor->destToCPIValue[destNode] = W_ijk;
    return W_ijk;
}

/* updates debt queue information (removing from it) before performing the regular
 * routine of forwarding a transction only if there's balance on the payment channel
 */
bool routerNodeCeler::forwardTransactionMessage(routerMsg *msg, int nextNode, simtime_t arrivalTime) {
    transactionMsg *transMsg = check_and_cast<transactionMsg *>(msg->getEncapsulatedPacket());
    int transactionId = transMsg->getTransactionId();
    PaymentChannel *neighbor = &(nodeToPaymentChannel[nextNode]);
    int dest = transMsg->getReceiver();
    int amt = transMsg->getAmount();

    if (neighbor->balance <= 0 || transMsg->getAmount() > neighbor->balance){
        return false;
    }
    else {
        //append next node to the route of the routerMsg
        vector<int> newRoute = msg->getRoute();
        newRoute.push_back(nextNode);
        transToNextHop[transactionId] = nextNode;
        msg->setRoute(newRoute);

        //decrement the local view of total amount in queue to the destination
        nodeToDestNodeStruct[dest].totalAmtInQueue -= transMsg->getAmount();

        //decrement the global debt queue
        _nodeToDebtQueue[myIndex()][dest] -= transMsg->getAmount();

        //increment statAmtSent for channel in-balance calculations
        neighbor->statAmtSent+=  transMsg->getAmount();

        return routerNodeBase::forwardTransactionMessage(msg, nextNode, arrivalTime);
    }
    return true;
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
