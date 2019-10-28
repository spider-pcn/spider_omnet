#include "routerNodeCeler.h"

Define_Module(routerNodeCeler);

/* initialization function to initialize debt queues to 0 */
void routerNodeCeler::initialize(){
    routerNodeBase::initialize();

    for (int i = 0; i < _numHostNodes; ++i) { 
        _nodeToDebtQueue[myIndex()][i] = 0;
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

    // if there is insufficient balance at the first node, return failure
    if (_hasQueueCapacity && _queueCapacity == 0) {
        if (forwardTransactionMessage(ttmsg, destNode, simTime()) == false) {
            routerMsg * failedAckMsg = generateAckMessage(ttmsg, false);
            handleAckMessage(failedAckMsg);
        }
    }
    else if (_hasQueueCapacity && getTotalAmount(nextNode) >= _queueCapacity) {
        // there are other transactions ahead in the queue so don't attempt to forward
        routerMsg * failedAckMsg = generateAckMessage(ttmsg, false);
        handleAckMessage(failedAckMsg);
    }
    else{
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
            k = &(nodeToDestNodeStruct[kStar].queuedTransUnits);
            processTransUnits(neighborNode, *q);
        }
    }
    else{
        // for each payment channel (nextNode), choose a k*/
        // destNode queue to use as q*, and send as much as possible
        // TODO: maybe randomize this ordering so you don't deterministically flood the first
        // payment channel
        for(auto iter = nodeToPaymentChannel.begin(); iter != nodeToPaymentChannel.end(); ++iter)
        {
            int key = iter->first; //node
            int kStar = findKStar(key);
            if (kStar >= 0){
                vector<tuple<int, double , routerMsg *, Id, simtime_t>> *q;
                k = &(nodeToDestNodeStruct[kStar].queuedTransUnits);
                processTransUnits(key, *q);
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

    return W_ijk;
}

/* updates debt queue information (removing from it) before performing the regular
 * routine of forwarding a transction only if there's balance on the payment channel
 */
bool routerNodeCeler::forwardTransactionMessage(routerMsg *msg, int dest, simtime_t arrivalTime) {
    transactionMsg *transMsg = check_and_cast<transactionMsg *>(msg->getEncapsulatedPacket());
    int transactionId = transMsg->getTransactionId();
    PaymentChannel *neighbor = &(nodeToPaymentChannel[dest]);
    int amt = transMsg->getAmount();

    if (neighbor->balance <= 0 || transMsg->getAmount() > neighbor->balance){
        return false;
    }
    else {
        //append next destination to the route of the routerMsg
        vector<int> newRoute = msg->getRoute();
        newRoute.push_back(dest);
        msg->setRoute(newRoute);

        //decrement the local view of total amount in queue
        nodeToDestNodeStruct[dest].totalAmtInQueue -= transMsg->getAmount();

        //decrement the global debt queue
        _nodeToDebtQueue[myIndex()][dest] -= transMsg->getAmount();

        //increment statAmtSent for channel in-balance calculations
        neighbor->statAmtSent+=  transMsg->getAmount();

        return routerNodeBase::forwardTransactionMessage(msg, dest, arrivalTime);
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
