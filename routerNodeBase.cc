#include "routerNodeBase.h"
#include "hostInitialize.h"
#include <queue>

Define_Module(routerNodeBase);

int routerNodeBase::myIndex(){
   return getIndex() + _numHostNodes;
}


/* helper function to go through all transactions in a queue and print their details out
 * meant for debugging 
 */
void routerNodeBase::checkQueuedTransUnits(vector<tuple<int, double, routerMsg*,  Id, simtime_t>> queuedTransUnits, 
        int nextNode){
    if (queuedTransUnits.size() > 0) {
        cout << "simTime(): " << simTime() << endl;
        cout << "queuedTransUnits size: " << queuedTransUnits.size() << endl;
        for (auto q: queuedTransUnits) {
            routerMsg* msg = get<2>(q);
            transactionMsg *transMsg = check_and_cast<transactionMsg *>(msg->getEncapsulatedPacket());

            if (transMsg->getHasTimeOut()){
                cout << "(" << (transMsg->getTimeSent() + transMsg->getTimeOut()) 
                     << "," << transMsg->getTransactionId() << "," << nextNode << ") ";
            }
            else {
                cout << "(-1) ";
            }
        }
        cout << endl;
        for (auto c: canceledTransactions) {
            cout << "(" << get<0>(c) << "," <<get<1>(c) <<","<< get<2>(c)<< "," << get<3>(c)<< ") ";
        }
        cout << endl;
    }
}


/* helper method to print channel balances 
 */
void routerNodeBase:: printNodeToPaymentChannel(){
    printf("print of channels\n" );
    for (auto i : nodeToPaymentChannel){
        printf("(key: %d, %f )",i.first, i.second.balance);
    }
    cout<<endl;
}

/* get total amount on queue to node x */
double routerNodeBase::getTotalAmount(int x) {
    return nodeToPaymentChannel[x].totalAmtInQueue;
} 

/* get total amount inflight incoming node x */
double routerNodeBase::getTotalAmountIncomingInflight(int x) {
    return nodeToPaymentChannel[x].totalAmtIncomingInflight;
} 

/* get total amount inflight outgoing node x */
double routerNodeBase::getTotalAmountOutgoingInflight(int x) {
    return nodeToPaymentChannel[x].totalAmtOutgoingInflight;
} 


/* register a signal per channel of the particular type passed in
 * and return the signal created
 */
simsignal_t routerNodeBase::registerSignalPerChannel(string signalStart, int id) {
    string signalPrefix = signalStart + "PerChannel";
    char signalName[64];
    string templateString = signalPrefix + "Template";

    if (id < _numHostNodes){
        sprintf(signalName, "%s(host %d)", signalPrefix.c_str(), id);
    } else{
        sprintf(signalName, "%s(router %d [%d] )", signalPrefix.c_str(),
                id - _numHostNodes, id);
    }
    simsignal_t signal = registerSignal(signalName);
    cProperty *statisticTemplate = getProperties()->get("statisticTemplate", 
            templateString.c_str());
    getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
    return signal;
}


/* initialize  basic
 * per channel information as well as default signals for all
 * payment channels 
 * */
