#include "hostNodeLndBaseline.h"
Define_Module(hostNodeLndBaseline);

//instaniate global parameter for hostNodeLndBaseline
double _restorePeriod;
int _numAttemptsLndBaseline;
vector<int> succRetriesList, failRetriesList;

/* helper function for sorting heap by prune time */
bool sortPrunedChannelsFunction(tuple<simtime_t, tuple<int,int>> x, tuple<simtime_t, tuple<int, int>> y){
    simtime_t xTime = get<0>( x );
    simtime_t yTime = get<0>( y );
    return xTime > yTime; //makes smallest simtime_t appear first
} 


/* helper function to record the tail retries for successful and failed transactions
 * for LND */
void hostNodeLndBaseline::recordTailRetries(simtime_t timeSent, bool success, int retries){
    if (timeSent >= _transStatStart && timeSent <= _transStatEnd) {
        vector<int> *retryList = success ? &succRetriesList : &failRetriesList;
        retryList->push_back(retries);
        if (retryList->size() == 1000) {
            if (success) {
                for (auto const& time : *retryList) 
                    _succRetriesFile << time << " ";
                _succRetriesFile << endl;
            } else {
                for (auto const& time : *retryList) 
                    _failRetriesFile << time << " ";
                _failRetriesFile << endl;
            }
            retryList->clear();
        }
    }
}


/* makes local copy of global _channels data structure, without 
 * delay, as paths are calculated using BFS (not weighted edges)
 * use this to periodically prune channels */
void hostNodeLndBaseline::initializeMyChannels(){
    //not going to store delay, because using BFS to find shortest paths
    _activeChannels.clear();
    for (auto nodeIter: _channels){
        int node = nodeIter.first;
        _activeChannels[node] = {};
        vector<pair<int, int>> edgeDelayVec = nodeIter.second;
        for (auto edgeIter: edgeDelayVec){
            int destNode = edgeIter.first;
            _activeChannels[node].insert(destNode);
        }
    }
}

/* generates next path, but adding in the edges whose retore times are over, then running
 * BFS on _activeChannels */
vector<int>  hostNodeLndBaseline::generateNextPath(int destNodePath){
    if (_prunedChannelsList.size() > 0){
        tuple<simtime_t, tuple<int, int>> currentEdge =  _prunedChannelsList.front();
        while (_prunedChannelsList.size()>0 && (get<0>(currentEdge) + _restorePeriod < simTime())){
            int sourceNode = get<0>(get<1>(currentEdge));
            int destNode = get<1>(get<1>(currentEdge));
            _activeChannels[sourceNode].insert(destNode);
            _prunedChannelsList.pop_front();
            if (_prunedChannelsList.size() > 0)
                currentEdge =  _prunedChannelsList.front();
        }
    }
    vector<int> resultPath = breadthFirstSearchByGraph(myIndex(),destNodePath, _activeChannels);
    return resultPath;       
}

/* given source and destination, will remove edge from _activeChannels, or if already removed,
 * update the time pruned */
void hostNodeLndBaseline::pruneEdge(int sourceNode, int destNode){
    tuple<int, int> edgeTuple = make_tuple(sourceNode, destNode);        
    auto iter = _activeChannels[sourceNode].find(destNode); 
    if (iter != _activeChannels[sourceNode].end())
    {
        _activeChannels[sourceNode].erase(iter);
        _prunedChannelsList.push_back(make_tuple(simTime(), make_tuple(sourceNode, destNode)));
    }
    else {
        auto iterHeap = find_if(_prunedChannelsList.begin(),
            _prunedChannelsList.end(),
            [&edgeTuple](const tuple<simtime_t, tuple<int,int>>& p)
            { return ((get<0>(get<1>(p)) == get<0>(edgeTuple)) && 
                    (get<1>(get<1>(p)) == get<1>(edgeTuple))); });

        if (iterHeap != _prunedChannelsList.end()){
            _prunedChannelsList.erase(iterHeap);
            _prunedChannelsList.push_back(make_tuple(simTime(), make_tuple(sourceNode, destNode)));
        }
    }
}


/* initialization function to initialize parameters */
void hostNodeLndBaseline::initialize(){
    hostNodeBase::initialize();
    for (int i = 0; i < _numHostNodes; ++i) {
        if (_destList[myIndex()].count(i) > 0 && _signalsEnabled) { 
            simsignal_t signal;
            signal = registerSignalPerDest("numPathsPerTrans", i, "");
            numPathsPerTransPerDestSignals.push_back(signal);
        }
    }
    
    _restorePeriod = 5.0;
    _numAttemptsLndBaseline = 20;
    initializeMyChannels(); 
    _prunedChannelsList = {};
}


