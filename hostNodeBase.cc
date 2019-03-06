#include "hostNodeBase.h"
#include <queue>

bool _windowEnabled;

Define_Module(hostNodeBase);

void hostNodeBase::setIndex(int index) {
    this->index = index;
}

int hostNodeBase::myIndex() {
    return index;
}

/* print channel information */
bool hostNodeBase:: printNodeToPaymentChannel(){
    bool invalidKey = false;
    printf("print of channels\n" );
    for (auto i : nodeToPaymentChannel){
       printf("(key: %d )",i.first);
       if (i.first<0) invalidKey = true;
    }
    cout<<endl;
    return !invalidKey;
}

/* print any vector */
void printVectorDouble(vector<double> v){
    for (auto temp : v){
        cout << temp << ", ";
    }
    cout << endl;
}


/* samples a random number (index) of the passed in vector
 *  based on the actual probabilities passed in
 */
int hostNodeBase::sampleFromDistribution(vector<double> probabilities) {
    vector<double> cumProbabilities { 0 };

    double sumProbabilities = accumulate(probabilities.begin(), probabilities.end(), 0.0); 
    assert(sumProbabilities <= 1.0);
    
    // compute cumulative probabilities
    for (int i = 0; i < probabilities.size(); i++) {
        cumProbabilities.push_back(probabilities[i] + cumProbabilities[i]);
    }

    // generate the next index to send on based on these probabilities
    double value  = (rand() / double(RAND_MAX));
    int index;
    for (int i = 1; i < cumProbabilities.size(); i++) {
        if (value < cumProbabilities[i])
            return i - 1;
    }
    return 0;
}


/* generate next Transaction to be processed at this node 
 * this is an optimization to prevent all txns from being loaded initially
 */
void hostNodeBase::generateNextTransaction() {
      if (_transUnitList[myIndex()].empty())
          return;
      TransUnit j = _transUnitList[myIndex()].top();
      double timeSent = j.timeSent;
      
      routerMsg *msg = generateTransactionMessage(j); //TODO: flag to whether to calculate path

      if (_waterfillingEnabled || _priceSchemeEnabled || _landmarkRoutingEnabled){
         vector<int> blankPath = {};
         //Radhika TODO: maybe no need to compute path to begin with?
         msg->setRoute(blankPath);
      }

      scheduleAt(timeSent, msg);

      if (_timeoutEnabled && !_priceSchemeEnabled && j.hasTimeOut){
         routerMsg *toutMsg = generateTimeOutMessage(msg);
         scheduleAt(timeSent + j.timeOut, toutMsg );
      }
      _transUnitList[myIndex()].pop();
}




/* register a signal per destination for this path of the particular type passed in
 * and return the signal created
 */
simsignal_t hostNodeBase::registerSignalPerDestPath(string signalStart, int pathIdx, int destNode) {
    string signalPrefix = signalStart + "PerDestPerPath";
    char signalName[64];
    string templateString = signalPrefix + "Template";
    
    if (destNode < _numHostNodes){
        sprintf(signalName, "%s_%d(host %d)", signalPrefix.c_str(), pathIdx, destNode);
    } else{
        sprintf(signalName, "%s_%d(router %d [%d] )", signalPrefix.c_str(),
             pathIdx, destNode - _numHostNodes, destNode);
    }
    simsignal_t signal = registerSignal(signalName);
    cProperty *statisticTemplate = getProperties()->get("statisticTemplate", 
            templateString.c_str());
    getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
    return signal;
}


/* register a signal per channel of the particular type passed in
 * and return the signal created
 */
simsignal_t hostNodeBase::registerSignalPerChannel(string signalStart, int id) {
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


/* register a signal per dest of the particular type passed in
 * and return the signal created
 */
simsignal_t hostNodeBase::registerSignalPerDest(string signalStart, int destNode, string suffix) {
    string signalPrefix = signalStart + "PerDest" + suffix;
    char signalName[64];
    string templateString = signalStart + "PerDestTemplate"; 

    sprintf(signalName, "%s(host node %d)", signalPrefix.c_str(), destNode);  
    simsignal_t signal = registerSignal(signalName);
    cProperty *statisticTemplate = getProperties()->get("statisticTemplate", 
            templateString.c_str());
    getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);

    return signal;
}


/****** MESSAGE GENERATORS **********/

/* Main function responsible for using TransUnit object and 
 * returning corresponding routerMsg message with encapsulated transactionMsg inside.
 *      note: calls get_route function to get route from sender to receiver
 */