void routerNodeBase::initialize()
{
    // Assign gates to all payment channels
    const char * gateName = "out";
    cGate *destGate = NULL;

    int i = 0;
    int gateSize = gate(gateName, 0)->size();

    do {
        destGate = gate(gateName, i++);
        cGate *nextGate = destGate->getNextGate();
        if (nextGate ) {
            PaymentChannel temp =  {};
            temp.gate = destGate;
            bool isHost = nextGate->getOwnerModule()->par("isHost");
            int key = nextGate->getOwnerModule()->getIndex();
            if (!isHost){
                key = key + _numHostNodes;
            }
            nodeToPaymentChannel[key] = temp; 
        }
    } while (i < gateSize);
    
    //initialize everything for adjacent nodes/nodes with payment channel to me
    for(auto iter = nodeToPaymentChannel.begin(); iter != nodeToPaymentChannel.end(); ++iter)
    {
        int key =  iter->first; //node

        //fill in balance field of nodeToPaymentChannel
        nodeToPaymentChannel[key].balance = _balances[make_tuple(myIndex(),key)];
        nodeToPaymentChannel[key].balanceEWMA = nodeToPaymentChannel[key].balance;

        // intialize capacity
        double balanceOpp =  _balances[make_tuple(key, myIndex())];
        nodeToPaymentChannel[key].origTotalCapacity = nodeToPaymentChannel[key].balance + balanceOpp;
      
        //initialize queuedTransUnits
        vector<tuple<int, double , routerMsg *, Id, simtime_t>> temp;
        make_heap(temp.begin(), temp.end(), sortFIFO);
        nodeToPaymentChannel[key].queuedTransUnits = temp;
       
        //register PerChannel signals
        if (_signalsEnabled) {
            simsignal_t signal;
            signal = registerSignalPerChannel("numInQueue", key);
            nodeToPaymentChannel[key].amtInQueuePerChannelSignal = signal;

            signal = registerSignalPerChannel("balance", key);
            nodeToPaymentChannel[key].balancePerChannelSignal = signal;

            signal = registerSignalPerChannel("queueDelayEWMA", key);
            nodeToPaymentChannel[key].queueDelayEWMASignal = signal;

            signal = registerSignalPerChannel("numInflight", key);
            nodeToPaymentChannel[key].numInflightPerChannelSignal = signal;

            signal = registerSignalPerChannel("timeInFlight", key);
            nodeToPaymentChannel[key].timeInFlightPerChannelSignal = signal;
        }
    }
    
    // generate statistic message
    routerMsg *statMsg = generateStatMessage();
    scheduleAt(simTime() + 0, statMsg);

    // generate time out clearing messages
    if (_timeoutEnabled) {
        routerMsg *clearStateMsg = generateClearStateMessage();
        scheduleAt(simTime() + _clearRate, clearStateMsg);
    }
}

/* function that is called at the end of the simulation that
 * deletes any remaining messages and records scalars
 */
void routerNodeBase::finish(){
    deleteMessagesInQueues();
    
    double numPoints = (_transStatEnd - _transStatStart)/(double) _statRate;
    double sumRebalancing = 0;
    double sumAmtAdded = 0;

    // iterate through all payment channels and print last summary statistics for queues
    for ( auto it = nodeToPaymentChannel.begin(); it!= nodeToPaymentChannel.end(); it++){ 
        int node = it->first; //key
        char buffer[30];
        sprintf(buffer, "queueSize %d -> %d", myIndex(), node);
        recordScalar(buffer, nodeToPaymentChannel[node].queueSizeSum/numPoints);

        sumRebalancing += nodeToPaymentChannel[node].numRebalanceEvents;
        sumAmtAdded += nodeToPaymentChannel[node].amtAdded;
    }
    
    // total amount of rebalancing incurred by this node
    char buffer[30];
    sprintf(buffer, "totalNumRebalancingEvents %d", myIndex());
    recordScalar(buffer, sumRebalancing);
    sprintf(buffer, "totalAmtAdded %d", myIndex());
    recordScalar(buffer, sumAmtAdded);
}

/*
 *  given an adjacent node, and TransUnit queue of things to send to that node, sends
 *  TransUnits until channel funds are too low
 *  calls forwardTransactionMessage on every individual TransUnit
 */
void routerNodeBase:: processTransUnits(int dest, vector<tuple<int, double , routerMsg *, Id, simtime_t>>& q) {
    bool successful = true;
    while ((int)q.size() > 0 && successful) {
        pop_heap(q.begin(), q.end(), sortFIFO);
        successful = forwardTransactionMessage(get<2>(q.back()), dest, get<4>(q.back()));
        if (successful){
            q.pop_back();
        }
    }
}


/* helper method to delete the messages in any payment channel queues at the end 
 * of the experiment 
 */
void routerNodeBase::deleteMessagesInQueues(){
    for (auto iter = nodeToPaymentChannel.begin(); iter!=nodeToPaymentChannel.end(); iter++){
        int key = iter->first;
        
        for (auto temp = (nodeToPaymentChannel[key].queuedTransUnits).begin();
            temp!= (nodeToPaymentChannel[key].queuedTransUnits).end(); ){
            routerMsg * rMsg = get<2>(*temp);
            auto tMsg = rMsg->getEncapsulatedPacket();
            rMsg->decapsulate();
            delete tMsg;
            delete rMsg;
            temp = (nodeToPaymentChannel[key].queuedTransUnits).erase(temp);
        }
    }
}



/********** MESSAGE GENERATOR ********************/
/* generates messages responsible for recognizing that a txn is complete
 * and funds have been securely transferred from a previous node to a 
 * neighboring node after the ack/secret has been received
 * Always goes only one hop, no more
 */
