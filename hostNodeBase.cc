#include "hostNodeBase.h"
#include <queue>

#define MSGSIZE 100
#define MAX_SENDER_PER_DEST_QUEUE 15000

//global parameters
unordered_map<int, priority_queue<TransUnit, vector<TransUnit>, LaterTransUnit>> _transUnitList;
unordered_map<double, int> _transactionCompletionBySize;
unordered_map<double, int> _transactionArrivalBySize;
unordered_map<double, double> _txnAvgCompTimeBySize;
unordered_map<double, vector<double>> _txnTailCompTimesBySize;
ofstream _succRetriesFile, _failRetriesFile;
ofstream _tailCompBySizeFile;
unordered_map<int, set<int>> _destList;
unordered_map<int, unordered_map<double, SplitState>> _numSplits;
unordered_map<int, unordered_map<int, vector<vector<int>>>> _pathsMap;
int _numNodes;
int _numRouterNodes;
int _numHostNodes;
double _maxTravelTime;
double _maxOneHopDelay;
double _statRate;
double _clearRate;
int _kValue;
double _simulationLength;
int _serviceArrivalWindow; 

double _transStatStart;
double  _transStatEnd;
double _waterfillingStartTime;
double _landmarkRoutingStartTime;
double _shortestPathStartTime;
double _shortestPathEndTime;
double _splitSize;
double _bank;
double _percentile;

 //adjacency list format of graph edges of network
unordered_map<int, vector<pair<int,int>>> _channels;

//unordered_map of balances for each edge; key = <int,int> is <source, destination>
unordered_map<tuple<int,int>,double, hashId> _balances;

unordered_map<tuple<int,int>,double, hashId> _capacities;

// controls algorithm and what is outputted
bool _waterfillingEnabled;
bool _dctcpEnabled;
bool _timeoutEnabled;
bool _loggingEnabled;
bool _signalsEnabled;
bool _priceSchemeEnabled;
bool _landmarkRoutingEnabled;
bool _windowEnabled;
bool _lndBaselineEnabled;
bool _splittingEnabled;
bool _celerEnabled;


bool _widestPathsEnabled;
bool _heuristicPathsEnabled;
bool _obliviousRoutingEnabled;
bool _kspYenEnabled;

// scheduling algorithms
bool _LIFOEnabled;
bool _FIFOEnabled;
bool _RREnabled;
bool _SPFEnabled;
bool _EDFEnabled;
bool (*_schedulingAlgorithm) (const tuple<int,double, routerMsg*, Id, simtime_t> &a,
      const tuple<int,double, routerMsg*, Id, simtime_t> &b);

// for all precision errors
double _epsilon; 


//global parameters for fixed size queues
bool _hasQueueCapacity;
int _queueCapacity;

// rebalancing related
bool _rebalancingEnabled;
double _rebalancingUpFactor;
double _queueDelayThreshold;
double _gamma;
double _maxGammaImbalanceQueueSize;
double _delayForAddingFunds;
double _rebalanceRate;
double _computeBalanceRate;

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

/* get total amount on queue to node x */
double hostNodeBase::getTotalAmount(int x) {
    if (_hasQueueCapacity && _queueCapacity == 0)
        return 0;
    return nodeToPaymentChannel[x].totalAmtInQueue;
} 

/* get total amount inflight incoming node x */
double hostNodeBase::getTotalAmountIncomingInflight(int x) {
    return nodeToPaymentChannel[x].totalAmtIncomingInflight;
} 