routerMsg *hostNodeBase::generateTransactionMessage(TransUnit unit) {
    char msgname[MSGSIZE];
    sprintf(msgname, "tic-%d-to-%d transactionMsg", unit.sender, unit.receiver);
    
    transactionMsg *msg = new transactionMsg(msgname);
    msg->setAmount(unit.amount);
    msg->setTimeSent(unit.timeSent);
    msg->setSender(unit.sender);
    msg->setReceiver(unit.receiver);
    msg->setPriorityClass(unit.priorityClass);
    msg->setTransactionId(msg->getId());
    msg->setHtlcIndex(0);
    msg->setHasTimeOut(unit.hasTimeOut);
    msg->setTimeOut(unit.timeOut);
    
    sprintf(msgname, "tic-%d-to-%d router-transaction-Msg", unit.sender, unit.receiver);
    
    routerMsg *rMsg = new routerMsg(msgname);
    // compute route only once
    if (destNodeToPath.count(unit.receiver) == 0){ 
       vector<int> route = getRoute(unit.sender,unit.receiver);
       destNodeToPath[unit.receiver] = route;
       rMsg->setRoute(route);
    }
    else{
       rMsg->setRoute(destNodeToPath[unit.receiver]);
    }
    rMsg->setHopCount(0);
    rMsg->setMessageType(TRANSACTION_MSG);
    rMsg->encapsulate(msg);
    return rMsg;
}



/* called only when a transactionMsg reaches end of its path to mark
 * the acknowledgement and receipt of the transaction at the receiver,
 * we assume no delay in procuring the key and that the receiver 
 * immediately starts sending an ack in the opposite direction that
 * unlocks funds along the reverse path
 * isSuccess denotes whether the ack is in response to a transaction
 * that succeeded or failed.
 */