routerMsg *routerNodeBase::generateUpdateMessage(int transId, 
        int receiver, double amount, int htlcIndex){
    char msgname[MSGSIZE];
    sprintf(msgname, "tic-%d-to-%d updateMsg", myIndex(), receiver);
    
    routerMsg *rMsg = new routerMsg(msgname);
    vector<int> route={myIndex(),receiver};
    rMsg->setRoute(route);
    rMsg->setHopCount(0);
    rMsg->setMessageType(UPDATE_MSG);

    updateMsg *uMsg = new updateMsg(msgname);
    uMsg->setAmount(amount);
    uMsg->setTransactionId(transId);
    uMsg->setHtlcIndex(htlcIndex);
    rMsg->encapsulate(uMsg);
    return rMsg;
}

/* generate statistic trigger message every x seconds
 * to output all statistics which can then be plotted
 */
routerMsg *routerNodeBase::generateStatMessage(){
    char msgname[MSGSIZE];
    sprintf(msgname, "node-%d statMsg", myIndex());
    routerMsg *rMsg = new routerMsg(msgname);
    rMsg->setMessageType(STAT_MSG);
    return rMsg;
}

/* generate a periodic message to remove
 * any state pertaining to transactions that have 
 * timed out
 */
routerMsg *routerNodeBase::generateClearStateMessage(){
    char msgname[MSGSIZE];
    sprintf(msgname, "node-%d clearStateMsg", myIndex());
    routerMsg *rMsg = new routerMsg(msgname);
    rMsg->setMessageType(CLEAR_STATE_MSG);
    return rMsg;
}

/* called only when a router in between the sender-receiver path
 * wants to send a failure ack due to insfufficent funds
 * similar to the endhost ack
 * isSuccess denotes whether the ack is in response to a transaction
 * that succeeded or failed.
 */
routerMsg *routerNodeBase::generateAckMessage(routerMsg* ttmsg, bool isSuccess) {
    int sender = (ttmsg->getRoute())[0];
    int receiver = (ttmsg->getRoute())[(ttmsg->getRoute()).size() -1];
    vector<int> route = ttmsg->getRoute();

    transactionMsg *transMsg = check_and_cast<transactionMsg *>(ttmsg->getEncapsulatedPacket());
    int transactionId = transMsg->getTransactionId();
    double timeSent = transMsg->getTimeSent();
    double amount = transMsg->getAmount();
    bool hasTimeOut = transMsg->getHasTimeOut();
    
    char msgname[MSGSIZE];
    sprintf(msgname, "receiver-%d-to-sender-%d ackMsg", receiver, sender);
    routerMsg *msg = new routerMsg(msgname);
    ackMsg *aMsg = new ackMsg(msgname);
    aMsg->setTransactionId(transactionId);
    aMsg->setIsSuccess(isSuccess);
    aMsg->setTimeSent(timeSent);
    aMsg->setAmount(amount);
    aMsg->setReceiver(transMsg->getReceiver());
    aMsg->setHasTimeOut(hasTimeOut);
    aMsg->setHtlcIndex(transMsg->getHtlcIndex());
    aMsg->setPathIndex(transMsg->getPathIndex());
    aMsg->setLargerTxnId(transMsg->getLargerTxnId());
    aMsg->setPriorityClass(transMsg->getPriorityClass());
    aMsg->setTimeOut(transMsg->getTimeOut());
    aMsg->setIsMarked(transMsg->getIsMarked());
    if (!isSuccess){
        aMsg->setFailedHopNum((route.size()-1) - ttmsg->getHopCount());
    }

    //no need to set secret - not modelled
    reverse(route.begin(), route.end());
    msg->setRoute(route);

    //need to reverse path from current hop number in case of partial failure
    msg->setHopCount((route.size()-1) - ttmsg->getHopCount());

    msg->setMessageType(ACK_MSG); 
    ttmsg->decapsulate();
    delete transMsg;
    delete ttmsg;
    msg->encapsulate(aMsg);
    return msg;
}



/****** MESSAGE FORWARDERS ********/
/* responsible for forwarding all messages but transactions which need special care
 * in particular, looks up the next node's interface and sends out the message
 */