/* get total amount inflight outgoing node x */
double hostNodeBase::getTotalAmountOutgoingInflight(int x) {
    return nodeToPaymentChannel[x].totalAmtOutgoingInflight;
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

/* helper function to push a transaction into the appropriate sender queue 
 * in order 
 */
void hostNodeBase::pushIntoSenderQueue(DestInfo* destInfo, routerMsg* ttmsg) {
    multiset<routerMsg*, transCompare> *senderQ = &(destInfo->transWaitingToBeSent);
    senderQ->insert(ttmsg);
    auto position = senderQ->end();
    if (senderQ->size() > MAX_SENDER_PER_DEST_QUEUE) {
        position--;
        routerMsg* lastMsg = *position;
        senderQ->erase(position);
        deleteTransaction(lastMsg);
    }
}

/* helper function to delete router message and encapsulated transaction message
 */
void hostNodeBase::deleteTransaction(routerMsg* ttmsg) {
    transactionMsg *transMsg = check_and_cast<transactionMsg *>(ttmsg->getEncapsulatedPacket());
    int destination = transMsg->getReceiver();
    statNumTimedOut[destination] += 1;
    ttmsg->decapsulate();
    delete transMsg;
    delete ttmsg;
}

/* generate next Transaction to be processed at this node 
 * this is an optimization to prevent all txns from being loaded initially
 */
void hostNodeBase::generateNextTransaction() {
      if (_transUnitList[myIndex()].empty())
          return;
      TransUnit j = _transUnitList[myIndex()].top();
      double timeSent = j.timeSent;
      
      routerMsg *msg = generateTransactionMessage(j);

      // override default shortest path 
      if (_waterfillingEnabled || _priceSchemeEnabled || _landmarkRoutingEnabled || _lndBaselineEnabled 
              || _dctcpEnabled || _celerEnabled){
         vector<int> blankPath = {};
         msg->setRoute(blankPath);
      }

      scheduleAt(timeSent, msg);

      if (_timeoutEnabled && j.hasTimeOut){
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



/* register a signal per destination for this path of the particular type passed in
 * and return the signal created
 */
simsignal_t hostNodeBase::registerSignalPerChannelPerDest(string signalStart, int chnlEndNode, int destNode) {
    string signalPrefix = signalStart + "PerChannelPerDest";
    char signalName[64];
    string templateString = signalPrefix + "Template";

    if (chnlEndNode < _numHostNodes){
        sprintf(signalName, "%s_%d(host %d)", signalPrefix.c_str(), destNode, chnlEndNode);
    } else{
        sprintf(signalName, "%s_%d(router %d [%d] )", signalPrefix.c_str(),
             destNode, chnlEndNode - _numHostNodes, chnlEndNode);
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

/* helper function to record the tail completion times for every algorithm
 * called by each algorithm wheever it computes its completion time
 */
void hostNodeBase::recordTailCompletionTime(simtime_t timeSent, double amount, double completionTime){
    if (timeSent >= _transStatStart && timeSent <= _transStatEnd) {
        _txnTailCompTimesBySize[amount].push_back(completionTime);
        if (_txnTailCompTimesBySize[amount].size() == 1000) {
            _tailCompBySizeFile << amount << ": ";
            for (auto const& time : _txnTailCompTimesBySize[amount]) 
                _tailCompBySizeFile << time << " ";
            _tailCompBySizeFile << endl;
            _tailCompBySizeFile.flush();
            _txnTailCompTimesBySize[amount].clear();
        }
    }
}

/****** MESSAGE GENERATORS **********/

/* responsible for generating one HTLC for a particular path 
 * for any algorithm  after the path has been decided by 
 * some function that does splitTransaction
 */
routerMsg* hostNodeBase::generateTransactionMessageForPath(double amt, 
        vector<int> path, int pathIndex, transactionMsg* transMsg) {
    char msgname[MSGSIZE];
    sprintf(msgname, "tic-%d-to-%d split-transMsg", myIndex(), transMsg->getReceiver());
    
    transactionMsg *msg = new transactionMsg(msgname);
    msg->setAmount(amt);
    msg->setTimeSent(transMsg->getTimeSent());
    msg->setSender(transMsg->getSender());
    msg->setReceiver(transMsg->getReceiver());
    msg->setPriorityClass(transMsg->getPriorityClass());
    msg->setHasTimeOut(transMsg->getHasTimeOut());
    msg->setPathIndex(pathIndex);
    msg->setTimeOut(transMsg->getTimeOut());
    msg->setTransactionId(transMsg->getTransactionId());
    msg->setLargerTxnId(transMsg->getLargerTxnId());
    msg->setIsMarked(false);

    // find htlc for txn
    int transactionId = transMsg->getTransactionId();    
    int htlcIndex = 0;
    if (transactionIdToNumHtlc.count(transactionId) == 0) {
        transactionIdToNumHtlc[transactionId] = 1;
    }
    else {
        htlcIndex =  transactionIdToNumHtlc[transactionId];
        transactionIdToNumHtlc[transactionId] = transactionIdToNumHtlc[transactionId] + 1;
    }
    msg->setHtlcIndex(htlcIndex);

    // routerMsg on the outside
    sprintf(msgname, "tic-%d-to-%d split-routerTransMsg", myIndex(), transMsg->getReceiver());
    routerMsg *rMsg = new routerMsg(msgname);
    rMsg->setRoute(path);
    rMsg->setHopCount(0);
    rMsg->setMessageType(TRANSACTION_MSG);
    rMsg->encapsulate(msg);
    return rMsg;

} 

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
    msg->setLargerTxnId(unit.largerTxnId);
    msg->setIsMarked(false);
    
    sprintf(msgname, "tic-%d-to-%d router-transaction-Msg %f", unit.sender, unit.receiver, unit.timeSent);
    
    routerMsg *rMsg = new routerMsg(msgname);
    // compute route only once
    if (destNodeToPath.count(unit.receiver) == 0){
        vector<int> route;
        if (_celerEnabled)
            route = {};
        else
            route = getRoute(unit.sender,unit.receiver);
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

/* Generates a duplicate transaction message using an ack for the same transaction message
 * useful if you have to duplicate a transaction from the ackMessage 
*/
routerMsg *hostNodeBase::generateDuplicateTransactionMessage(ackMsg* aMsg) {
   char msgname[MSGSIZE];
   int sender = myIndex();
   int receiver = aMsg->getReceiver();
   sprintf(msgname, "tic-%d-to-%d transactionMsg", sender, receiver);
   
   transactionMsg *msg = new transactionMsg(msgname);
   msg->setAmount(aMsg->getAmount());
   msg->setTimeSent(aMsg->getTimeSent());
   msg->setSender(sender);
   msg->setReceiver(receiver);
   msg->setPriorityClass(aMsg->getPriorityClass());
   msg->setTransactionId(aMsg->getTransactionId());
   msg->setHtlcIndex(aMsg->getHtlcIndex());
   msg->setHasTimeOut(aMsg->getHasTimeOut());
   msg->setTimeOut(aMsg->getTimeOut());
   msg->setLargerTxnId(aMsg->getLargerTxnId());
   
   sprintf(msgname, "tic-%d-to-%d router-transaction-Msg %f", sender, receiver, aMsg->getTimeSent());
   
   routerMsg *rMsg = new routerMsg(msgname);
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
    aMsg->setLargerTxnId(transMsg->getLargerTxnId());
    aMsg->setPriorityClass(transMsg->getPriorityClass());
    aMsg->setTimeOut(transMsg->getTimeOut());
    aMsg->setTimeAttempted(transMsg->getTimeAttempted());
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
routerMsg *hostNodeBase::generateTriggerRebalancingMessage(){
    char msgname[MSGSIZE];
    sprintf(msgname, "node-%d rebalancingMsg", myIndex());
    routerMsg *rMsg = new routerMsg(msgname);
    rMsg->setMessageType(TRIGGER_REBALANCING_MSG);
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

/* generate message trigger t generate balances for all the payment channels
 */
routerMsg *hostNodeBase::generateComputeMinBalanceMessage(){
    char msgname[MSGSIZE];
    sprintf(msgname, "node-%d computeMinBalanceMsg", myIndex());
    routerMsg *rMsg = new routerMsg(msgname);
    rMsg->setMessageType(COMPUTE_BALANCE_MSG);
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

/* special type of time out message for waterfilling, etd. designed for a specific path so that
 * such messages will be sent on all paths considered for waterfilling
 */
routerMsg* hostNodeBase::generateTimeOutMessageForPath(vector<int> path, 
        int transactionId, int receiver){
    char msgname[MSGSIZE];
    sprintf(msgname, "tic-%d-to-%d path-timeOutMsg", myIndex(), receiver);
    timeOutMsg *msg = new timeOutMsg(msgname);

    msg->setReceiver(receiver);
    msg->setTransactionId(transactionId);

    sprintf(msgname, "tic-%d-to-%d path-router-timeOutMsg", myIndex(), receiver);
    routerMsg *rMsg = new routerMsg(msgname);
    rMsg->setRoute(path);

    rMsg->setHopCount(0);
    rMsg->setMessageType(TIME_OUT_MSG);
    rMsg->encapsulate(msg);
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

/* generate a message that designates which payment channels at this router need funds
 * and how much funds they need, will be processed a few seconds/minutes later to 
 * actually add the funds to those payment channels */
routerMsg *hostNodeBase::generateAddFundsMessage(map<int, double> fundsToBeAdded) {
    map<int,double> pcsNeedingFunds = fundsToBeAdded;
    
    char msgname[MSGSIZE];
    sprintf(msgname, "addfundmessage-at-%d", myIndex());
    routerMsg *msg = new routerMsg(msgname);
    addFundsMsg *afMsg = new addFundsMsg(msgname);
    afMsg->setPcsNeedingFunds(pcsNeedingFunds);

    msg->setMessageType(ADD_FUNDS_MSG); 
    msg->encapsulate(afMsg);
    return msg;
}

/***** MESSAGE HANDLERS *****/
/* overall controller for handling messages that dispatches the right function
 * based on message type
 */
void hostNodeBase::handleMessage(cMessage *msg){
    routerMsg *ttmsg = check_and_cast<routerMsg *>(msg);
 
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
                break;
            }

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
        
        case TRIGGER_REBALANCING_MSG:
            if (_loggingEnabled) cout<< "[HOST "<< myIndex() 
                <<": RECEIVED TRIGGER REBALANCE MSG] "<< msg->getName() << endl;
                handleTriggerRebalancingMessage(ttmsg);
            if (_loggingEnabled) cout<< "[AFTER HANDLING:]  "<< endl;
            break;

        case COMPUTE_BALANCE_MSG:
            if (_loggingEnabled) cout<< "[HOST "<< myIndex() 
                <<": RECEIVED COMPUTE BALANCE MSG] "<< msg->getName() << endl;
                handleComputeMinAvailableBalanceMessage(ttmsg);
            if (_loggingEnabled) cout<< "[AFTER HANDLING:]  "<< endl;
            break;

        case ADD_FUNDS_MSG:
            if (_loggingEnabled) cout<< "[HOST "<< myIndex() 
                <<": RECEIVED ADD FUNDS MSG] "<< msg->getName() << endl;
                handleAddFundsMessage(ttmsg);
            if (_loggingEnabled) cout<< "[AFTER HANDLING:]  "<< endl;
            break;
        
        default:
            handleMessage(ttmsg);

    }

}

/* Specialized function to handle transactions as per the algorithm
 * Since this is shortest path, no other fancy handler required 
 */
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
    vector<tuple<int, double , routerMsg *, Id, simtime_t>> *q;
    int destination = transMsg->getReceiver();
    int sender = transMsg->getSender();
    int transactionId = transMsg->getTransactionId();
    
    if (!revisit && transMsg->getTimeSent() >= _transStatStart && 
            transMsg->getTimeSent() <= _transStatEnd && transMsg->isSelfMessage()) {
        statRateArrived[destination] += 1;
        statAmtArrived[destination] += transMsg->getAmount();
        statRateAttempted[destination] += 1;
        statAmtAttempted[destination] += transMsg->getAmount();
    }
    else if (!revisit && transMsg->isSelfMessage()) 
        statNumArrived[destination] += 1;


    
    // if it is at the destination
    if (ttmsg->getHopCount() ==  ttmsg->getRoute().size() - 1) {
        // add to incoming trans units 
        int prevNode = ttmsg->getRoute()[ttmsg->getHopCount() - 1];
        unordered_map<Id, double, hashId> *incomingTransUnits = 
            &(nodeToPaymentChannel[prevNode].incomingTransUnits);
        (*incomingTransUnits)[make_tuple(transMsg->getTransactionId(), transMsg->getHtlcIndex())] = 
            transMsg->getAmount();
        nodeToPaymentChannel[prevNode].totalAmtIncomingInflight += transMsg->getAmount();
        
        if (_timeoutEnabled){
            auto iter = canceledTransactions.find(make_tuple(transactionId, 0, 0, 0, 0));
            if (iter != canceledTransactions.end()) {
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
        PaymentChannel *neighbor = &(nodeToPaymentChannel[nextNode]);
        q = &(nodeToPaymentChannel[nextNode].queuedTransUnits);
        tuple<int,int > key = make_tuple(transMsg->getTransactionId(), transMsg->getHtlcIndex());
        
        if (!revisit) 
            transMsg->setTimeAttempted(simTime().dbl());

        // mark the arrival
        neighbor->arrivalTimeStamps.push_back(make_tuple(transMsg->getAmount(), simTime()));
        neighbor->sumArrivalWindowTxns += transMsg->getAmount();
        if (neighbor->arrivalTimeStamps.size() > _serviceArrivalWindow) {
            double frontAmt = get<0>(neighbor->serviceArrivalTimeStamps.front());
            neighbor->arrivalTimeStamps.pop_front(); 
            neighbor->sumArrivalWindowTxns -= frontAmt;
        }

        // if there is insufficient balance at the first node, return failure
        if (_hasQueueCapacity && _queueCapacity == 0) {
            if (forwardTransactionMessage(ttmsg, destNode, simTime()) == 0) {
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
            neighbor->totalAmtInQueue += transMsg->getAmount();
            push_heap((*q).begin(), (*q).end(), _schedulingAlgorithm);
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

/* handler that periodically computes the minimum balance on a payment channel 
 * this is then used accordingly to trigger rebalancing events */
void hostNodeBase::handleComputeMinAvailableBalanceMessage(routerMsg* ttmsg) {
    // reschedule this message to be sent again
    if (simTime() > _simulationLength || !_rebalancingEnabled) {
        delete ttmsg;
    }
    else {
        scheduleAt(simTime()+_computeBalanceRate, ttmsg);
    }
    
    for (auto it = nodeToPaymentChannel.begin(); it!= nodeToPaymentChannel.end(); it++){
        PaymentChannel *p = &(it->second);
        if (p->balance < p->minAvailBalance)
            p->minAvailBalance = p->balance;
    }
}


/* handler for the periodic rebalancing message that gets triggered 
 * that is responsible for implicit rebalancing at the end-hosts
 * basically refunds from what has been sent out and remove what's been
 * received, all to be adjusted on the single payment channel
 * connected to it
 */ 
void hostNodeBase::handleTriggerRebalancingMessage(routerMsg* ttmsg) {
    delete ttmsg;
    return;
    // reschedule the message again to be periodic
    if (simTime() > _simulationLength || !_rebalancingEnabled){
        delete ttmsg;
    }
    else {
        scheduleAt(simTime()+_rebalanceRate, ttmsg);
    }

    map<int, double> pcsNeedingFunds;
    for (auto it = nodeToPaymentChannel.begin(); it!= nodeToPaymentChannel.end(); it++){
        // technically there's only payment channel here
        int id = it->first;
        PaymentChannel *p = &(it->second);
        double totalAmtReceived = 0;
        double totalAmtSent = 0;
        double currentlyInflight = getTotalAmountOutgoingInflight(it->first);

        // remove funds for everything that has been received and needs to be refunded
        for (auto nodeIt = senderToAmtRefundable.begin(); nodeIt != senderToAmtRefundable.end(); nodeIt++) {
            totalAmtReceived += nodeIt->second;
            nodeIt->second = 0;
        }
        // schedule message to add funds for everything that has been sent and therefore is getting refunded 
        for (auto receiverIt = receiverToAmtRefunded.begin(); receiverIt != receiverToAmtRefunded.end(); 
                receiverIt++) {
            totalAmtSent += receiverIt->second;
            receiverIt->second = 0;
        }

        p->owedFunds += max(totalAmtReceived - totalAmtSent, 0.0);
        double removableFunds = min(p->owedFunds, p->balance);
        
        if (removableFunds > 0) {
            _bank += removableFunds;
            setPaymentChannelBalanceByNode(it->first, p->balance - removableFunds);
            p->owedFunds -= removableFunds;
            if (p->balance < 0)
                cout << "abhishtu at " << myIndex() << " removable  " 
                    << removableFunds << " balance " << p->balance << "min available balance "
                    << p->minAvailBalance << " bank balance " << _bank << endl;
            
            p->amtImplicitlyRebalanced -= removableFunds;
            p->numRebalanceEvents += 1;
            
            tuple<int, int> senderReceiverTuple = (id < myIndex()) ? make_tuple(id, myIndex()) :
                make_tuple(myIndex(), id);
            _capacities[senderReceiverTuple] -= removableFunds; 
        } 
        
        p->entitledFunds += max(totalAmtSent - totalAmtReceived, 0.0);
        double addableFunds = min(p->entitledFunds, _bank); 
        if (addableFunds > 0) {
            // add this to the list of payment channels to be addressed 
            // along with a particular addFundsEvent
            pcsNeedingFunds[id] = addableFunds;
            _bank -= addableFunds;
            p->entitledFunds -= addableFunds;
        } 
    }

    // generate and schedule add funds message to add these funds after some fixed time period
    if (pcsNeedingFunds.size() > 0) {
        routerMsg* addFundsMsg = generateAddFundsMessage(pcsNeedingFunds);
        scheduleAt(simTime() + _delayForAddingFunds, addFundsMsg);
    }
}

/* handler to add the desired amount of funds to the given payment channels when an addFundsMessage
 * is received 
 */
void hostNodeBase::handleAddFundsMessage(routerMsg* ttmsg) {
    addFundsMsg *afMsg = check_and_cast<addFundsMsg *>(ttmsg->getEncapsulatedPacket());
    map<int, double> pcsNeedingFunds = afMsg->getPcsNeedingFunds();
    for (auto it = pcsNeedingFunds.begin(); it!= pcsNeedingFunds.end(); it++) {
        int pcIdentifier = it->first;
        double fundsToAdd = it->second;
        PaymentChannel *p = &(nodeToPaymentChannel[pcIdentifier]);

        // add funds at this end
        setPaymentChannelBalanceByNode(pcIdentifier, p->balance + fundsToAdd);
        tuple<int, int> senderReceiverTuple = (pcIdentifier < myIndex()) ? 
            make_tuple(pcIdentifier, myIndex()) :
            make_tuple(myIndex(), pcIdentifier);
        _capacities[senderReceiverTuple] +=  fundsToAdd;
        
        p->numRebalanceEvents += 1;
        p->amtAdded += fundsToAdd;
        p->amtImplicitlyRebalanced += fundsToAdd;

        // process as many new transUnits as you can for this payment channel
        processTransUnits(pcIdentifier, p->queuedTransUnits);
    }
    
    ttmsg->decapsulate();
    delete afMsg;
    delete ttmsg;
}

/* specialized ack handler that does the routine if this is a shortest paths 
 * algorithm. In particular, collects stats assuming that this is the only
 * one path on which a txn might complete
 * NOTE: acks are on the reverse path relative to the original sender
 */
void hostNodeBase::handleAckMessageSpecialized(routerMsg* ttmsg) { 

    int destNode = ttmsg->getRoute()[0];
    ackMsg *aMsg = check_and_cast<ackMsg *>(ttmsg->getEncapsulatedPacket());

    if (aMsg->getIsSuccess() == false && aMsg->getTimeSent() >= _transStatStart && 
            aMsg->getTimeSent() <= _transStatEnd) {
        statRateFailed[destNode] = statRateFailed[destNode] + 1;
        statAmtFailed[destNode] += aMsg->getAmount();
    }
    else if (aMsg->getTimeSent() >= _transStatStart && 
            aMsg->getTimeSent() <= _transStatEnd) {
        statRateCompleted[destNode] = statRateCompleted[destNode] + 1;
        _transactionCompletionBySize[aMsg->getAmount()] += 1;
        statAmtCompleted[destNode] += aMsg->getAmount();

        // stats
        double timeTaken = simTime().dbl() - aMsg->getTimeSent();
        statCompletionTimes[destNode] += timeTaken * 1000;
        _txnAvgCompTimeBySize[aMsg->getAmount()] += timeTaken * 1000;
        recordTailCompletionTime(aMsg->getTimeSent(), aMsg->getAmount(), timeTaken * 1000);
    }
    else 
        statNumCompleted[destNode] += 1;

    hostNodeBase::handleAckMessage(ttmsg);
}


/* default routine for handling an ack that is responsible for 
 * updating outgoing transunits and incoming trans units 
 * and triggering an update message to the next node on the path
 * before forwarding the ack back to the previous node
 */
void hostNodeBase::handleAckMessage(routerMsg* ttmsg){
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
   
    if (aMsg->getIsSuccess() == false) {
        // increment funds on this channel unless this is the node that caused the fauilure
        // in which case funds were never decremented in the first place
        if (aMsg->getFailedHopNum() < ttmsg->getHopCount()) {
            double updatedBalance = prevChannel->balance + aMsg->getAmount();
            prevChannel->balanceEWMA = 
                (1 -_ewmaFactor) * prevChannel->balanceEWMA + (_ewmaFactor) * updatedBalance; 
            setPaymentChannelBalanceByNode(prevNode, updatedBalance);
            prevChannel->totalAmtOutgoingInflight -= aMsg->getAmount();
        }
        
        // no relevant incoming_trans_units because no node on fwd path before this
        if (ttmsg->getHopCount() < ttmsg->getRoute().size() - 1) {
            int nextNode = ttmsg->getRoute()[ttmsg->getHopCount()+1];
            unordered_map<Id, double, hashId> *incomingTransUnits = 
                &(nodeToPaymentChannel[nextNode].incomingTransUnits);
            (*incomingTransUnits).erase(make_tuple(aMsg->getTransactionId(), 
                        aMsg->getHtlcIndex()));
            nodeToPaymentChannel[nextNode].totalAmtIncomingInflight -= aMsg->getAmount();
        }
    }
    else { 
        // mark the time it spent inflight
        prevChannel->sumTimeInFlight += timeInflight;
        prevChannel->timeInFlightSamples += 1;
        prevChannel->totalAmtOutgoingInflight -= aMsg->getAmount();

        routerMsg* uMsg =  generateUpdateMessage(aMsg->getTransactionId(), 
                prevNode, aMsg->getAmount(), aMsg->getHtlcIndex() );
        prevChannel->numUpdateMessages += 1;
        forwardMessage(uMsg);

        // keep track of how much you have sent to others if rebalancing is enabled
        // and how much of that needs to be replenished and
        // replenish my end host - router link immediately to make up (simulates receiving money back)
        int dest = aMsg->getReceiver();
        if (_rebalancingEnabled) {
            tuple<int, int> senderReceiverTuple = make_tuple(myIndex(), prevNode);
            _capacities[senderReceiverTuple] += aMsg->getAmount();
            double newBalance = prevChannel->balance + aMsg->getAmount();
            prevChannel->balanceEWMA = (1 -_ewmaFactor) * prevChannel->balanceEWMA + 
            (_ewmaFactor) * newBalance;
            setPaymentChannelBalanceByNode(prevNode, newBalance);
        }
    }
    
    //delete ack message
    ttmsg->decapsulate();
    delete aMsg;
    delete ttmsg;
}



/* handles the logic for ack messages in the presence of timeouts
 * in particular, removes the transaction from the cancelled txns
 * to mark that it has been received 
 * it uses the successfulDoNotSendTimeout to detect if txns have
 * been completed when handling the timeout - so insert into it here
 */
void hostNodeBase::handleAckMessageTimeOut(routerMsg* ttmsg){
    ackMsg *aMsg = check_and_cast<ackMsg *>(ttmsg->getEncapsulatedPacket());
    int transactionId = aMsg->getTransactionId();
    
    // only if it isn't waterfilling
    if (aMsg->getIsSuccess()) {
        auto iter = canceledTransactions.find(make_tuple(transactionId, 0, 0, 0, 0));
        if (iter != canceledTransactions.end()) {
            canceledTransactions.erase(iter);
        }
        successfulDoNotSendTimeOut.insert(aMsg->getTransactionId());
    }
}



/*
 * handleUpdateMessage - called when receive update message, increment back funds, see if we can
 *      process more jobs with new funds, delete update message
 */
void hostNodeBase::handleUpdateMessage(routerMsg* msg) {
    vector<tuple<int, double , routerMsg *, Id, simtime_t>> *q;
    int prevNode = msg->getRoute()[msg->getHopCount()-1];
    updateMsg *uMsg = check_and_cast<updateMsg *>(msg->getEncapsulatedPacket());
    PaymentChannel *prevChannel = &(nodeToPaymentChannel[prevNode]);
   
    //increment the in flight funds back
    double newBalance = prevChannel->balance + uMsg->getAmount();
    prevChannel->balanceEWMA = (1 -_ewmaFactor) * prevChannel->balanceEWMA 
        + (_ewmaFactor) * newBalance;
    setPaymentChannelBalanceByNode(prevNode, newBalance);

    // immediately remove these funds - simulates giving end host back these funds
    if (_rebalancingEnabled) {
        tuple<int, int> senderReceiverTuple = make_tuple(myIndex(), prevNode);
        _capacities[senderReceiverTuple] -= uMsg->getAmount();
        double newBalance = prevChannel->balance - uMsg->getAmount();
        prevChannel->balanceEWMA = (1 -_ewmaFactor) * prevChannel->balanceEWMA + 
        (_ewmaFactor) * newBalance;
        setPaymentChannelBalanceByNode(prevNode, newBalance);
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
void hostNodeBase::handleStatMessage(routerMsg* ttmsg){
    // reschedule this message to be sent again
    if (simTime() > _simulationLength){
        delete ttmsg;
    }
    else {
        scheduleAt(simTime() + _statRate, ttmsg);
    }
    
    if (_signalsEnabled) {
        // per channel Stats
        for ( auto it = nodeToPaymentChannel.begin(); it!= nodeToPaymentChannel.end(); it++){
            PaymentChannel *p = &(it->second);
            int id = it->first;
            if (myIndex() == 0) {
                emit(p->bankSignal, _bank);
                double sumCapacities = accumulate(begin(_capacities), end(_capacities), 0,
                    [] (double value, const std::map<tuple<int,int>, double>::value_type& p)
                   { return value + p.second; }
                    ); 
            }
            
            emit(p->amtInQueuePerChannelSignal, getTotalAmount(it->first));
            emit(p->balancePerChannelSignal, p->balance);
            emit(p->explicitRebalancingAmtPerChannelSignal, p->amtExplicitlyRebalanced/_statRate);
            emit(p->implicitRebalancingAmtPerChannelSignal, p->amtImplicitlyRebalanced/_statRate);
            emit(p->timeInFlightPerChannelSignal, p->sumTimeInFlight/p->timeInFlightSamples);
            emit(p->numInflightPerChannelSignal, getTotalAmountIncomingInflight(it->first) +
                    getTotalAmountOutgoingInflight(it->first));
            p->sumTimeInFlight = 0;
            p->timeInFlightSamples = 0;
            p->amtExplicitlyRebalanced = 0;
            p->amtImplicitlyRebalanced = 0;
            
            tuple<int, int> senderReceiverTuple = (id < myIndex()) ? make_tuple(id, myIndex()) :
                make_tuple(myIndex(), id);
            emit(p->capacityPerChannelSignal, _capacities[senderReceiverTuple]);
        }
    }

    recordScalar("time", simTime());
    for (auto it = 0; it < _numHostNodes; it++){
        if (_destList[myIndex()].count(it) > 0) {
            char buffer[30];
            sprintf(buffer, "rateCompleted %d -> %d", myIndex(), it);
            recordScalar(buffer, statRateCompleted[it]);
            sprintf(buffer, "amtCompleted %d -> %d", myIndex(), it);
            recordScalar(buffer, statAmtCompleted[it]);

            sprintf(buffer, "rateAttempted %d -> %d", myIndex(), it);
            recordScalar(buffer, statRateAttempted[it]);
            sprintf(buffer, "amtAttempted  %d -> %d", myIndex(), it);
            recordScalar(buffer, statAmtAttempted[it]);

            sprintf(buffer, "rateArrived %d -> %d", myIndex(), it);
            recordScalar(buffer, statRateArrived[it]);
            sprintf(buffer, "amtArrived  %d -> %d", myIndex(), it);
            recordScalar(buffer, statAmtArrived[it]);

            sprintf(buffer, "completionTime %d -> %d ", myIndex(), it);
            recordScalar(buffer, statCompletionTimes[it]);

            statRateAttempted[it] = 0;
            statAmtAttempted[it] = 0;
            statRateArrived[it] = 0;
            statAmtArrived[it] = 0;
            statAmtCompleted[it] = 0;
            statRateCompleted[it] = 0;
        }

        // per destination stats
        if (it != getIndex() && _destList[myIndex()].count(it) > 0) {
            if (nodeToShortestPathsMap.count(it) > 0) {
                for (auto p: nodeToShortestPathsMap[it]) {
                    PathInfo *pathInfo = &(nodeToShortestPathsMap[it][p.first]);
                    
                    //emit rateCompleted per path
                    pathInfo->statRateAttempted = 0;
                    pathInfo->statRateCompleted = 0;
                    
                    //emit rateAttempted per path
                    if (_signalsEnabled) {
                        emit(pathInfo->rateAttemptedPerDestPerPathSignal, 
                            pathInfo->statRateAttempted);
                        emit(pathInfo->rateCompletedPerDestPerPathSignal, 
                            pathInfo->statRateCompleted);
                    }
                }
            }

            if (_signalsEnabled) {
                if (_hasQueueCapacity){
                    emit(rateFailedPerDestSignals[it], statRateFailed[it]);
                }
                emit(rateCompletedPerDestSignals[it], statRateCompleted[it]);
                emit(rateAttemptedPerDestSignals[it], statRateAttempted[it]);
                emit(rateArrivedPerDestSignals[it], statRateArrived[it]);
                
                emit(numTimedOutPerDestSignals[it], statNumTimedOut[it]);
                emit(numPendingPerDestSignals[it], destNodeToNumTransPending[it]);
                emit(numCompletedPerDestSignals[it], statNumCompleted[it]);
                emit(numArrivedPerDestSignals[it], statNumArrived[it]);
                double frac = ((float(statNumCompleted[it]))/(max(statNumArrived[it],1)));
                statNumCompleted[it] = 0;
                statNumArrived[it] = 0;
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
                vector<tuple<int, double, routerMsg*, Id, simtime_t>>* queuedTransUnits = 
                    &(nodeToPaymentChannel[nextNode].queuedTransUnits);

                auto iterQueue = find_if((*queuedTransUnits).begin(),
                  (*queuedTransUnits).end(),
                  [&transactionId](const tuple<int, double, routerMsg*, Id, simtime_t>& p)
                  { return (get<0>(get<3>(p)) == transactionId); });
                
                // delete first occurences of this transaction in the queue
                // tids are unique, so this can't be a problem (we do splitting before hand)
                // especially if there are splits
                if (iterQueue != (*queuedTransUnits).end()){
                    routerMsg * rMsg = get<2>(*iterQueue);
                    auto tMsg = rMsg->getEncapsulatedPacket();
                    rMsg->decapsulate();
                    nodeToPaymentChannel[nextNode].totalAmtInQueue -= get<1>(*iterQueue);
                    iterQueue = (*queuedTransUnits).erase(iterQueue);
                    delete tMsg;
                    delete rMsg;
                }
                
                // resort the queue based on priority
                make_heap((*queuedTransUnits).begin(), (*queuedTransUnits).end(), 
                        _schedulingAlgorithm);
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
        }

        // remove from outgoing transUnits to nextNode and restore balance on own end
        if (simTime() > (msgArrivalTime + _maxTravelTime + _maxOneHopDelay)){
            if (nextNode != -1){
                unordered_map<tuple<int,int>, double, hashId> *outgoingTransUnits = 
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
                    setPaymentChannelBalanceByNode(nextNode, updatedBalance);
                    nextChannel->balanceEWMA = (1 -_ewmaFactor) * nextChannel->balanceEWMA + 
                        (_ewmaFactor) * updatedBalance;
                    nextChannel->totalAmtOutgoingInflight -= amount;
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
int hostNodeBase::forwardTransactionMessage(routerMsg *msg, int dest, simtime_t arrivalTime) {
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
        neighbor->serviceArrivalTimeStamps.push_back(
                make_tuple(transMsg->getAmount(), simTime(), arrivalTime));
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
        int amt = transMsg->getAmount();
        double newBalance = neighbor->balance - amt;
        setPaymentChannelBalanceByNode(nextDest, newBalance);
        neighbor-> balanceEWMA = (1 -_ewmaFactor) * neighbor->balanceEWMA + 
            (_ewmaFactor) * newBalance;
        neighbor->totalAmtInQueue -= amt;

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
    successfulDoNotSendTimeOut = {};
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
        _dctcpEnabled = par("dctcpEnabled");
        _splittingEnabled = par("splittingEnabled");
        cout << "splitting" << _splittingEnabled << endl;
        _serviceArrivalWindow = par("serviceArrivalWindow");
        string resultPrefix = par("resultPrefix");

        _hasQueueCapacity = true;
        _queueCapacity = 12000;

        _transStatStart = par("transStatStart");
        _transStatEnd = par("transStatEnd");
        _waterfillingStartTime = 0;
        _landmarkRoutingStartTime = 0;
        _shortestPathStartTime = 0;
        _shortestPathEndTime = 5000;

        _widestPathsEnabled = par("widestPathsEnabled");
        _heuristicPathsEnabled = par("heuristicPathsEnabled");
        _kspYenEnabled = par("kspYenEnabled");
        _obliviousRoutingEnabled = par("obliviousRoutingEnabled");

        _splitSize = par("splitSize");
        _percentile = 0.01;

        _celerEnabled = par("celerEnabled");
        _lndBaselineEnabled = par("lndBaselineEnabled");
        _landmarkRoutingEnabled = par("landmarkRoutingEnabled");
                                  
        if (_landmarkRoutingEnabled || _lndBaselineEnabled || _celerEnabled){
            _hasQueueCapacity = true;
            _queueCapacity = 0;
            if (!_celerEnabled)
                _timeoutEnabled = false;
        }

        // rebalancing related flags/parameters
        _rebalancingEnabled = par("rebalancingEnabled");
        _rebalancingUpFactor = 3.0;
        _queueDelayThreshold = par("queueDelayThreshold");
        _gamma = par("gamma");
        _maxGammaImbalanceQueueSize = par("gammaImbalanceQueueSize");
        _delayForAddingFunds = par("rebalancingDelayForAddingFunds");
        _rebalanceRate = par("rebalancingRate");
        _computeBalanceRate = par("minBalanceComputeRate");
        _bank = 0;

        // files for recording tail completion and retries
        _tailCompBySizeFile.open(resultPrefix + "_tailCompBySize.txt");
        if (_lndBaselineEnabled) {
            _succRetriesFile.open(resultPrefix + "_succRetries.txt");
            _failRetriesFile.open(resultPrefix + "_failRetries.txt");
        }

        // path choices
        string pathFileName;
        if (_widestPathsEnabled)
            pathFileName = topologyFile_ + "_widestPaths";
        else if (_obliviousRoutingEnabled)
            pathFileName = topologyFile_ + "_obliviousPaths";
        else if (_heuristicPathsEnabled)
            pathFileName = topologyFile_ + "_heuristicPaths";
        else if (_kspYenEnabled)
            pathFileName = topologyFile_ + "_kspYenPaths";

        // scheduling algorithms
        _LIFOEnabled = par("LIFOEnabled");
        _FIFOEnabled = par("FIFOEnabled");
        _SPFEnabled = par("SPFEnabled");
        _RREnabled = par("RREnabled");
        _EDFEnabled = par("EDFEnabled");

        if (_LIFOEnabled) 
            _schedulingAlgorithm = &sortLIFO;
        else if (_FIFOEnabled)
            _schedulingAlgorithm = &sortFIFO;
        else if (_SPFEnabled)
            _schedulingAlgorithm = &sortSPF;
        else if (_EDFEnabled)
            _schedulingAlgorithm = &sortEDF;
        else // default
            _schedulingAlgorithm = &sortLIFO;

        if (_widestPathsEnabled || _kspYenEnabled || _obliviousRoutingEnabled || _heuristicPathsEnabled)
            initializePathMaps(pathFileName);

        _epsilon = pow(10, -6);
        cout << "epsilon" << _epsilon << endl;
        _maxTravelTime = 0.0;
        _delta = 0.01; // to avoid divide by zero 
        
        if (_waterfillingEnabled || _priceSchemeEnabled || _landmarkRoutingEnabled || _dctcpEnabled){
           _kValue = par("numPathChoices");
        }
        
        setNumNodes(topologyFile_);
        generateTransUnitList(workloadFile_);
        generateChannelsBalancesMap(topologyFile_);
    }
    
    // set index and compute the top percentile size to choose elements accordingly
    setIndex(getIndex());
    maxPercentileHeapSize =  round(_numSplits[myIndex()].size() * _percentile);
    statNumTries.push(0);
    
    // Assign gates to all the payment channels
    const char * gateName = "out";
    cGate *destGate = NULL;

    int i = 0;
    int gateSize = gate(gateName, 0)->size();
    
    do {
        destGate = gate(gateName, i++);
        cGate *nextGate = destGate->getNextGate();
        if (nextGate) {
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
        int key =iter->first; //node

        //fill in balance field of nodeToPaymentChannel
        nodeToPaymentChannel[key].balance = _balances[make_tuple(myIndex(),key)];
        nodeToPaymentChannel[key].balanceEWMA = nodeToPaymentChannel[key].balance;

        // intialize capacity
        double balanceOpp =  _balances[make_tuple(key, myIndex())];
        nodeToPaymentChannel[key].origTotalCapacity = nodeToPaymentChannel[key].balance + balanceOpp;

        //initialize queuedTransUnits
        vector<tuple<int, double , routerMsg *, Id, simtime_t>> temp;
        make_heap(temp.begin(), temp.end(), _schedulingAlgorithm);
        nodeToPaymentChannel[key].queuedTransUnits = temp;

        //register PerChannel signals
        if (_signalsEnabled) {
            simsignal_t signal;
            signal = registerSignalPerChannel("numInQueue", key);
            nodeToPaymentChannel[key].amtInQueuePerChannelSignal = signal;

            signal = registerSignalPerChannel("balance", key);
            nodeToPaymentChannel[key].balancePerChannelSignal = signal;
            
            signal = registerSignalPerChannel("capacity", key);
            nodeToPaymentChannel[key].capacityPerChannelSignal = signal;
            
            signal = registerSignalPerChannel("timeInFlight", key);
            nodeToPaymentChannel[key].timeInFlightPerChannelSignal = signal;

            signal = registerSignalPerChannel("numInflight", key);
            nodeToPaymentChannel[key].numInflightPerChannelSignal = signal;
            
            signal = registerSignalPerChannel("bank", key);
            nodeToPaymentChannel[key].bankSignal = signal;
            
            signal = registerSignalPerChannel("implicitRebalancingAmt", key);
            nodeToPaymentChannel[key].implicitRebalancingAmtPerChannelSignal = signal;
            
            signal = registerSignalPerChannel("explicitRebalancingAmt", key);
            nodeToPaymentChannel[key].explicitRebalancingAmtPerChannelSignal = signal;
        }
    }

    //initialize signals with all other nodes in graph
    for (int i = 0; i < _numHostNodes; ++i) {
        if (_destList[myIndex()].count(i) > 0) {
            simsignal_t signal;
            if (_signalsEnabled) {
                signal = registerSignalPerDest("rateCompleted", i, "_Total");
                rateCompletedPerDestSignals[i] = signal;

                signal = registerSignalPerDest("rateAttempted", i, "_Total");
                rateAttemptedPerDestSignals[i] = signal;

                signal = registerSignalPerDest("rateArrived", i, "_Total");
                rateArrivedPerDestSignals[i] = signal;

                signal = registerSignalPerDest("numTimedOut", i, "_Total");
                numTimedOutPerDestSignals[i] = signal;

                signal = registerSignalPerDest("numPending", i, "_Total");
                numPendingPerDestSignals[i] = signal;

                signal = registerSignalPerDest("numArrived", i, "_Total");
                numArrivedPerDestSignals[i] = signal;

                signal = registerSignalPerDest("numCompleted", i, "_Total");
                numCompletedPerDestSignals[i] = signal;
                
                signal = registerSignalPerDest("fracSuccessful", i, "_Total");
                fracSuccessfulPerDestSignals[i] = signal;

                signal = registerSignalPerDest("rateFailed", i, "");
                rateFailedPerDestSignals[i] = signal;
            }

            statRateCompleted[i] = 0;
            statAmtCompleted[i] = 0;
            statNumCompleted[i] = 0;
            statRateAttempted[i] = 0;
            statAmtAttempted[i] = 0;
            statRateArrived[i] = 0;
            statNumArrived[i] = 0;
            statAmtArrived[i] = 0;
            statNumTimedOut[i] = 0;;
            statRateFailed[i] = 0;
            statAmtFailed[i] = 0;
            statCompletionTimes[i] = 0;
        }
    }
    
    // generate first transaction
    generateNextTransaction();

    //generate stat message
    routerMsg *statMsg = generateStatMessage();
    scheduleAt(simTime() + 0, statMsg);

    if (_timeoutEnabled){
       routerMsg *clearStateMsg = generateClearStateMessage();
       scheduleAt(simTime()+ _clearRate, clearStateMsg);
    }

    // generate rebalancing trigger messages
    if (_rebalancingEnabled) {
        routerMsg *triggerRebalancingMsg = generateTriggerRebalancingMessage();
        scheduleAt(simTime() + _rebalanceRate, triggerRebalancingMsg);
        
        routerMsg *computeMinBalanceMsg = generateComputeMinBalanceMessage();
        scheduleAt(simTime() + _computeBalanceRate, computeMinBalanceMsg);
    }
}

/* function that is called at the end of the simulation that
 * deletes any remaining messages and records scalars
 */
void hostNodeBase::finish() {
    deleteMessagesInQueues();

    for (int it = 0; it < _numHostNodes; ++it) {
        if (_destList[myIndex()].count(it) > 0) {
            char buffer[30];
            sprintf(buffer, "rateCompleted %d -> %d", myIndex(), it);
            recordScalar(buffer, statRateCompleted[it]);
            sprintf(buffer, "amtCompleted %d -> %d", myIndex(), it);
            recordScalar(buffer, statAmtCompleted[it]);

            sprintf(buffer, "rateAttempted %d -> %d", myIndex(), it);
            recordScalar(buffer, statRateAttempted[it]);
            sprintf(buffer, "amtAttempted  %d -> %d", myIndex(), it);
            recordScalar(buffer, statAmtAttempted[it]);

            sprintf(buffer, "rateArrived %d -> %d", myIndex(), it);
            recordScalar(buffer, statRateArrived[it]);
            sprintf(buffer, "amtArrived  %d -> %d", myIndex(), it);
            recordScalar(buffer, statAmtArrived[it]);

            sprintf(buffer, "completionTime %d -> %d ", myIndex(), it);
            recordScalar(buffer, statCompletionTimes[it]);
        }
    }

    // print all the PQ items for number of tries 
    while (!statNumTries.empty()) {
        char buffer[350];
        sprintf(buffer, "retries top percentile %d ", myIndex());
        recordScalar(buffer, statNumTries.top());
        statNumTries.pop();
    }


    if (myIndex() == 0) {
        // can be done on a per node basis also if need be
        // all in seconds
        recordScalar("max travel time", _maxTravelTime);
        recordScalar("delta", _delta);
        recordScalar("average delay", _avgDelay/1000.0);
        recordScalar("epsilon", _epsilon);
        recordScalar("cannonical RTT", _cannonicalRTT);

        for (auto const& x : _transactionCompletionBySize) {
            double amount = x.first;
            int completed = x.second;
            int arrived = _transactionArrivalBySize[amount];
            
            char buffer[60];
            sprintf(buffer, "size %d: arrived (%d) completed", int(amount), arrived);
            recordScalar(buffer, completed);
            
            if (completed > 0) {
                double avg = _txnAvgCompTimeBySize[amount] / completed;
                sprintf(buffer, "size %d: avg_comp_time ", int(amount));
                recordScalar(buffer, avg);

                _tailCompBySizeFile << amount << ": ";
                for (auto const& time : _txnTailCompTimesBySize[amount]) 
                    _tailCompBySizeFile << time << " ";
                _tailCompBySizeFile << endl;
                _txnTailCompTimesBySize[amount].clear();
            }
        }
        _tailCompBySizeFile.close(); 
    }
}


/*
 *  given an adjacent node, and TransUnit queue of things to send to that node, sends
 *  TransUnits until channel funds are too low
 *  calls forwardTransactionMessage on every individual TransUnit
 *  returns true when it still can continue processing more transactions
 */
bool hostNodeBase:: processTransUnits(int dest, vector<tuple<int, double , routerMsg *, Id, simtime_t>>& q) {
    int successful = true;
    while ((int)q.size() > 0 && successful == 1) {
        pop_heap(q.begin(), q.end(), _schedulingAlgorithm);
        successful = forwardTransactionMessage(get<2>(q.back()), dest, get<4>(q.back()));
        if (successful == 1){
            q.pop_back();
        }
    }
    return (successful != 0); // anything other than balance exhausted implies you can go on
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
        for (auto &rMsg : nodeToDestInfo[dest].transWaitingToBeSent) {
            auto tMsg = rMsg->getEncapsulatedPacket();
            rMsg->decapsulate();
            delete tMsg;
            delete rMsg;
        }
        nodeToDestInfo[dest].transWaitingToBeSent.clear();
    }
}

/* helper method to set a particular payment channel's balance to the passed in amount 
 */ 
void hostNodeBase::setPaymentChannelBalanceByNode(int node, double amt){
       nodeToPaymentChannel[node].balance = amt;
}
