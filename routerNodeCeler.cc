#include "routerNodeCeler.h"

Define_Module(routerNodeCeler);



/* initialization function to initialize parameters */
void routerNodeCeler::initialize(){
    routerNodeBase::initialize();

    for (int i = 0; i < _numHostNodes; ++i) { //initialize debt queues values to 0 for each dest/host node
        _nodeToDebtQueue[myIndex()][i] = 0;
    }
}


/* TODO: KATHY - rewrite this
 * main routine for handling transaction messages for celer
 * that initiates probes and splits transactions according to latest probes
 */

void routerNodeCeler::handleTransactionMessage(routerMsg* ttmsg){
    transactionMsg *transMsg = check_and_cast<transactionMsg *>(ttmsg->getEncapsulatedPacket());
     int hopcount = ttmsg->getHopCount();
     vector<tuple<int, double , routerMsg *, Id, simtime_t>> *q;
     int destination = transMsg->getReceiver();
     int transactionId = transMsg->getTransactionId();

     // ignore if txn is already cancelled
     auto iter = canceledTransactions.find(make_tuple(transactionId, 0, 0, 0, 0));
     if ( iter!=canceledTransactions.end() ){
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

     /*
     // find the outgoing channel to check capacity/ability to send on it
     int nextNode = ttmsg->getRoute()[hopcount+1];
     PaymentChannel *neighbor = &(nodeToPaymentChannel[nextNode]);
     q = &(neighbor->queuedTransUnits);

     // mark the arrival on this payment channel
     neighbor->arrivalTimeStamps.push_back(make_tuple(transMsg->getAmount(), simTime()));
     neighbor->sumArrivalWindowTxns += transMsg->getAmount();
     if (neighbor->arrivalTimeStamps.size() > _serviceArrivalWindow) {
         double frontAmt = get<0>(neighbor->serviceArrivalTimeStamps.front());
         neighbor->arrivalTimeStamps.pop_front();
         neighbor->sumArrivalWindowTxns -= frontAmt;
     }


     // if balance is insufficient at the first node, return failure ack
     if (_hasQueueCapacity && _queueCapacity == 0) {
         if (forwardTransactionMessage(ttmsg, nextNode, simTime()) == false) {
             routerMsg * failedAckMsg = generateAckMessage(ttmsg, false);
             handleAckMessage(failedAckMsg);
         }
     } else if (_hasQueueCapacity && _queueCapacity<= getTotalAmount(nextNode)) {
         //failed transaction, queue at capacity, others are in queue so no point trying this txn
         routerMsg * failedAckMsg = generateAckMessage(ttmsg, false);
         handleAckMessage(failedAckMsg);
     } else {
         // add to queue and process in order of priority
         (*q).push_back(make_tuple(transMsg->getPriorityClass(), transMsg->getAmount(),
                ttmsg, make_tuple(transMsg->getTransactionId(), transMsg->getHtlcIndex()), simTime()));
         neighbor->totalAmtInQueue += transMsg->getAmount();
         push_heap((*q).begin(), (*q).end(), _schedulingAlgorithm);
         processTransUnits(nextNode, *q);
     }
*/
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

                 // TODO: KATHY - do above for router node
                 nodeToPaymentChannel[prevNode].statAmtReceived +=  transMsg->getAmount();
                 //update global debt queue
                 _nodeToDebtQueue[myIndex()][destNode] += transMsg->getAmount();

                 push_heap((*q).begin(), (*q).end(), _schedulingAlgorithm);

                 // for each payment channel (nextNode), choose a k*/destNode queue to use as q*, and send as much as possible
                 for(auto iter = nodeToPaymentChannel.begin(); iter != nodeToPaymentChannel.end(); ++iter)
                 {
                     int key =iter->first; //node

                     int kStar = findKStar(key);
                     if (kStar >= 0){
                         vector<tuple<int, double , routerMsg *, Id, simtime_t>> *k;
                         k = &(nodeToDestNodeStruct[destNode].queuedTransUnits);
                         processTransUnits(key, *k);
                     }

                     //KATHY - TODO: Question: proessTransUnits might not use up all the balance on the payment channel,
                     //should we do this for-loop multiple times?
                 }
             }

}


int routerNodeCeler::findKStar(int endLinkNode){
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

double routerNodeCeler::calculateCPI(int destNode, int neighborNode){
    PaymentChannel *neighbor = &(nodeToPaymentChannel[neighborNode]);

    double channelInbalance = neighbor->statAmtReceived - neighbor->statAmtSent; //received - sent
    double Q_ik = _nodeToDebtQueue[myIndex()][destNode];
    double Q_jk = _nodeToDebtQueue[neighborNode][destNode];

    double W_ijk = Q_ik - Q_jk + _celerBeta*channelInbalance;

    return W_ijk;
}

/* TODO: KATHY - need to write this
 * handles forwarding of  transactions out of the queue
 * the way other schemes' routers do except that it marks the packet
 * if the queue is larger than the threshold, therfore mostly similar to the base code */

bool routerNodeCeler::forwardTransactionMessage(routerMsg *msg, int dest, simtime_t arrivalTime) {
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

        routerNodeBase::forwardTransactionMessage(msg, dest, arrivalTime);
    }
}