void routerNodeBase::forwardMessage(routerMsg* msg){
   // Increment hop count.
   msg->setHopCount(msg->getHopCount()+1);
   //use hopCount to find next destination
   int nextDest = msg->getRoute()[msg->getHopCount()];
   if (_loggingEnabled) cout << "forwarding " << msg->getMessageType() << " at " 
       << simTime() << endl;
   send(msg, nodeToPaymentChannel[nextDest].gate);
}

/*
 *  Given a message representing a TransUnit, increments hopCount, finds next destination,
 *  adjusts (decrements) channel balance, sends message to next node on route
 *  as long as it isn't cancelled
 */
bool routerNodeBase::forwardTransactionMessage(routerMsg *msg, int dest, simtime_t arrivalTime) {
    transactionMsg *transMsg = check_and_cast<transactionMsg *>(msg->getEncapsulatedPacket());
    int nextDest = msg->getRoute()[msg->getHopCount()+1];
    int transactionId = transMsg->getTransactionId();
    PaymentChannel *neighbor = &(nodeToPaymentChannel[nextDest]);
    int amt = transMsg->getAmount();

    if (neighbor->balance <= 0 || transMsg->getAmount() > neighbor->balance){
        return false;
    }
    else {
        // return true directly if txn has been cancelled
        // so that you waste not resources on this and move on to a new txn
        // if you return false processTransUnits won't look for more txns
        auto iter = canceledTransactions.find(make_tuple(transactionId, 0, 0, 0, 0));
        if (iter != canceledTransactions.end()) {
            msg->decapsulate();
            delete transMsg;
            delete msg;
            neighbor->totalAmtInQueue -= amt;
            return true;
        }

        // update state to send transaction out
        msg->setHopCount(msg->getHopCount()+1);

        // update service arrival times
        neighbor->serviceArrivalTimeStamps.push_back(make_tuple(transMsg->getAmount(), simTime(), arrivalTime));
        neighbor->sumServiceWindowTxns += transMsg->getAmount();
        if (neighbor->serviceArrivalTimeStamps.size() > _serviceArrivalWindow) {
            double frontAmt = get<0>(neighbor->serviceArrivalTimeStamps.front());
            neighbor->serviceArrivalTimeStamps.pop_front(); 
            neighbor->sumServiceWindowTxns -= frontAmt;
        }

        // add amount to outgoing map, mark time sent
        Id thisTrans = make_tuple(transMsg->getTransactionId(), transMsg->getHtlcIndex());
        (neighbor->outgoingTransUnits)[thisTrans] = transMsg->getAmount();
        neighbor->txnSentTimes[thisTrans] = simTime();
        neighbor->totalAmtOutgoingInflight += transMsg->getAmount();
      
        // update balance
        double newBalance = neighbor->balance - amt;
        neighbor->balance = newBalance;
        neighbor-> balanceEWMA = (1 -_ewmaFactor) * neighbor->balanceEWMA + 
            (_ewmaFactor) * newBalance;
        neighbor->totalAmtInQueue -= amt;

        if (_loggingEnabled) cout << "forwardTransactionMsg send: " << simTime() << endl;
        send(msg, nodeToPaymentChannel[nextDest].gate);
        return true;
    } 
}

/* overall controller for handling messages that dispatches the right function
 * based on message type
 */