/* calls base method and also reports any unreported success and failure retries
 */
void hostNodeLndBaseline::finish(){
    hostNodeBase::finish();

    if (myIndex() == 0) {
        for (auto const& t : succRetriesList) 
            _succRetriesFile << t << " ";
        _succRetriesFile << endl;
        _succRetriesFile.close();

        for (auto const& t : failRetriesList) 
            _failRetriesFile << t << " ";
        _failRetriesFile << endl;
        _failRetriesFile.close();
    }
}

/* generateAckMessage that encapsulates transaction message to use for reattempts 
 * Note: this is different from the hostNodeBase version that will delete the 
 * passed in transaction message */
routerMsg *hostNodeLndBaseline::generateAckMessage(routerMsg* ttmsg, bool isSuccess) {
    int sender = (ttmsg->getRoute())[0];
    int receiver = (ttmsg->getRoute())[(ttmsg->getRoute()).size() - 1];
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
    aMsg->setIsMarked(transMsg->getIsMarked());
    if (!isSuccess){
        aMsg->setFailedHopNum((route.size() - 1) - ttmsg->getHopCount());
    }
    
    //no need to set secret - not modelled
    reverse(route.begin(), route.end());
    msg->setRoute(route);

    //need to reverse path from current hop number in case of partial failure
    msg->setHopCount((route.size() - 1) - ttmsg->getHopCount());

    msg->setMessageType(ACK_MSG);
    ttmsg->decapsulate();
    aMsg->encapsulate(transMsg);
    msg->encapsulate(aMsg);
    delete ttmsg;
    return msg;
}


/* main routine for handling a new transaction under lnd baseline 
 */
void hostNodeLndBaseline::handleTransactionMessageSpecialized(routerMsg* ttmsg){
    transactionMsg *transMsg = check_and_cast<transactionMsg *>(ttmsg->getEncapsulatedPacket());
    int hopcount = ttmsg->getHopCount();
    vector<tuple<int, double , routerMsg *, Id>> *q;
    int destNode = transMsg->getReceiver();
    int transactionId = transMsg->getTransactionId();

    SplitState* splitInfo = &(_numSplits[myIndex()][transMsg->getLargerTxnId()]);
    splitInfo->numArrived += 1;

    // if its at the sender, initiate probes, when they come back,
    // call normal handleTransactionMessage
    if (ttmsg->isSelfMessage()) {
        if (splitInfo->numArrived == 1)
            splitInfo->firstAttemptTime = simTime().dbl();

        if (transMsg->getTimeSent() >= _transStatStart && 
            transMsg->getTimeSent() <= _transStatEnd) {
            statAmtArrived[destNode] += transMsg->getAmount();
            statAmtAttempted[destNode] += transMsg->getAmount();
            
            if (splitInfo->numArrived == 1) {       
                statRateArrived[destNode] += 1; 
                statRateAttempted[destNode] += 1;
                splitInfo->numTries += 1; 
            }
        }
        if (splitInfo->numArrived == 1)     
            statNumArrived[destNode] += 1;

        vector<int> newRoute = generateNextPath(destNode);
        //note: number of paths attempted is calculated as pathIndex + 1, so if fails
        //without attempting any paths, want 0 = -1+1
        transMsg->setPathIndex(-1);
        if (newRoute.size() > 0) {
            transMsg->setPathIndex(0);
            ttmsg->setRoute(newRoute);
            handleTransactionMessage(ttmsg, true);
        }
        else {
            routerMsg * failedAckMsg = generateAckMessage(ttmsg, false);
            handleAckMessageNoMoreRoutes(failedAckMsg, true);
        }
    }
    else {
        handleTransactionMessage(ttmsg,true);
    }
}

/* handleAckMessageNoMoreRoute - increments failed statistics, and deletes all three messages:
 * ackMsg, transMsg, routerMsg */
void hostNodeLndBaseline::handleAckMessageNoMoreRoutes(routerMsg *msg, bool toDelete){
    ackMsg *aMsg = check_and_cast<ackMsg *>(msg->getEncapsulatedPacket());
    transactionMsg *transMsg = check_and_cast<transactionMsg *>(aMsg->getEncapsulatedPacket());
    int numPathsAttempted = aMsg->getPathIndex() + 1;
    SplitState* splitInfo = &(_numSplits[myIndex()][aMsg->getLargerTxnId()]);

    if (aMsg->getTimeSent() >= _transStatStart && aMsg->getTimeSent() <= _transStatEnd) {
        statRateFailed[aMsg->getReceiver()] = statRateFailed[aMsg->getReceiver()] + 1;
        statAmtFailed[aMsg->getReceiver()] += aMsg->getAmount();
        recordTailRetries(aMsg->getTimeSent(), false, splitInfo->numTries); 
    }

    if (toDelete) {
        aMsg->decapsulate();
        delete transMsg;
        msg->decapsulate();
        delete aMsg;
        delete msg;
    }
}

