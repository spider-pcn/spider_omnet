#include "hostNodeCeler.h"

Define_Module(hostNodeCeler);

// global debt queue and beta value
unordered_map<int, unordered_map<int, double>> _nodeToDebtQueue;
double _celerBeta;

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

    cout << "host before register signals" << endl;
    for ( auto it = nodeToPaymentChannel.begin(); it!= nodeToPaymentChannel.end(); it++){
        PaymentChannel *p = &(it->second);
        int id = it->first;

        for (int destNode = 0; destNode < _numHostNodes; destNode++){
            simsignal_t signal;
            signal = registerSignalPerChannelPerDest("cpi", id, destNode);
            //naming: signal_(paymentChannel endNode)destNode
            p->destToCPISignal[destNode] = signal;
            p->destToCPIValue[destNode] = -1;


        }

    }
    cout << "host after register signals" << endl;
}

/* handler for the statistic message triggered every x seconds
 */
void hostNodeCeler::handleStatMessage(routerMsg* ttmsg){
    if (_signalsEnabled) {

        for ( auto it = nodeToPaymentChannel.begin(); it!= nodeToPaymentChannel.end(); it++){
            int node = it->first; //key
            PaymentChannel* p = &(nodeToPaymentChannel[node]);

            for (auto destNode = 0; destNode < _numHostNodes; destNode++){
                emit(p->destToCPISignal[destNode], p->destToCPIValue[destNode]);
            }
        }
    }

    // call the base method to output rest of the stats
    hostNodeBase::handleStatMessage(ttmsg);
}

/* main routine for handling transaction messages for celer
 * first adds transactions to the appropriate per destination queue at a router
 * and then processes transactions in order of the destination with highest CPI
 */
void hostNodeCeler::handleTransactionMessageSpecialized(routerMsg* ttmsg){
    transactionMsg *transMsg = check_and_cast<transactionMsg *>(ttmsg->getEncapsulatedPacket());
    int hopCount = ttmsg->getHopCount();
    int destNode = transMsg->getReceiver();
    vector<tuple<int, double , routerMsg *, Id, simtime_t>> *q;
    int transactionId = transMsg->getTransactionId();

    // process transaction at sender/receiver
    if (myIndex() == destNode) { 
        //if at destination, increment stats and generate ack
        int prevNode = ttmsg->getRoute()[ttmsg->getHopCount() - 1];
        PaymentChannel *prevNeighbor = &(nodeToPaymentChannel[prevNode]);
        prevNeighbor->statAmtReceived += transMsg->getAmount();
        handleTransactionMessage(ttmsg, false); 
    }
    else {
        // transaction received at sender
        // add transaction to appropriate queue (sorted based on dest node)
        int destNode = transMsg->getReceiver();
        DestNodeStruct *destStruct = &(nodeToDestNodeStruct[destNode]);
        q = &(destStruct->queuedTransUnits);
        tuple<int,int > key = make_tuple(transMsg->getTransactionId(), transMsg->getHtlcIndex());

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
        transMsg->setTimeAttempted(simTime().dbl());

        // if there is insufficient balance at the first node, return failure
        if (_hasQueueCapacity && _queueCapacity == 0) {
            if (forwardTransactionMessage(ttmsg, destNode, simTime()) == false) {
                routerMsg * failedAckMsg = generateAckMessage(ttmsg, false);
                handleAckMessage(failedAckMsg);
            }
        }
        else if (false){ //_hasQueueCapacity && getTotalAmount(nextNode) >= _queueCapacity) {
            // there are other transactions ahead in the queue so don't attempt to forward
            routerMsg * failedAckMsg = generateAckMessage(ttmsg, false);
            handleAckMessage(failedAckMsg);
        }
        else{
            // add to queue, udpate debt queue and process in order of queue
            (*q).push_back(make_tuple(transMsg->getPriorityClass(), transMsg->getAmount(),
                    ttmsg, key, simTime()));
            destStruct->totalAmtInQueue += transMsg->getAmount();
            _nodeToDebtQueue[myIndex()][destNode] += transMsg->getAmount();
            push_heap((*q).begin(), (*q).end(), _schedulingAlgorithm); 
            
            celerProcessTransactions();
        }
    }
}

/* helper function to process transactions to the neighboring node if there are transactions to 
 * be sent on this payment channel, if one is passed in
 * otherwise use any payment channel to send out transactions
 */
void hostNodeCeler::celerProcessTransactions(int neighborNode){
    // -1 denotes you can send on any of the links and not a particualr one
    if (neighborNode != -1){ 
        int kStar = findKStar(neighborNode);
        if (kStar >= 0){
            vector<tuple<int, double , routerMsg *, Id, simtime_t>> *q;
            q = &(nodeToDestNodeStruct[kStar].queuedTransUnits);
            processTransUnits(neighborNode, *q);
        }
    }
    else{
        cout<< "here5" << endl;
        // for each payment channel (nextNode), choose a k*
        // destNode queue to use as q*, and send as much as possible
        // TODO: maybe randomize this order so its not deterministically gonna congest the smallest link


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

        // TODO: processTransUnits will keep going till the balance is exhausted or queue is empty - you want
        // to identify separately the two cases and either move on to the next link or stop processing 
        // newer transactions only if balances on all payment channels are exhausted or all dest queues
        // are empty
    }
}


/* helper function to find the destination that has the maximum CPI weight for the 
 * payment channel with passed in neighbor node so that you process its transactions next
 */
int hostNodeCeler::findKStar(int neighborNode){
    int destNode = -1;
    int highestCPI = -1;
    for (int i = 0; i < _numHostNodes; ++i) { //initialize debt queues map
        if (nodeToDestNodeStruct[i].queuedTransUnits.size() > 0){
            double CPI = calculateCPI(i, neighborNode); //calculateCPI(destNode, neighbor)
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

    double channelInbalance = neighbor->statAmtReceived - neighbor->statAmtSent; //received - sent
    double Q_ik = _nodeToDebtQueue[myIndex()][destNode];
    double Q_jk = _nodeToDebtQueue[neighborNode][destNode];

    double W_ijk = Q_ik - Q_jk + _celerBeta*channelInbalance;

    return W_ijk;
}

/* updates debt queue information (removing from it) before performing the regular
 * routine of forwarding a transction only if there's balance on the payment channel
 */
bool hostNodeCeler::forwardTransactionMessage(routerMsg *msg, int dest, simtime_t arrivalTime) {
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
        if (newRoute.size() == 0){
            newRoute.push_back(myIndex());
        }
        newRoute.push_back(dest);
        msg->setRoute(newRoute);

        //decrement debt queue stats and payment stats
        nodeToDestNodeStruct[dest].totalAmtInQueue -= transMsg->getAmount();
        _nodeToDebtQueue[myIndex()][dest] -= transMsg->getAmount();
        neighbor->statAmtSent+=  transMsg->getAmount();

        return hostNodeBase::forwardTransactionMessage(msg, dest, arrivalTime);
    }
    return true;
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