void routerNodeBase::handleMessage(cMessage *msg){
    routerMsg *ttmsg = check_and_cast<routerMsg *>(msg);
 
    //Radhika TODO: figure out what's happening here
    if (simTime() > _simulationLength){
        auto encapMsg = (ttmsg->getEncapsulatedPacket());
        ttmsg->decapsulate();
        delete ttmsg;
        delete encapMsg;
        return;
    }

    // handle all messges by type
    switch (ttmsg->getMessageType()) {
        case ACK_MSG:
            if (_loggingEnabled) 
                cout << "[ROUTER "<< myIndex() <<": RECEIVED ACK MSG] " << msg->getName() << endl;
            if (_timeoutEnabled)
                handleAckMessageTimeOut(ttmsg);
            handleAckMessage(ttmsg);
            if (_loggingEnabled) cout << "[AFTER HANDLING:]" <<endl;
            break;

        case TRANSACTION_MSG: 
            { 
                if (_loggingEnabled) 
                    cout<< "[ROUTER "<< myIndex() <<": RECEIVED TRANSACTION MSG]  "
                     << msg->getName() <<endl;
             
                if (_timeoutEnabled && handleTransactionMessageTimeOut(ttmsg)){
                    return;
                }
                handleTransactionMessage(ttmsg);
                if (_loggingEnabled) cout << "[AFTER HANDLING:]" << endl;
            }
            break;

        case UPDATE_MSG:
            if (_loggingEnabled) cout<< "[ROUTER "<< myIndex() 
                <<": RECEIVED UPDATE MSG] "<< msg->getName() << endl;
                handleUpdateMessage(ttmsg);
            if (_loggingEnabled) cout<< "[AFTER HANDLING:]  "<< endl;
            break;

        case STAT_MSG:
            if (_loggingEnabled) cout<< "[ROUTER "<< myIndex() 
                <<": RECEIVED STAT MSG] "<< msg->getName() << endl;
                handleStatMessage(ttmsg);
            if (_loggingEnabled) cout<< "[AFTER HANDLING:]  "<< endl;
            break;

        case TIME_OUT_MSG:
            if (_loggingEnabled) cout<< "[ROUTER "<< myIndex() 
                <<": RECEIVED TIME_OUT_MSG] "<< msg->getName() << endl;
       
            if (!_timeoutEnabled){
                cout << "timeout message generated when it shouldn't have" << endl;
                return;
            }

                handleTimeOutMessage(ttmsg);
            if (_loggingEnabled) cout<< "[AFTER HANDLING:]  "<< endl;
            break;
        
        case CLEAR_STATE_MSG:
            if (_loggingEnabled) cout<< "[ROUTER "<< myIndex() 
                <<": RECEIVED CLEAR_STATE_MSG] "<< msg->getName() << endl;
                handleClearStateMessage(ttmsg);
            if (_loggingEnabled) cout<< "[AFTER HANDLING:]  "<< endl;
            break;
        
        default:
                handleMessage(ttmsg);

    }

}

/* Main handler for normal processing of a transaction
 * checks if message has reached sender
 *      1. has reached (is in rev direction)  - turn transactionMsg into ackMsg, forward ackMsg
 *      2. has not reached yet - add to appropriate job queue q, process q as
 *          much as we have funds for
 */
void routerNodeBase::handleTransactionMessage(routerMsg* ttmsg) {
    transactionMsg *transMsg = check_and_cast<transactionMsg *>(ttmsg->getEncapsulatedPacket());
    int hopcount = ttmsg->getHopCount();
    vector<tuple<int, double , routerMsg *, Id, simtime_t>> *q;
    int destination = transMsg->getReceiver();
    int transactionId = transMsg->getTransactionId();

    // add to incoming trans units
    int prevNode = ttmsg->getRoute()[ttmsg->getHopCount()-1];
    unordered_map<Id, double, hashId> *incomingTransUnits = &(nodeToPaymentChannel[prevNode].incomingTransUnits);
    (*incomingTransUnits)[make_tuple(transMsg->getTransactionId(), transMsg->getHtlcIndex())] = transMsg->getAmount();
    nodeToPaymentChannel[prevNode].totalAmtIncomingInflight += transMsg->getAmount();

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

    // ignore if txn is already cancelled
    auto iter = canceledTransactions.find(make_tuple(transactionId, 0, 0, 0, 0));
    if ( iter!=canceledTransactions.end() ){
        //delete yourself, message won't be encountered again
        ttmsg->decapsulate();
        delete transMsg;
        delete ttmsg;
        return;
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
        push_heap((*q).begin(), (*q).end(), sortFIFO);
        processTransUnits(nextNode, *q);
    }
}

/* handler responsible for prematurely terminating the processing
 * of a transaction if it has timed out and deleteing it. Returns
 * true if the transaction is timed out so that no special handlers
 * are called after
 */
bool routerNodeBase::handleTransactionMessageTimeOut(routerMsg* ttmsg) {
    transactionMsg *transMsg = check_and_cast<transactionMsg *>(ttmsg->getEncapsulatedPacket());
    int transactionId = transMsg->getTransactionId();

    // look for transaction in cancelled txn set and delete if present
    auto iter = canceledTransactions.find(make_tuple(transactionId, 0, 0, 0, 0));
    if ( iter!=canceledTransactions.end() ){
        //delete yourself
        ttmsg->decapsulate();
        delete transMsg;
        delete ttmsg;
        return true;
    }
    else{
        return false;
    }
}