/* handles ack messages - guaranteed to be returning from an attempted path to the sender
 * if successful or no more attempts left, call hostNodeBase's handleAckMsgSpecialized
 * if need to reattempt:
 * - reset state by handlingAckMessage
 * - see if more routes possible: if possible, then generate new transaction msg, else fail it
 */
void hostNodeLndBaseline::handleAckMessageSpecialized(routerMsg *msg)
{
    ackMsg *aMsg = check_and_cast<ackMsg *>(msg->getEncapsulatedPacket());
    transactionMsg *transMsg = check_and_cast<transactionMsg *>(aMsg->getEncapsulatedPacket());
    int transactionId = transMsg->getTransactionId();
    int destNode = msg->getRoute()[0];
    double largerTxnId = aMsg->getLargerTxnId();
    SplitState* splitInfo = &(_numSplits[myIndex()][largerTxnId]);
    
    auto iter = canceledTransactions.find(make_tuple(transactionId, 0, 0, 0, 0));
    int numPathsAttempted = aMsg->getPathIndex() + 1;

    if (aMsg->getIsSuccess() || (numPathsAttempted == _numAttemptsLndBaseline || 
            (_timeoutEnabled && iter != canceledTransactions.end()))) {
        //no more attempts allowed
        if (iter != canceledTransactions.end())
            canceledTransactions.erase(iter);

        if (aMsg->getIsSuccess()) {
            splitInfo->numReceived += 1;
            if (transMsg->getTimeSent() >= _transStatStart && 
                    transMsg->getTimeSent() <= _transStatEnd) {
                statAmtCompleted[destNode] += aMsg->getAmount();

                if (splitInfo->numTotal == splitInfo->numReceived) {
                    statRateCompleted[destNode] += 1;
                    _transactionCompletionBySize[splitInfo->totalAmount] += 1;
                    double timeTaken = simTime().dbl() - splitInfo->firstAttemptTime;
                    statCompletionTimes[destNode] += timeTaken * 1000;
                    _txnAvgCompTimeBySize[splitInfo->totalAmount] += timeTaken * 1000;
                    recordTailCompletionTime(aMsg->getTimeSent(), splitInfo->totalAmount, timeTaken * 1000);
                    recordTailRetries(aMsg->getTimeSent(), true, splitInfo->numTries);
                }
            }
            if (splitInfo->numTotal == splitInfo->numReceived) 
                statNumCompleted[destNode] += 1; 
        }
        else {
            statNumTimedOut[destNode] += 1;
        }
        aMsg->decapsulate();
        delete transMsg;
        hostNodeBase::handleAckMessage(msg);
    }
    else { 
        // more attempts allowed
        int newIndex = aMsg->getPathIndex() + 1;
        transMsg->setPathIndex(newIndex);

        //prune edge
        int failedHopNum = aMsg->getFailedHopNum();
        int failedSource = msg->getRoute()[failedHopNum];
        int failedDest = msg->getRoute()[failedHopNum - 1];
        pruneEdge(failedSource, failedDest);

        vector<int> newRoute = generateNextPath(transMsg->getReceiver());
        if (newRoute.size() == 0) {
            handleAckMessageNoMoreRoutes(msg, false);
            hostNodeBase::handleAckMessage(msg);
        } else if (iter != canceledTransactions.end()) {
            canceledTransactions.erase(iter);
            handleAckMessageNoMoreRoutes(msg, false);
            hostNodeBase::handleAckMessage(msg);
        }
        else {
            //generate new router  message for transaction message
            char msgname[MSGSIZE];
            sprintf(msgname, "tic-%d-to-%d transactionMsg", transMsg->getSender(), transMsg->getReceiver());
            routerMsg* ttmsg = new routerMsg(msgname);
            ttmsg->setRoute(newRoute);
            ttmsg->setHopCount(0);
            ttmsg->setMessageType(TRANSACTION_MSG);
            aMsg->decapsulate();
            hostNodeBase::handleAckMessage(msg);
            ttmsg->encapsulate(transMsg);
            splitInfo->numTries += 1;
            handleTransactionMessage(ttmsg, true);
        }   
    }
}