routerMsg *hostNodeBase::generateAckMessage(routerMsg* ttmsg, bool isSuccess) {
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



/* generates messages responsible for recognizing that a txn is complete
 * and funds have been securely transferred from a previous node to a 
 * neighboring node after the ack/secret has been received
 * Always goes only one hop, no more
 */
routerMsg *hostNodeBase::generateUpdateMessage(int transId, 
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
routerMsg *hostNodeBase::generateStatMessage(){
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
routerMsg *hostNodeBase::generateClearStateMessage(){
    char msgname[MSGSIZE];
    sprintf(msgname, "node-%d clearStateMsg", myIndex());
    routerMsg *rMsg = new routerMsg(msgname);
    rMsg->setMessageType(CLEAR_STATE_MSG);
    return rMsg;
}


/* responsible for generating the generic time out message 
 * generated whenever transaction is sent out into the network
 */
routerMsg *hostNodeBase::generateTimeOutMessage(routerMsg* msg) {
    transactionMsg *transMsg = check_and_cast<transactionMsg *>(msg->getEncapsulatedPacket());
    char msgname[MSGSIZE];
    sprintf(msgname, "tic-%d-to-%d timeOutMsg", transMsg->getSender(), transMsg->getReceiver());
   
    timeOutMsg *toutMsg = new timeOutMsg(msgname);
    toutMsg->setTransactionId(transMsg->getTransactionId());
    toutMsg->setReceiver(transMsg->getReceiver());

    sprintf(msgname, "tic-%d-to-%d routerTimeOutMsg(%f)", 
            transMsg->getSender(), transMsg->getReceiver(), transMsg->getTimeSent());
   
    routerMsg *rMsg = new routerMsg(msgname);
    rMsg->setRoute(msg->getRoute());
    rMsg->setHopCount(0);
    rMsg->setMessageType(TIME_OUT_MSG);
    rMsg->encapsulate(toutMsg);
    return rMsg;
}



/***** MESSAGE HANDLERS *****/
/* overall controller for handling messages that dispatches the right function
 * based on message type
 */
void hostNodeBase::handleMessage(cMessage *msg){
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
                cout << "[HOST "<< myIndex() <<": RECEIVED ACK MSG] " << msg->getName() << endl;
            if (_timeoutEnabled)
                handleAckMessageTimeOut(ttmsg);
            handleAckMessageSpecialized(ttmsg);
            if (_loggingEnabled) cout << "[AFTER HANDLING:]" <<endl;
            break;

        case TRANSACTION_MSG: 
            { 
                if (_loggingEnabled) 
                    cout<< "[HOST "<< myIndex() <<": RECEIVED TRANSACTION MSG]  "
                     << msg->getName() <<endl;
             
                transactionMsg *transMsg = 
                    check_and_cast<transactionMsg *>(ttmsg->getEncapsulatedPacket());
                if (transMsg->isSelfMessage() && simTime() == transMsg->getTimeSent()) {
                    generateNextTransaction();
                }
             
                if (_timeoutEnabled && handleTransactionMessageTimeOut(ttmsg)){
                    return;
                }
                handleTransactionMessageSpecialized(ttmsg);
                if (_loggingEnabled) cout << "[AFTER HANDLING:]" << endl;
            }
            break;

        case UPDATE_MSG:
            if (_loggingEnabled) cout<< "[HOST "<< myIndex() 
                <<": RECEIVED UPDATE MSG] "<< msg->getName() << endl;
                handleUpdateMessage(ttmsg);
            if (_loggingEnabled) cout<< "[AFTER HANDLING:]  "<< endl;
            break;

        case STAT_MSG:
            if (_loggingEnabled) cout<< "[HOST "<< myIndex() 
                <<": RECEIVED STAT MSG] "<< msg->getName() << endl;
                handleStatMessage(ttmsg);
            if (_loggingEnabled) cout<< "[AFTER HANDLING:]  "<< endl;
            break;

        case TIME_OUT_MSG:
            if (_loggingEnabled) cout<< "[HOST "<< myIndex() 
                <<": RECEIVED TIME_OUT_MSG] "<< msg->getName() << endl;
       
            if (!_timeoutEnabled){
                cout << "timeout message generated when it shouldn't have" << endl;
                return;
            }

                handleTimeOutMessage(ttmsg);
            if (_loggingEnabled) cout<< "[AFTER HANDLING:]  "<< endl;
            break;
        
        case CLEAR_STATE_MSG:
            if (_loggingEnabled) cout<< "[HOST "<< myIndex() 
                <<": RECEIVED CLEAR_STATE_MSG] "<< msg->getName() << endl;
                handleClearStateMessage(ttmsg);
            if (_loggingEnabled) cout<< "[AFTER HANDLING:]  "<< endl;
            break;
        
        default:
                handleMessage(ttmsg);

    }

}

void hostNodeBase::handleTransactionMessageSpecialized(routerMsg *ttmsg) {
    handleTransactionMessage(ttmsg);
}

/* Main handler for normal processing of a transaction
 * checks if message has reached sender
 *      1. has reached  - turn transactionMsg into ackMsg, forward ackMsg
 *      2. has not reached yet - add to appropriate job queue q, process q as
 *          much as we have funds for
 */
void hostNodeBase::handleTransactionMessage(routerMsg* ttmsg, bool revisit){
    transactionMsg *transMsg = check_and_cast<transactionMsg *>(ttmsg->getEncapsulatedPacket());
    int hopcount = ttmsg->getHopCount();
    vector<tuple<int, double , routerMsg *, Id>> *q;
    int destination = transMsg->getReceiver();
    int transactionId = transMsg->getTransactionId();
    
    if (!revisit) {
        statRateArrived[destination] += 1;
        statRateAttempted[destination] += 1;
    }
    
    // if it is at the destination
    if (ttmsg->getHopCount() ==  ttmsg->getRoute().size() - 1) {
        // add to incoming trans units 
        int prevNode = ttmsg->getRoute()[ttmsg->getHopCount() - 1];
        map<Id, double> *incomingTransUnits = &(nodeToPaymentChannel[prevNode].incomingTransUnits);
        (*incomingTransUnits)[make_tuple(transMsg->getTransactionId(), transMsg->getHtlcIndex())] = 
            transMsg->getAmount();
        
        if (_timeoutEnabled){
            //TODO: what if we have multiple HTLC's per transaction id?
            auto iter = find_if(canceledTransactions.begin(),
               canceledTransactions.end(),
               [&transactionId](const tuple<int, simtime_t, int, int, int>& p)
               { return get<0>(p) == transactionId; });
            if (iter!=canceledTransactions.end()) {
                canceledTransactions.erase(iter);
            }
        }
        // send ack even if it has timed out because txns wait till _maxTravelTime before being 
        // cleared by clearState
        routerMsg* newMsg =  generateAckMessage(ttmsg);
        forwardMessage(newMsg);
        return;
    } 
    else{
        //at the sender
        int destNode = transMsg->getReceiver();
        int nextNode = ttmsg->getRoute()[hopcount+1];
        q = &(nodeToPaymentChannel[nextNode].queuedTransUnits);
        tuple<int,int > key = make_tuple(transMsg->getTransactionId(), transMsg->getHtlcIndex());

        // if there is insufficient balance at the first node, return failure
        if (_hasQueueCapacity && _queueCapacity == 0) {
            if (forwardTransactionMessage(ttmsg) == false) {
                routerMsg * failedAckMsg = generateAckMessage(ttmsg, false);
                handleAckMessage(failedAckMsg);
            }
        }
        else if (_hasQueueCapacity && (*q).size() >= _queueCapacity) {
            // there are other transactions ahead in the queue so don't attempt to forward 
            routerMsg * failedAckMsg = generateAckMessage(ttmsg, false);
            handleAckMessage(failedAckMsg);
        }
        else{
            // add to queue and process in order of queue
            (*q).push_back(make_tuple(transMsg->getPriorityClass(), transMsg->getAmount(),
                  ttmsg, key));
            push_heap((*q).begin(), (*q).end(), sortPriorityThenAmtFunction);
            processTransUnits(nextNode, *q);
        }
    }
}


/* handler responsible for prematurely terminating the processing
 * of a transaction if it has timed out and deleteing it. Returns
 * true if the transaction is timed out so that no special handlers
 * are called after
 */
bool hostNodeBase::handleTransactionMessageTimeOut(routerMsg* ttmsg) {
    transactionMsg *transMsg = check_and_cast<transactionMsg *>(ttmsg->getEncapsulatedPacket());
    int transactionId = transMsg->getTransactionId();

    // look for transaction in cancelled txn set and delete if present
    auto iter = find_if(canceledTransactions.begin(),
         canceledTransactions.end(),
         [&transactionId](const tuple<int, simtime_t, int, int, int>& p)
         { return get<0>(p) == transactionId; });
    
    if ( iter!=canceledTransactions.end() ){
        if (getIndex() == transMsg->getSender()) {
            statNumTimedOutAtSender[transMsg->getReceiver()] = 
                statNumTimedOutAtSender[transMsg->getReceiver()] + 1;
        }

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
 */
void hostNodeBase::handleTimeOutMessage(routerMsg* ttmsg){
    timeOutMsg *toutMsg = check_and_cast<timeOutMsg *>(ttmsg->getEncapsulatedPacket());
    int destination = toutMsg->getReceiver();
    int transactionId = toutMsg->getTransactionId();
    
    if (ttmsg->isSelfMessage()) {
            if (successfulDoNotSendTimeOut.count(transactionId) > 0) {
                successfulDoNotSendTimeOut.erase(toutMsg->getTransactionId());
                ttmsg->decapsulate();
                delete toutMsg;
                delete ttmsg;
            }
            else {
                int nextNode = (ttmsg->getRoute())[ttmsg->getHopCount()+1];
                CanceledTrans ct = make_tuple(transactionId, 
                        simTime(),-1, nextNode, destination);
                canceledTransactions.insert(ct);
                forwardMessage(ttmsg);
            }
    }
    else { 
        //is at the destination
        CanceledTrans ct = make_tuple(transactionId, simTime(), 
                (ttmsg->getRoute())[ttmsg->getHopCount()-1],-1, destination);
        canceledTransactions.insert(ct);
        ttmsg->decapsulate();
        delete toutMsg;
        delete ttmsg;
    }
}

/* specialized ack handler that does the routine if this is a shortest paths 
 * algorithm. In particular, collects stats assuming that this is the only
 * one path on which a txn might complete
 * NOTE: acks are on the reverse path relative to the original sender
 */
void hostNodeBase::handleAckMessageSpecialized(routerMsg* ttmsg) { 
    int destNode = ttmsg->getRoute()[0];
    ackMsg *aMsg = check_and_cast<ackMsg *>(ttmsg->getEncapsulatedPacket());

    if (aMsg->getIsSuccess()==false) {
        statNumFailed[destNode] = statNumFailed[destNode] + 1;
        statRateFailed[destNode] = statRateFailed[destNode] + 1;
    }
    else {
        statNumCompleted[destNode] = statNumCompleted[destNode] + 1;
        statRateCompleted[destNode] = statRateCompleted[destNode] + 1;
    }
    hostNodeBase::handleAckMessage(ttmsg);
}


/* default routine for handling an ack that is responsible for 
 * updating outgoingtransunits and incoming trans units 
 * and triggering an update message to the next node on the path
 * before forwarding the ack back to the previous node
 */
void hostNodeBase::handleAckMessage(routerMsg* ttmsg){
    assert(myIndex() == ttmsg->getRoute()[ttmsg->getHopCount()]);
    ackMsg *aMsg = check_and_cast<ackMsg *>(ttmsg->getEncapsulatedPacket());
    
    // this is previous node on the ack path, so next node on the forward path
    // remove txn from outgone txn list
    int prevNode = ttmsg->getRoute()[ttmsg->getHopCount()-1];
    map<Id, double> *outgoingTransUnits = &(nodeToPaymentChannel[prevNode].outgoingTransUnits);
    (*outgoingTransUnits).erase(make_tuple(aMsg->getTransactionId(), aMsg->getHtlcIndex()));
   
    if (aMsg->getIsSuccess() == false) {
        // increment funds on this channel unless this is the node that caused the fauilure
        // in which case funds were never decremented in the first place
        if (ttmsg->getRoute()[aMsg->getFailedHopNum()] != myIndex())
            nodeToPaymentChannel[prevNode].balance += aMsg->getAmount();

        // no relevant incoming_trans_units because no node on fwd path before this
        if (ttmsg->getHopCount() < ttmsg->getRoute().size() - 1) {
            int nextNode = ttmsg->getRoute()[ttmsg->getHopCount()-1];
            map<Id, double> *incomingTransUnits = 
                &(nodeToPaymentChannel[nextNode].incomingTransUnits);
            (*incomingTransUnits).erase(make_tuple(aMsg->getTransactionId(), 
                        aMsg->getHtlcIndex()));
        }
    }
    else { 
        routerMsg* uMsg =  generateUpdateMessage(aMsg->getTransactionId(), 
                prevNode, aMsg->getAmount(), aMsg->getHtlcIndex() );
        forwardMessage(uMsg);
    }
    
    // stats
    nodeToPaymentChannel[prevNode].statNumProcessed += 1;
    simtime_t timeTakenInMilli = 1000*(simTime() - aMsg->getTimeSent());
    // if (_signalsEnabled) emit(completionTimeSignal, timeTakenInMilli);
    
    //delete ack message
    ttmsg->decapsulate();
    delete aMsg;
    delete ttmsg;
}



/* handles to logic for ack messages in the presence of timeouts
 * in particular, removes the transaction from the cancelled txns
 * to mark that it has been received 
 * it uses the successfulDoNotSendTimeout to detect if txns have
 * been completed when handling the timeout - so insert into it here
 */
void hostNodeBase::handleAckMessageTimeOut(routerMsg* ttmsg){
    ackMsg *aMsg = check_and_cast<ackMsg *>(ttmsg->getEncapsulatedPacket());
    int transactionId = aMsg->getTransactionId();
    
    //TODO: what if there are multiple HTLC's per transaction? 
    auto iter = find_if(canceledTransactions.begin(),
         canceledTransactions.end(),
         [&transactionId](const tuple<int, simtime_t, int, int, int>& p)
         { return get<0>(p) == transactionId; });
    
    if (iter!=canceledTransactions.end()) {
        canceledTransactions.erase(iter);
    }

    // only if it isn't waterfilling
    successfulDoNotSendTimeOut.insert(aMsg->getTransactionId());
}



/*
 * handleUpdateMessage - called when receive update message, increment back funds, see if we can
 *      process more jobs with new funds, delete update message
 */
void hostNodeBase::handleUpdateMessage(routerMsg* msg) {
    vector<tuple<int, double , routerMsg *, Id>> *q;
    int prevNode = msg->getRoute()[msg->getHopCount()-1];
    updateMsg *uMsg = check_and_cast<updateMsg *>(msg->getEncapsulatedPacket());
    PaymentChannel *prevChannel = &(nodeToPaymentChannel[prevNode]);
   
    //increment the in flight funds back
    double newBalance = prevChannel->balance + uMsg->getAmount();
    prevChannel->balance =  newBalance;       
    prevChannel->balanceEWMA = (1 -_ewmaFactor) * prevChannel->balanceEWMA 
        + (_ewmaFactor) * newBalance; 

    //remove transaction from incoming_trans_units
    map<Id, double> *incomingTransUnits = &(prevChannel->incomingTransUnits);
    (*incomingTransUnits).erase(make_tuple(uMsg->getTransactionId(), uMsg->getHtlcIndex()));
    prevChannel->statNumProcessed = prevChannel->statNumProcessed + 1;

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
void hostNodeBase::handleStatMessage(routerMsg* ttmsg){
    // reschedule this message to be sent again
    if (simTime() > _simulationLength){
        delete ttmsg;
    }
    else {
        scheduleAt(simTime()+_statRate, ttmsg);
    }
    
    if (_signalsEnabled) {
        // per channel Stats
        for ( auto it = nodeToPaymentChannel.begin(); it!= nodeToPaymentChannel.end(); it++){
            PaymentChannel *p = &(it->second);
            
            emit(p->numInQueuePerChannelSignal, (p->queuedTransUnits).size());
            emit(p->balancePerChannelSignal, p->balance);
            // emit(p->numProcessedPerChannelSignal, p->statNumProcessed);
            // emit(p->numSentPerChannelSignal, p->statNumSent);
            
            p->statNumProcessed = 0;
            p->statNumSent = 0;
        }
    }

    for (auto it = 0; it < _numHostNodes; it++){ 
       // per destination stats
       if (it != getIndex()) {
           if (nodeToShortestPathsMap.count(it) > 0) {
               for (auto p: nodeToShortestPathsMap[it]) {
                   PathInfo *pathInfo = &(nodeToShortestPathsMap[it][p.first]);
                   if (_signalsEnabled) {
                       // TODO: move to waterfilling file
                       emit(pathInfo->bottleneckPerDestPerPathSignal, pathInfo->bottleneck);
                       emit(pathInfo->probabilityPerDestPerPathSignal, pathInfo->probability);
                   }
                   
                   //emit rateCompleted per path
                   emit(pathInfo->rateCompletedPerDestPerPathSignal, 
                           pathInfo->statRateCompleted);
                   pathInfo->statRateCompleted = 0;
                   
                   //emit rateAttempted per path
                   emit(pathInfo->rateAttemptedPerDestPerPathSignal, 
                           pathInfo->statRateAttempted);
                   pathInfo->statRateAttempted = 0;
               }
           }
           emit(rateAttemptedPerDestSignals[it], statRateAttempted[it]);
           statRateAttempted[it] = 0;
           
           emit(rateArrivedPerDestSignals[it], statRateArrived[it]);
           statRateArrived[it] = 0;
           
           emit(rateCompletedPerDestSignals[it], statRateCompleted[it]);
           statRateCompleted[it] = 0;

           if (_signalsEnabled) {
               if (_hasQueueCapacity){
                   emit(numFailedPerDestSignals[it], statNumFailed[it]);
                   emit(rateFailedPerDestSignals[it], statRateFailed[it]);
               }

               emit(numArrivedPerDestSignals[it], statNumArrived[it]);
               emit(numCompletedPerDestSignals[it], statNumCompleted[it]);
               emit(numTimedOutPerDestSignals[it], statNumTimedOut[it]);
               emit(numPendingPerDestSignals[it], destNodeToNumTransPending[it]);
               int frac = ((100*statNumCompleted[it])/(max(statNumArrived[it],1)));
               emit(fracSuccessfulPerDestSignals[it],frac);
           }
       }
    }
}

/* handler that is responsible for removing all the state associated
 * with a cancelled transaction once its grace period has passed
 * this included removal from outgoing/incoming units and any
 * queues
 */
void hostNodeBase::handleClearStateMessage(routerMsg* ttmsg){
    //reschedule for the next interval
    if (simTime() > _simulationLength){
        delete ttmsg;
    }
    else{
        scheduleAt(simTime()+_clearRate, ttmsg);
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
                vector<tuple<int, double, routerMsg*, Id>>* queuedTransUnits = 
                    &(nodeToPaymentChannel[nextNode].queuedTransUnits);

                auto iterQueue = find_if((*queuedTransUnits).begin(),
                  (*queuedTransUnits).end(),
                  [&transactionId](const tuple<int, double, routerMsg*, Id>& p)
                  { return (get<0>(get<3>(p)) == transactionId); });
                
                // delete all occurences of this transaction in the queue
                // especially if there are splits
                while (iterQueue != (*queuedTransUnits).end()){
                    routerMsg * rMsg = get<2>(*iterQueue);
                    auto tMsg = rMsg->getEncapsulatedPacket();
                    rMsg->decapsulate();
                    delete tMsg;
                    delete rMsg;
                    iterQueue = (*queuedTransUnits).erase(iterQueue);
                    
                    iterQueue = find_if((*queuedTransUnits).begin(),
                     (*queuedTransUnits).end(),
                     [&transactionId](const tuple<int, double, routerMsg*, Id>& p)
                     { return (get<0>(get<3>(p)) == transactionId); });
                }
                
                // resort the queue based on priority
                make_heap((*queuedTransUnits).begin(), (*queuedTransUnits).end(), 
                        sortPriorityThenAmtFunction);
            }

            // remove from incoming TransUnits from the previous node
            if (prevNode != -1){
                map<tuple<int,int>, double> *incomingTransUnits = 
                    &(nodeToPaymentChannel[prevNode].incomingTransUnits);
                auto iterIncoming = find_if((*incomingTransUnits).begin(),
                  (*incomingTransUnits).end(),
                  [&transactionId](const pair<tuple<int, int >, double> & p)
                  { return get<0>(p.first) == transactionId; });
                
                while (iterIncoming != (*incomingTransUnits).end()){
                    iterIncoming = (*incomingTransUnits).erase(iterIncoming);
                    // TODO: remove?
                    // tuple<int, int> tempId = make_tuple(transactionId, 
                    // get<1>(iterIncoming->first));
                    //
                    iterIncoming = find_if((*incomingTransUnits).begin(),
                         (*incomingTransUnits).end(),
                        [&transactionId](const pair<tuple<int, int >, double> & p)
                        { return get<0>(p.first) == transactionId; });
                }
            }

            // remove from outgoing transUnits to nextNode and restore balance on own end
            if (nextNode != -1){
                map<tuple<int,int>, double> *outgoingTransUnits = 
                    &(nodeToPaymentChannel[nextNode].outgoingTransUnits);
                
                auto iterOutgoing = find_if((*outgoingTransUnits).begin(),
                  (*outgoingTransUnits).end(),
                  [&transactionId](const pair<tuple<int, int >, double> & p)
                  { return get<0>(p.first) == transactionId; });
                
                while (iterOutgoing != (*outgoingTransUnits).end()){
                    double amount = iterOutgoing -> second;
                    iterOutgoing = (*outgoingTransUnits).erase(iterOutgoing);
              
                    PaymentChannel *nextChannel = &(nodeToPaymentChannel[nextNode]);
                    double updatedBalance = nextChannel->balance + amount;
                    nextChannel->balance = updatedBalance; 
                    nextChannel->balanceEWMA = (1 -_ewmaFactor) * nextChannel->balanceEWMA + 
                        (_ewmaFactor) * updatedBalance;

                    iterOutgoing = find_if((*outgoingTransUnits).begin(),
                        (*outgoingTransUnits).end(),
                        [&transactionId](const pair<tuple<int, int >, double> & p)
                        { return get<0>(p.first) == transactionId; });
                }
            }
            
            // all done, can remove txn and update stats
            it = canceledTransactions.erase(it);

            //end host didn't receive ack, so txn timed out 
            statNumTimedOut[destNode] = statNumTimedOut[destNode]  + 1;
        }
        else{
            it++;
        }
    }
}



/*
 *  Given a message representing a TransUnit, increments hopCount, finds next destination,
 *  adjusts (decrements) channel balance, sends message to next node on route
 *  as long as it isn't cancelled
 */
bool hostNodeBase::forwardTransactionMessage(routerMsg *msg) {
    transactionMsg *transMsg = check_and_cast<transactionMsg *>(msg->getEncapsulatedPacket());
    int nextDest = msg->getRoute()[msg->getHopCount()+1];
    int transactionId = transMsg->getTransactionId();
    PaymentChannel *neighbor = &(nodeToPaymentChannel[nextDest]);

    if (neighbor->balance <= 0 || transMsg->getAmount() > neighbor->balance){
        return false;
    }
    else {
        // return true directly if txn has been cancelled
        // so that you waste not resources on this and move on to a new txn
        // if you return false processTransUnits won't look for more txns
        auto iter = find_if(canceledTransactions.begin(),
           canceledTransactions.end(),
           [&transactionId](const tuple<int, simtime_t, int, int, int>& p)
           { return get<0>(p) == transactionId; });

        // can potentially erase info?
        if (iter != canceledTransactions.end()) {
            return true;
        }

        // update state to send transaction out
        msg->setHopCount(msg->getHopCount()+1);

        //add amount to outgoing map
        map<Id, double> *outgoingTransUnits = &(neighbor->outgoingTransUnits);
        (*outgoingTransUnits)[make_tuple(transMsg->getTransactionId(), 
                transMsg->getHtlcIndex())] = transMsg->getAmount();
        neighbor->statNumSent += 1;
      
        // update balance
        int amt = transMsg->getAmount();
        double newBalance = neighbor->balance - amt;
        neighbor->balance = newBalance;
        neighbor-> balanceEWMA = (1 -_ewmaFactor) * neighbor->balanceEWMA + 
            (_ewmaFactor) * newBalance;
        
        if (_loggingEnabled) cout << "forwardTransactionMsg send: " << simTime() << endl;
        send(msg, nodeToPaymentChannel[nextDest].gate);
        return true;
    } 
}



/* responsible for forwarding all messages but transactions which need special care
 * in particular, looks up the next node's interface and sends out the message
 */
void hostNodeBase::forwardMessage(routerMsg* msg){
   // Increment hop count.
   msg->setHopCount(msg->getHopCount()+1);
   //use hopCount to find next destination
   int nextDest = msg->getRoute()[msg->getHopCount()];
   if (_loggingEnabled) cout << "forwarding " << msg->getMessageType() << " at " 
       << simTime() << endl;
   send(msg, nodeToPaymentChannel[nextDest].gate);
}



/* initialize() all of the global parameters and basic
 * per channel information as well as default signals for all
 * payment channels and destinations
 */
void hostNodeBase::initialize() {
    completionTimeSignal = registerSignal("completionTime");
    successfulDoNotSendTimeOut = {};
    

    cout << "starting initialization" ;
    string topologyFile_ = par("topologyFile");
    string workloadFile_ = par("workloadFile");


    // initialize global parameters once
    if (getIndex() == 0){  
        _simulationLength = par("simulationLength");
        _statRate = par("statRate");
        _clearRate = par("timeoutClearRate");
        _waterfillingEnabled = par("waterfillingEnabled");
        _timeoutEnabled = par("timeoutEnabled");
        _signalsEnabled = par("signalsEnabled");
        _loggingEnabled = par("loggingEnabled");
        _priceSchemeEnabled = par("priceSchemeEnabled");

        _hasQueueCapacity = false;
        _queueCapacity = 0;


        _landmarkRoutingEnabled = false;
        if (_landmarkRoutingEnabled){
            _hasQueueCapacity = true;
            _queueCapacity = 0;
            _timeoutEnabled = false;
        }

        _epsilon = pow(10, -6);
        cout << "epsilon" << _epsilon << endl;
        
        if (_waterfillingEnabled || _priceSchemeEnabled || _landmarkRoutingEnabled){
           _kValue = par("numPathChoices");
        }

        _maxTravelTime = 0.0;
        _delta = 0.01; // to avoid divide by zero 
        setNumNodes(topologyFile_);
        // add all the TransUnits into global list
        generateTransUnitList(workloadFile_);

        //create "channels" map - from topology file
        generateChannelsBalancesMap(topologyFile_);
    }
    
    
    setIndex(getIndex());

    // Assign gates to all the payment channels
    cGate *destGate = NULL;
    cGate* curOutGate = gate("out", 0);

    int i = 0;
    int gateSize = curOutGate->size();
    do {
        cGate *nextGate = curOutGate->getNextGate();
        if (nextGate ) {
            PaymentChannel temp =  {};
            temp.gate = curOutGate;

            bool isHost = nextGate->getOwnerModule()->par("isHost");
            int key = nextGate->getOwnerModule()->getIndex();
            if (!isHost){
                key = key + _numHostNodes;
            }
            nodeToPaymentChannel[key] = temp;
        }
        i++;
        curOutGate = nextGate;
    } while (i < gateSize);


    //initialize everything for adjacent nodes/nodes with payment channel to me
    for(auto iter = nodeToPaymentChannel.begin(); iter != nodeToPaymentChannel.end(); ++iter)
    {
        int key =iter->first; //node

        //fill in balance field of nodeToPaymentChannel
        nodeToPaymentChannel[key].balance = _balances[make_tuple(myIndex(),key)];
        nodeToPaymentChannel[key].balanceEWMA = nodeToPaymentChannel[key].balance;

        // intialize capacity
        double balanceOpp =  _balances[make_tuple(key, myIndex())];
        nodeToPaymentChannel[key].totalCapacity = nodeToPaymentChannel[key].balance + balanceOpp;

        //initialize queuedTransUnits
        vector<tuple<int, double , routerMsg *, Id>> temp;
        make_heap(temp.begin(), temp.end(), sortPriorityThenAmtFunction);
        nodeToPaymentChannel[key].queuedTransUnits = temp;

        //register PerChannel signals
        simsignal_t signal;
        signal = registerSignalPerChannel("numInQueue", key);
        nodeToPaymentChannel[key].numInQueuePerChannelSignal = signal;

        signal = registerSignalPerChannel("numProcessed", key);
        nodeToPaymentChannel[key].numProcessedPerChannelSignal = signal;
        nodeToPaymentChannel[key].statNumProcessed = 0;

        signal = registerSignalPerChannel("balance", key);
        nodeToPaymentChannel[key].balancePerChannelSignal = signal;

        signal = registerSignalPerChannel("numSent", key);
        nodeToPaymentChannel[key].numSentPerChannelSignal = signal;
        nodeToPaymentChannel[key].statNumSent = 0;
    }

    //initialize signals with all other nodes in graph
    for (int i = 0; i < _numHostNodes; ++i) {
        simsignal_t signal;
        signal = registerSignalPerDest("rateCompleted", i, "_Total");
        rateCompletedPerDestSignals.push_back(signal);
        statRateCompleted.push_back(0);

        signal = registerSignalPerDest("numCompleted", i, "_Total");
        numCompletedPerDestSignals.push_back(signal);
        statNumCompleted.push_back(0);

        signal = registerSignalPerDest("rateAttempted", i, "_Total");
        rateAttemptedPerDestSignals.push_back(signal);
        statRateAttempted.push_back(0);

        signal = registerSignalPerDest("rateArrived", i, "_Total");
        rateArrivedPerDestSignals.push_back(signal);
        statRateArrived.push_back(0);

        signal = registerSignalPerDest("numArrived", i, "_Total");
        numArrivedPerDestSignals.push_back(signal);
        statNumArrived.push_back(0);

        signal = registerSignalPerDest("numFailed", i, "_Total");
        numFailedPerDestSignals.push_back(signal);
        statNumFailed.push_back(0);

        signal = registerSignalPerDest("numTimedOut", i, "_Total");
        numTimedOutPerDestSignals.push_back(signal);
        statNumTimedOut.push_back(0);

        signal = registerSignalPerDest("numPending", i, "_Total");
        numPendingPerDestSignals.push_back(signal);
        
        signal = registerSignalPerDest("fracSuccessful", i, "_Total");
        fracSuccessfulPerDestSignals.push_back(signal);

        signal = registerSignalPerDest("rateFailed", i, "");
        rateFailedPerDestSignals.push_back(signal);
        statRateFailed.push_back(0);
    }
    
    // generate first transaction
    generateNextTransaction();

    cout << "Initialized all state";

    //generate stat message
    routerMsg *statMsg = generateStatMessage();
    scheduleAt(simTime() + 0, statMsg);

    if (_timeoutEnabled){
       routerMsg *clearStateMsg = generateClearStateMessage();
       scheduleAt(simTime()+ _clearRate, clearStateMsg);
    }
}

/* function that is called at the end of the simulation that
 * deletes any remaining messages and records scalars
 */
void hostNodeBase::finish() {
    deleteMessagesInQueues();
    if (myIndex() == 0) {
        // can be done on a per node basis also if need be
        // all in seconds
        recordScalar("max travel time", _maxTravelTime);
        recordScalar("delta", _delta);
        recordScalar("average delay", _avgDelay/1000.0);
        recordScalar("epsilon", _epsilon);
    }
}


/*
 *  given an adjacent node, and TransUnit queue of things to send to that node, sends
 *  TransUnits until channel funds are too low
 *  calls forwardTransactionMessage on every individual TransUnit
 */
void hostNodeBase:: processTransUnits(int dest, vector<tuple<int, double , routerMsg *, Id>>& q) {
    bool successful = true;
    while((int)q.size() > 0 && successful) {
        successful = forwardTransactionMessage(get<2>(q.back()));
        if (successful){
            q.pop_back();
        }
    }
}


/* removes all of the cancelled messages from the queues to any
 * of the adjacent payment channels
 */
void hostNodeBase::deleteMessagesInQueues(){
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

    // remove any waiting transactions too
    for (auto iter = nodeToDestInfo.begin(); iter!=nodeToDestInfo.end(); iter++){
        int dest = iter->first;
        while ((nodeToDestInfo[dest].transWaitingToBeSent).size() > 0) {
            routerMsg * rMsg = nodeToDestInfo[dest].transWaitingToBeSent.front();
            auto tMsg = rMsg->getEncapsulatedPacket();
            rMsg->decapsulate();
            delete tMsg;
            delete rMsg;
            nodeToDestInfo[dest].transWaitingToBeSent.pop_front();
        }
    }
}