/*  Default action for time out message that is responsible for either recognizing
 *  that txn is complete and timeout is a noop or inserting the transaction into 
 *  a cancelled transaction list
 *  The actual cancellation/clearing of the state happens on the clear state 
 *  message
 *  Will only be encountered in forward direction
 */
void routerNodeBase::handleTimeOutMessage(routerMsg* ttmsg){
    timeOutMsg *toutMsg = check_and_cast<timeOutMsg *>(ttmsg->getEncapsulatedPacket());
    int nextNode = (ttmsg->getRoute())[ttmsg->getHopCount()+1];
    int prevNode = (ttmsg->getRoute())[ttmsg->getHopCount()-1];
    
    CanceledTrans ct = make_tuple(toutMsg->getTransactionId(), simTime(), prevNode, nextNode, toutMsg->getReceiver());
    canceledTransactions.insert(ct);
    forwardMessage(ttmsg);
}

/* default routine for handling an ack that is responsible for 
 * updating outgoing transunits and incoming trans units 
 * and triggering an update message to the next node on the path
 * before forwarding the ack back to the previous node
 */

void routerNodeBase::handleAckMessage(routerMsg* ttmsg){
    assert(myIndex() == ttmsg->getRoute()[ttmsg->getHopCount()]);
    ackMsg *aMsg = check_and_cast<ackMsg *>(ttmsg->getEncapsulatedPacket());

    // this is previous node on the ack path, so next node on the forward path
    // remove txn from outgone txn list
    Id thisTrans = make_tuple(aMsg->getTransactionId(), aMsg->getHtlcIndex());
    int prevNode = ttmsg->getRoute()[ttmsg->getHopCount()-1];
    PaymentChannel *prevChannel = &(nodeToPaymentChannel[prevNode]);
    double timeInflight = (simTime() - prevChannel->txnSentTimes[thisTrans]).dbl();
    (prevChannel->outgoingTransUnits).erase(thisTrans);
    (prevChannel->txnSentTimes).erase(thisTrans);
    prevChannel->totalAmtOutgoingInflight -= aMsg->getAmount();
   
    if (aMsg->getIsSuccess() == false){
        // increment funds on this channel unless this is the node that caused the fauilure
        // in which case funds were never decremented in the first place
        if (aMsg->getFailedHopNum() < ttmsg->getHopCount()) {
            double updatedBalance = prevChannel->balance + aMsg->getAmount();
            prevChannel->balanceEWMA = 
                (1 -_ewmaFactor) * prevChannel->balanceEWMA + (_ewmaFactor) * updatedBalance; 
            prevChannel->balance = updatedBalance;
        }
        
        // this is nextNode on the ack path and so prev node in the forward path or rather
        // node sending you mayments
        int nextNode = ttmsg->getRoute()[ttmsg->getHopCount()+1];
        unordered_map<Id, double, hashId> *incomingTransUnits = &(nodeToPaymentChannel[nextNode].incomingTransUnits);
        (*incomingTransUnits).erase(make_tuple(aMsg->getTransactionId(), aMsg->getHtlcIndex()));
        nodeToPaymentChannel[nextNode].totalAmtIncomingInflight -= aMsg->getAmount();
    }
    else { 
        // mark the time it spent inflight
        prevChannel->sumTimeInFlight += timeInflight;
        prevChannel->timeInFlightSamples += 1;
        
        routerMsg* uMsg =  generateUpdateMessage(aMsg->getTransactionId(), prevNode, 
                aMsg->getAmount(), aMsg->getHtlcIndex() );
        prevChannel->numUpdateMessages += 1;
        forwardMessage(uMsg);
    }
    forwardMessage(ttmsg);
}

/* handles the logic for ack messages in the presence of timeouts
 * in particular, removes the transaction from the cancelled txns
 * to mark that it has been received 
 */
void routerNodeBase::handleAckMessageTimeOut(routerMsg* ttmsg){
    ackMsg *aMsg = check_and_cast<ackMsg *>(ttmsg->getEncapsulatedPacket());
    int transactionId = aMsg->getTransactionId();
    
    auto iter = canceledTransactions.find(make_tuple(transactionId, 0, 0, 0, 0));
    if (iter != canceledTransactions.end()) {
        canceledTransactions.erase(iter);
    }
}

/*
 * handleUpdateMessage - called when receive update message, increment back funds, see if we can
 *      process more jobs with new funds, delete update message
 */
