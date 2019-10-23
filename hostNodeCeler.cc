#include "hostNodeCeler.h"

Define_Module(hostNodeCeler);

/* initialization function to initialize parameters */
void hostNodeCeler::initialize(){
    hostNodeBase::initialize();
    if (myIndex() == 0) {
        _celerBeta = 0.2;
        _nodeToDebtQueue = {};
        for (int i = 0; i < _numNodes; ++i) { //initialize debt queues map, one for each node (host and router)
            _nodeToDebtQueue[i] = {};
        }
    }

    for (int i = 0; i < _numHostNodes; ++i) { //initialize debt queues values to 0 for each dest/host node
        _nodeToDebtQueue[myIndex()][i] = 0;
    }
}


/* TODO: KATHY - rewrite this
 * main routine for handling transaction messages for celer
 */
void hostNodeCeler::handleTransactionMessageSpecialized(routerMsg* ttmsg){
    transactionMsg *transMsg = check_and_cast<transactionMsg *>(ttmsg->getEncapsulatedPacket());
    int hopCount = ttmsg->getHopCount();
    int destNode = transMsg->getReceiver();
    vector<tuple<int, double , routerMsg *, Id, simtime_t>> *q;
    //int nextNode = ttmsg->getRoute()[hopCount+1];
    int transactionId = transMsg->getTransactionId();

    // txn at receiver
    if (myIndex() == destNode) { //if is at the destination
        //increment statAmtReceived
        int prevNode = ttmsg->getRoute()[ttmsg->getHopCount() - 1];
        PaymentChannel *prevNeighbor = &(nodeToPaymentChannel[prevNode]);
        prevNeighbor->statAmtReceived += transMsg->getAmount();
        handleTransactionMessage(ttmsg, false);  //bool revisit == false? - KATHY: think this is right...
    }
    else {

        // transaction received at sender

        //add transaction to appropriate queue (sorted based on dest node)

        //send transactions if possible (possible if: 1) more transaction received [here], 2) more funds unlocked)

        //at the sender
        int destNode = transMsg->getReceiver();
        DestNodeStruct *destStruct = &(nodeToDestNodeStruct[destNode]);

        q = &(destStruct->queuedTransUnits);
        tuple<int,int > key = make_tuple(transMsg->getTransactionId(), transMsg->getHtlcIndex());

        /* KATHY - ???
		   if (!revisit)
		   transMsg->setTimeAttempted(simTime().dbl());
         */

        // mark the arrival
        /* KATHY - ???
		   neighbor->arrivalTimeStamps.push_back(make_tuple(transMsg->getAmount(), simTime()));
		   neighbor->sumArrivalWindowTxns += transMsg->getAmount();
		   if (neighbor->arrivalTimeStamps.size() > _serviceArrivalWindow) {
		   double frontAmt = get<0>(neighbor->serviceArrivalTimeStamps.front());
		   neighbor->arrivalTimeStamps.pop_front();
		   neighbor->sumArrivalWindowTxns -= frontAmt;
		   }
         */

        // if there is insufficient balance at the first node, return failure
        if (false){ //KATHY - ??? _hasQueueCapacity && _queueCapacity == 0) {
            if (forwardTransactionMessage(ttmsg, destNode, simTime()) == false) {
                routerMsg * failedAckMsg = generateAckMessage(ttmsg, false);
                handleAckMessage(failedAckMsg);
            }
        }
        else if (false){ // KATHY - ??? _hasQueueCapacity && getTotalAmount(nextNode) >= _queueCapacity) {
            // there are other transactions ahead in the queue so don't attempt to forward
            routerMsg * failedAckMsg = generateAckMessage(ttmsg, false);
            handleAckMessage(failedAckMsg);
        }
        else{
            // add to queue and process in order of queue
            (*q).push_back(make_tuple(transMsg->getPriorityClass(), transMsg->getAmount(),
                    ttmsg, key, simTime()));

            destStruct->totalAmtInQueue += transMsg->getAmount();

            // because hostNode, don't need to increment statNumReceived

            //update global debt queue
            _nodeToDebtQueue[myIndex()][destNode] += transMsg->getAmount();
            push_heap((*q).begin(), (*q).end()); //, _schedulingAlgorithm); //TODO: KATHY - why is this crashing?
            celerProcessTransactions();

        }
    }
}

void hostNodeCeler::celerProcessTransactions(int endLinkNode){
    if (endLinkNode != -1){
        int kStar = findKStar(endLinkNode);
        if (kStar >= 0){
            vector<tuple<int, double , routerMsg *, Id, simtime_t>> *k;
            k = &(nodeToDestNodeStruct[kStar].queuedTransUnits);
            processTransUnits(endLinkNode, *k);
        }
    }
    else{
        cout<< "here5" << endl;
        // for each payment channel (nextNode), choose a k*/destNode queue to use as q*, and send as much as possible
        for(auto iter = nodeToPaymentChannel.begin(); iter != nodeToPaymentChannel.end(); ++iter)
        {
            int key =iter->first; //node

            int kStar = findKStar(key);
            if (kStar >= 0){
                vector<tuple<int, double , routerMsg *, Id, simtime_t>> *k;
                k = &(nodeToDestNodeStruct[kStar].queuedTransUnits);
                processTransUnits(key, *k);
            }

            //KATHY - TODO: Question: proessTransUnits might not use up all the balance on the payment channel,
            //should we do this for-loop multiple times?
        }
    }

}

int hostNodeCeler::findKStar(int endLinkNode){
    int destNode = -1;
    int highestCPI = -1;
    for (int i = 0; i < _numHostNodes; ++i) { //initialize debt queues map
        if (nodeToDestNodeStruct[destNode].queuedTransUnits.size()> 0){
            double CPI = calculateCPI(i,endLinkNode); //calculateCPI(destNode, neighbor)
            if (destNode == -1 || (CPI > highestCPI)){
                destNode = i;
                highestCPI = CPI;
            }
        }
    }
    return destNode;
}

double hostNodeCeler::calculateCPI(int destNode, int neighborNode){
    PaymentChannel *neighbor = &(nodeToPaymentChannel[neighborNode]);

    double channelInbalance = neighbor->statAmtReceived - neighbor->statAmtSent; //received - sent
    double Q_ik = _nodeToDebtQueue[myIndex()][destNode];
    double Q_jk = _nodeToDebtQueue[neighborNode][destNode];

    double W_ijk = Q_ik - Q_jk + _celerBeta*channelInbalance;

    return W_ijk;
}

/* TODO: KATHY - need to write this
 *
 *
 * */
bool hostNodeCeler::forwardTransactionMessage(routerMsg *msg, int dest, simtime_t arrivalTime) {
    transactionMsg *transMsg = check_and_cast<transactionMsg *>(msg->getEncapsulatedPacket());
    //int nextDest = msg->getRoute()[msg->getHopCount()+1];
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

        hostNodeBase::forwardTransactionMessage(msg, dest, arrivalTime);
        return true;
    }
    return true;
}


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