void routerNodeBase::handleUpdateMessage(routerMsg* msg) {
    vector<tuple<int, double , routerMsg *, Id, simtime_t>> *q;
    int prevNode = msg->getRoute()[msg->getHopCount()-1];
    updateMsg *uMsg = check_and_cast<updateMsg *>(msg->getEncapsulatedPacket());
    PaymentChannel *prevChannel = &(nodeToPaymentChannel[prevNode]);
   
    //increment the in flight funds back
    double newBalance = prevChannel->balance + uMsg->getAmount();
    prevChannel->balance =  newBalance;       
    prevChannel->balanceEWMA = (1 -_ewmaFactor) * prevChannel->balanceEWMA 
        + (_ewmaFactor) * newBalance; 
    
    if (_rebalancingEnabled) {
        if (newBalance > _rebalancingUpFactor * prevChannel->origTotalCapacity) {
            prevChannel->balance = prevChannel->origTotalCapacity;
            tuple<int, int> senderReceiverTuple = (prevNode < myIndex()) ? make_tuple(prevNode, myIndex()) :
                make_tuple(myIndex(), prevNode);
            _capacities[senderReceiverTuple] -= (_rebalancingUpFactor - 1)*prevChannel->origTotalCapacity;

            prevChannel->numRebalanceEvents += 1;
            prevChannel->amtAdded -= (_rebalancingUpFactor - 1)*prevChannel->origTotalCapacity;
        }
    }

    //remove transaction from incoming_trans_units
    unordered_map<Id, double, hashId> *incomingTransUnits = &(prevChannel->incomingTransUnits);
    (*incomingTransUnits).erase(make_tuple(uMsg->getTransactionId(), uMsg->getHtlcIndex()));
    prevChannel->totalAmtIncomingInflight -= uMsg->getAmount();

    msg->decapsulate();
    delete uMsg;
    delete msg; //delete update message

    //see if we can send more jobs out
    q = &(prevChannel->queuedTransUnits);
    processTransUnits(prevNode, *q);
} 



/* emits all the default statistics across all the schemes
 * until the end of the simulation
 */
void routerNodeBase::handleStatMessage(routerMsg* ttmsg){
    // reschedule this message to be sent again
    if (simTime() > _simulationLength){
        delete ttmsg;
    }
    else {
        scheduleAt(simTime()+_statRate, ttmsg);
    }
    
    // per channel Stats
    for (auto it = nodeToPaymentChannel.begin(); it!= nodeToPaymentChannel.end(); it++){
        PaymentChannel *p = &(it->second);

        if (simTime() > _transStatStart && simTime() < _transStatEnd)
            p->queueSizeSum += getTotalAmount(it->first);

        if (_signalsEnabled) {
            emit(p->amtInQueuePerChannelSignal, getTotalAmount(it->first));
            emit(p->balancePerChannelSignal, p->balance);
            emit(p->numInflightPerChannelSignal, getTotalAmountIncomingInflight(it->first) +
                    getTotalAmountOutgoingInflight(it->first));
            emit(p->queueDelayEWMASignal, p->queueDelayEWMA);

            emit(p->timeInFlightPerChannelSignal, p->sumTimeInFlight/p->timeInFlightSamples);
            p->sumTimeInFlight = 0;
            p->timeInFlightSamples = 0;
        }
    }
}

/* handler that is responsible for removing all the state associated
 * with a cancelled transaction once its grace period has passed
 * this included removal from outgoing/incoming units and any
 * queues
 */
void routerNodeBase::handleClearStateMessage(routerMsg* ttmsg){
    //reschedule for the next interval
    if (simTime() > _simulationLength){
        delete ttmsg;
    }
    else{
        scheduleAt(simTime()+_clearRate, ttmsg);
    }

    /* hack for now to do this periodically */
    if (_rebalancingEnabled && !_priceSchemeEnabled) {
        for ( auto it = nodeToPaymentChannel.begin(); it!= nodeToPaymentChannel.end(); it++ ) {
            PaymentChannel *neighborChannel = &(nodeToPaymentChannel[it->first]);   

            auto lastTransTimes =  neighborChannel->serviceArrivalTimeStamps.back();
            double curQueueingDelay = get<1>(lastTransTimes).dbl() - get<2>(lastTransTimes).dbl();
            neighborChannel->queueDelayEWMA = 0.6*curQueueingDelay + 0.4*neighborChannel->queueDelayEWMA;

            if (neighborChannel->queueDelayEWMA > _queueDelayThreshold) {
                neighborChannel->balance += getTotalAmount(it->first);
                tuple<int, int> senderReceiverTuple = 
                    (it->first < myIndex()) ? make_tuple(it->first, myIndex()) :
                    make_tuple(myIndex(), it->first);
                    _capacities[senderReceiverTuple] += getTotalAmount(it->first);
            
                neighborChannel->numRebalanceEvents += 1; 
                processTransUnits(it->first, neighborChannel->queuedTransUnits);
            }
        }
    }

    for ( auto it = canceledTransactions.begin(); it!= canceledTransactions.end(); ) {       
        int transactionId = get<0>(*it);
        simtime_t msgArrivalTime = get<1>(*it);
        int prevNode = get<2>(*it);
        int nextNode = get<3>(*it);
        int destNode = get<4>(*it);
        
        // if grace period has passed
        if (simTime() > (msgArrivalTime + _maxTravelTime)){
            // remove from queue to next node
            if (nextNode != -1){   
                vector<tuple<int, double, routerMsg*, Id, simtime_t>>* queuedTransUnits = 
                    &(nodeToPaymentChannel[nextNode].queuedTransUnits);

                auto iterQueue = find_if((*queuedTransUnits).begin(),
                  (*queuedTransUnits).end(),
                  [&transactionId](const tuple<int, double, routerMsg*, Id, simtime_t>& p)
                  { return (get<0>(get<3>(p)) == transactionId); });
                
                // delete all occurences of this transaction in the queue
                // especially if there are splits
                if (iterQueue != (*queuedTransUnits).end()){
                    routerMsg * rMsg = get<2>(*iterQueue);
                    auto tMsg = rMsg->getEncapsulatedPacket();
                    rMsg->decapsulate();
                    delete tMsg;
                    delete rMsg;
                    nodeToPaymentChannel[nextNode].totalAmtInQueue -= get<1>(*iterQueue);
                    iterQueue = (*queuedTransUnits).erase(iterQueue);
                    
                    /*iterQueue = find_if((*queuedTransUnits).begin(),
                     (*queuedTransUnits).end(),
                     [&transactionId](const tuple<int, double, routerMsg*, Id, simtime_t>& p)
                     { return (get<0>(get<3>(p)) == transactionId); });*/
                }
                
                // resort the queue based on priority
                make_heap((*queuedTransUnits).begin(), (*queuedTransUnits).end(), 
                        sortFIFO);
            }

            // remove from incoming TransUnits from the previous node
            if (prevNode != -1){
                unordered_map<Id, double, hashId> *incomingTransUnits = 
                    &(nodeToPaymentChannel[prevNode].incomingTransUnits);
                auto iterIncoming = find_if((*incomingTransUnits).begin(),
                  (*incomingTransUnits).end(),
                  [&transactionId](const pair<tuple<int, int >, double> & p)
                  { return get<0>(p.first) == transactionId; });
                
                if (iterIncoming != (*incomingTransUnits).end()){
                    nodeToPaymentChannel[prevNode].totalAmtIncomingInflight -= iterIncoming->second;
                    iterIncoming = (*incomingTransUnits).erase(iterIncoming);
                }
            }

            // remove from outgoing transUnits to nextNode and restore balance on own end
            if (nextNode != -1){
                unordered_map<Id, double, hashId> *outgoingTransUnits = 
                    &(nodeToPaymentChannel[nextNode].outgoingTransUnits);
                
                auto iterOutgoing = find_if((*outgoingTransUnits).begin(),
                  (*outgoingTransUnits).end(),
                  [&transactionId](const pair<tuple<int, int >, double> & p)
                  { return get<0>(p.first) == transactionId; });
                
                if (iterOutgoing != (*outgoingTransUnits).end()){
                    double amount = iterOutgoing -> second;
                    iterOutgoing = (*outgoingTransUnits).erase(iterOutgoing);
              
                    PaymentChannel *nextChannel = &(nodeToPaymentChannel[nextNode]);
                    double updatedBalance = nextChannel->balance + amount;
                    nextChannel->balance = updatedBalance; 
                    nextChannel->balanceEWMA = (1 -_ewmaFactor) * nextChannel->balanceEWMA + 
                        (_ewmaFactor) * updatedBalance;
                    nextChannel->totalAmtOutgoingInflight -= amount;
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
