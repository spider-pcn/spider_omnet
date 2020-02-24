#include "hostNodeLandmarkRouting.h"

// set of landmarks for landmark routing
vector<tuple<int,int>> _landmarksWithConnectivityList = {};
vector<int> _landmarks;

Define_Module(hostNodeLandmarkRouting);

/* generates the probe message for a particular destination and a particur path
 * identified by the list of hops and the path index
 */
routerMsg* hostNodeLandmarkRouting::generateProbeMessage(int destNode, int pathIdx, vector<int> path){
    char msgname[MSGSIZE];
    sprintf(msgname, "tic-%d-to-%d probeMsg [idx %d]", myIndex(), destNode, pathIdx);
    probeMsg *pMsg = new probeMsg(msgname);
    pMsg->setSender(myIndex());
    pMsg->setPathIndex(pathIdx);
    pMsg->setReceiver(destNode);
    pMsg->setIsReversed(false);
    vector<double> pathBalances;
    pMsg->setPathBalances(pathBalances);
    pMsg->setPath(path);

    sprintf(msgname, "tic-%d-to-%d router-probeMsg [idx %d]", myIndex(), destNode, pathIdx);
    routerMsg *rMsg = new routerMsg(msgname);
    rMsg->setRoute(path);

    rMsg->setHopCount(0);
    rMsg->setMessageType(PROBE_MSG);

    rMsg->encapsulate(pMsg);
    return rMsg;
}

/* forwards probe messages for waterfilling alone that appends the current balance
 * to the list of balances
 */
void hostNodeLandmarkRouting::forwardProbeMessage(routerMsg *msg){
    // Increment hop count.
    msg->setHopCount(msg->getHopCount()+1);
    //use hopCount to find next destination
    int nextDest = msg->getRoute()[msg->getHopCount()];

    probeMsg *pMsg = check_and_cast<probeMsg *>(msg->getEncapsulatedPacket());
    if (pMsg->getIsReversed() == false){
        vector<double> *pathBalances = & ( pMsg->getPathBalances());
        (*pathBalances).push_back(nodeToPaymentChannel[nextDest].balanceEWMA);
    }

   if (_loggingEnabled) cout << "forwarding " << msg->getMessageType() << " at " 
       << simTime() << endl;
   send(msg, nodeToPaymentChannel[nextDest].gate);
}


/* overall controller for handling messages that dispatches the right function
 * based on message type in Landmark Routing
 */
void hostNodeLandmarkRouting::handleMessage(cMessage *msg) {
    routerMsg *ttmsg = check_and_cast<routerMsg *>(msg);
    if (simTime() > _simulationLength){
        auto encapMsg = (ttmsg->getEncapsulatedPacket());
        ttmsg->decapsulate();
        delete ttmsg;
        delete encapMsg;
        return;
    } 

    switch(ttmsg->getMessageType()) {
        case PROBE_MSG:
             if (_loggingEnabled) cout<< "[HOST "<< myIndex() 
                 <<": RECEIVED PROBE MSG] "<< ttmsg->getName() << endl;
             handleProbeMessage(ttmsg);
             if (_loggingEnabled) cout<< "[AFTER HANDLING:]  "<< endl;
             break;

        default:
             hostNodeBase::handleMessage(msg);
    }
}

/* main routine for handling a new transaction under landmark routing 
 * In particular, initiate probes at sender and send acknowledgements
 * at the receiver
 */
void hostNodeLandmarkRouting::handleTransactionMessageSpecialized(routerMsg* ttmsg){
    transactionMsg *transMsg = check_and_cast<transactionMsg *>(ttmsg->getEncapsulatedPacket());
    int hopcount = ttmsg->getHopCount();
    vector<tuple<int, double , routerMsg *, Id>> *q;
    int destNode = transMsg->getReceiver();
    int destination = destNode;
    int transactionId = transMsg->getTransactionId();
    SplitState* splitInfo = &(_numSplits[myIndex()][transMsg->getLargerTxnId()]);

    // if its at the sender, initiate probes, when they come back,
    // call normal handleTransactionMessage
    if (ttmsg->isSelfMessage()) {
        if (transMsg->getTimeSent() >= _transStatStart && 
            transMsg->getTimeSent() <= _transStatEnd) {
            statRateArrived[destination] += 1;
            statAmtArrived[destination] += transMsg->getAmount();
            splitInfo->firstAttemptTime = simTime().dbl();
        }
        statNumArrived[destination] += 1; 

        AckState * s = new AckState();
        s->amtReceived = 0;
        s->amtSent = transMsg->getAmount();
        transToAmtLeftToComplete[transMsg->getTransactionId()] = *s;

        // if destination hasn't been encountered, find paths
        if (nodeToShortestPathsMap.count(destNode) == 0 ){
            vector<vector<int>> kShortestRoutes = getKShortestRoutesLandmarkRouting(transMsg->getSender(), 
                    destNode, _kValue);
            initializePathInfoLandmarkRouting(kShortestRoutes, destNode);
        }
        
        initializeLandmarkRoutingProbes(ttmsg, transMsg->getTransactionId(), destNode);
    }
    else if (ttmsg->getHopCount() ==  ttmsg->getRoute().size() - 1) { 
       handleTransactionMessage(ttmsg, false); 
    }
    else {
        cout << "entering this case " << endl;
        assert(false);
    }
}


/* handles the special time out mechanism for landmark routing which is responsible
 * for sending time out messages on all paths that may have seen this txn and 
 * marking the txn as cancelled
 */
void hostNodeLandmarkRouting::handleTimeOutMessage(routerMsg* ttmsg){
    timeOutMsg *toutMsg = check_and_cast<timeOutMsg *>(ttmsg->getEncapsulatedPacket());

    if (ttmsg->isSelfMessage()) { 
        //is at the sender
        int transactionId = toutMsg->getTransactionId();
        int destination = toutMsg->getReceiver();

        if (transToAmtLeftToComplete.count(transactionId) == 0) {
                delete ttmsg;
                return;
        }
        
        for (auto p : (nodeToShortestPathsMap[destination])){
            int pathIndex = p.first;
            tuple<int,int> key = make_tuple(transactionId, pathIndex);
            if(_loggingEnabled) {
                cout << "transPathToAckState.count(key): " 
                   << transPathToAckState.count(key) << endl;
                cout << "transactionId: " << transactionId 
                    << "; pathIndex: " << pathIndex << endl;
            }
            
            if (transPathToAckState[key].amtSent > transPathToAckState[key].amtReceived + _epsilon) {
                int nextNode = nodeToShortestPathsMap[destination][pathIndex].path[1];
                CanceledTrans ct = make_tuple(toutMsg->getTransactionId(), 
                        simTime(), -1, nextNode, destination);
                canceledTransactions.insert(ct);
            }
            else {
                transPathToAckState.erase(key);
            }
        }
        delete ttmsg;
    }
    else{
        // at the receiver
        CanceledTrans ct = make_tuple(toutMsg->getTransactionId(),simTime(),
                (ttmsg->getRoute())[ttmsg->getHopCount()-1], -1, toutMsg->getReceiver());
        canceledTransactions.insert(ct);
        ttmsg->decapsulate();
        delete toutMsg;
        delete ttmsg;
    }
}


/* handles to logic for ack messages in the presence of timeouts
 * in particular, removes the transaction from the cancelled txns
 * to mark that it has been received 
 * it uses the transAmtSent vs Received to detect if txn is complete
 * and therefore is different from the base class 
 */
void hostNodeLandmarkRouting::handleAckMessageTimeOut(routerMsg* ttmsg){
    ackMsg *aMsg = check_and_cast<ackMsg *>(ttmsg->getEncapsulatedPacket());
    int transactionId = aMsg->getTransactionId();

    if (aMsg->getIsSuccess()) {
        double totalAmtReceived = (transToAmtLeftToComplete[transactionId]).amtReceived +
            aMsg->getAmount();
        if (totalAmtReceived != transToAmtLeftToComplete[transactionId].amtSent) 
            return;
        
        auto iter = canceledTransactions.find(make_tuple(transactionId, 0, 0, 0, 0));
        if (iter!=canceledTransactions.end()) {
            canceledTransactions.erase(iter);
        }
    }
}


/* specialized ack handler that does the routine for handling acks
 * across paths. In particular, collects/updates stats for this path alone
 * NOTE: acks are on the reverse path relative to the original sender
 */
void hostNodeLandmarkRouting::handleAckMessageSpecialized(routerMsg* ttmsg) {
    ackMsg *aMsg = check_and_cast<ackMsg *>(ttmsg->getEncapsulatedPacket());
    int receiver = aMsg->getReceiver();
    int pathIndex = aMsg->getPathIndex();
    int transactionId = aMsg->getTransactionId();
    tuple<int,int> key = make_tuple(transactionId, pathIndex);
   
    if (aMsg->getIsSuccess()) { 
        if (transToAmtLeftToComplete.count(transactionId) == 0){
            cout << "error, transaction " << transactionId 
              <<" htlc index:" << aMsg->getHtlcIndex() 
              << " acknowledged at time " << simTime() 
              << " wasn't written to transToAmtLeftToComplete for amount " <<  aMsg->getAmount() << endl;
        }
        else {
            (transToAmtLeftToComplete[transactionId]).amtReceived += aMsg->getAmount();
            if (aMsg->getTimeSent() >= _transStatStart && aMsg->getTimeSent() <= _transStatEnd) {
                statAmtCompleted[receiver] += aMsg->getAmount();
            }
            
            double amtReceived = transToAmtLeftToComplete[transactionId].amtReceived;
            double amtSent = transToAmtLeftToComplete[transactionId].amtSent;

            if (amtReceived < amtSent + _epsilon && amtReceived > amtSent -_epsilon) {
                nodeToShortestPathsMap[receiver][pathIndex].statRateCompleted += 1;

                if (aMsg->getTimeSent() >= _transStatStart && aMsg->getTimeSent() <= _transStatEnd) {
                    statRateCompleted[receiver] += 1;
                    _transactionCompletionBySize[amtSent] += 1;

                    SplitState* splitInfo = &(_numSplits[myIndex()][aMsg->getLargerTxnId()]);
                    double timeTaken = simTime().dbl() - splitInfo->firstAttemptTime;
                    statCompletionTimes[receiver] += timeTaken * 1000;
                    _txnAvgCompTimeBySize[amtSent] += timeTaken * 1000;
                    recordTailCompletionTime(aMsg->getTimeSent(), amtSent, timeTaken * 1000);
                }
                statNumCompleted[receiver] += 1; 
                transToAmtLeftToComplete.erase(aMsg->getTransactionId());
            }
            transPathToAckState[key].amtReceived += aMsg->getAmount();
        }
    }
    hostNodeBase::handleAckMessage(ttmsg);
}


/* handler that clears additional state particular to lr 
 * when a cancelled transaction is deemed no longer completeable
 * in particular it clears the state that tracks how much of a
 * transaction is still pending
 * calls the base class's handler after its own handler
 */
void hostNodeLandmarkRouting::handleClearStateMessage(routerMsg *ttmsg) {
    for ( auto it = canceledTransactions.begin(); it!= canceledTransactions.end(); it++){
        int transactionId = get<0>(*it);
        simtime_t msgArrivalTime = get<1>(*it);
        int prevNode = get<2>(*it);
        int nextNode = get<3>(*it);
        int destNode = get<4>(*it);
        
        if (simTime() > (msgArrivalTime + _maxTravelTime + 1)){
            for (auto p : nodeToShortestPathsMap[destNode]) {
                int pathIndex = p.first;
                tuple<int,int> key = make_tuple(transactionId, pathIndex);
                if (transPathToAckState.count(key) != 0) {
                    transPathToAckState.erase(key);
                }
            }
        }
    }
    hostNodeBase::handleClearStateMessage(ttmsg);
}

/* handle Probe Message for Landmark Routing 
 * In essence, is waiting for all the probes, finding those paths 
 * with non-zero bottleneck balance and splitting the transaction
 * amongst them
 */
void hostNodeLandmarkRouting::handleProbeMessage(routerMsg* ttmsg){
    probeMsg *pMsg = check_and_cast<probeMsg *>(ttmsg->getEncapsulatedPacket());
    int transactionId = pMsg->getTransactionId();

    if (simTime() > _simulationLength ){
        ttmsg->decapsulate();
        delete pMsg;
        delete ttmsg;
        return;
    }

    bool isReversed = pMsg->getIsReversed();
    int nextDest = ttmsg->getRoute()[ttmsg->getHopCount()+1];
    
    if (isReversed == true){ 
       //store times into private map, delete message
       int pathIdx = pMsg->getPathIndex();
       int destNode = pMsg->getReceiver();
       double bottleneck = minVectorElemDouble(pMsg->getPathBalances());
       ProbeInfo *probeInfo = &(transactionIdToProbeInfoMap[transactionId]);
       
       probeInfo->probeReturnTimes[pathIdx] = simTime();
       probeInfo->numProbesWaiting -= 1; 
       probeInfo->probeBottlenecks[pathIdx] = bottleneck;

       // once all probes are back
       if (probeInfo->numProbesWaiting == 0){ 
           int numPathsPossible = 0;
           for (auto bottleneck: probeInfo->probeBottlenecks){
               if (bottleneck > 0){
                   numPathsPossible++;
               }
           }
           
           transactionMsg *transMsg = check_and_cast<transactionMsg*>(
                   probeInfo->messageToSend->getEncapsulatedPacket());
           vector<double> amtPerPath(probeInfo->probeBottlenecks.size());

           if (numPathsPossible > 0 && 
                   randomSplit(transMsg->getAmount(), probeInfo->probeBottlenecks, amtPerPath)) {
               if (transMsg->getTimeSent() >= _transStatStart && transMsg->getTimeSent() <= _transStatEnd) {
                   statRateAttempted[destNode] += 1;
                   statAmtAttempted[destNode] += transMsg->getAmount();
               }

               for (int i = 0; i < amtPerPath.size(); i++) {
                   double amt = amtPerPath[i];
                   if (amt > 0) {
                       tuple<int,int> key = make_tuple(transMsg->getTransactionId(), i); 
                       //update the data structure keeping track of how much sent and received on each path
                       if (transPathToAckState.count(key) == 0){
                           AckState temp = {};
                           temp.amtSent = amt;
                           temp.amtReceived = 0;
                           transPathToAckState[key] = temp;
                        }
                        else {
                            transPathToAckState[key].amtSent =  transPathToAckState[key].amtSent + amt;
                        }

                       // send a new transaction on that path with that amount
                       PathInfo *pathInfo = &(nodeToShortestPathsMap[destNode][i]);
                       routerMsg* lrMsg = generateTransactionMessageForPath(amt, 
                               pathInfo->path, i, transMsg); 
                       handleTransactionMessage(lrMsg, true /*revisit*/);
                   }
               }
           } 
           else {
                if (transMsg->getTimeSent() >= _transStatStart && transMsg->getTimeSent() <= _transStatEnd) {
                    statRateFailed[destNode] += 1;
                    statAmtFailed[destNode] += transMsg->getAmount();
                }
               transToAmtLeftToComplete.erase(transactionId);
           }

           probeInfo->messageToSend->decapsulate();
           delete transMsg;
           delete probeInfo->messageToSend;
           transactionIdToProbeInfoMap.erase(transactionId);
       }
       
       ttmsg->decapsulate();
       delete pMsg;
       delete ttmsg;
   }
   else { 
       //reverse and send message again from receiver
       pMsg->setIsReversed(true);
       ttmsg->setHopCount(0);
       vector<int> route = ttmsg->getRoute();
       reverse(route.begin(), route.end());
       ttmsg->setRoute(route);
       forwardMessage(ttmsg);
   }
}

/* function to compute a random split across all the paths for landmark routing
 */
bool hostNodeLandmarkRouting::randomSplit(double totalAmt, vector<double> bottlenecks, 
        vector<double> &amtPerPath) {
    vector<int> pathsPossible;
    vector<int> nextSet;
    double remainingAmt = totalAmt;

    // start with non-zero bottlneck paths
    for (int i = 0; i < bottlenecks.size(); i++)
        if (bottlenecks[i] > 0)
            pathsPossible.push_back(i);

    // keep allocating while you can
    while (remainingAmt > _epsilon - _epsilon && pathsPossible.size() > 0) {
        nextSet.clear();
        random_shuffle(pathsPossible.begin(), pathsPossible.end());
        for (int i : pathsPossible) {
            double amtToAdd = round((remainingAmt - 1) * (rand() / RAND_MAX + 1.) + 1);
            if (amtPerPath[i] + amtToAdd > bottlenecks[i])
                amtToAdd = bottlenecks[i] - amtPerPath[i];
            else 
                nextSet.push_back(i);
            amtPerPath[i] += amtToAdd;
            remainingAmt -= amtToAdd;
            if (remainingAmt <= _epsilon)
                break;
        }       
        pathsPossible = nextSet;
    }
    return (remainingAmt <= _epsilon);
}

/* initializes the table with the paths to use for Landmark Routing, everything else as 
 * to how many probes are in progress is initialized when probes are sent
 * This function only helps for memoization
 */
void hostNodeLandmarkRouting::initializePathInfoLandmarkRouting(vector<vector<int>> kShortestPaths, 
        int  destNode){ 
    nodeToShortestPathsMap[destNode] = {};
    for (int pathIdx = 0; pathIdx < kShortestPaths.size(); pathIdx++){
        PathInfo temp = {};
        nodeToShortestPathsMap[destNode][pathIdx] = temp;
        nodeToShortestPathsMap[destNode][pathIdx].path = kShortestPaths[pathIdx];
    }
    return;
}


/* initializes the actual balance queries for landmark routing and the state 
 * to keep track of how many of them are currently outstanding and how many of them
 * have returned with what balances
 * the msg passed is the transaction that triggered this set of probes which also
 * corresponds to the txn that needs to be sent out once all probes return
 */
void hostNodeLandmarkRouting::initializeLandmarkRoutingProbes(routerMsg * msg, 
        int transactionId, int destNode){
    ProbeInfo probeInfoTemp =  {};
    probeInfoTemp.messageToSend = msg; //message to send out once all probes return
    probeInfoTemp.probeReturnTimes = {}; //probeReturnTimes[0] is return time of first probe

    for (auto pTemp: nodeToShortestPathsMap[destNode]){
        int pathIndex = pTemp.first;
        PathInfo pInfo = pTemp.second;
        vector<int> path = pInfo.path;
        routerMsg * rMsg = generateProbeMessage(destNode , pathIndex, path);

        //set the transactionId in the generated message
        probeMsg *pMsg = check_and_cast<probeMsg *>(rMsg->getEncapsulatedPacket());
        pMsg->setTransactionId(transactionId);
        forwardProbeMessage(rMsg);

        probeInfoTemp.probeReturnTimes.push_back(-1);
        probeInfoTemp.probeBottlenecks.push_back(-1);
    }

    // set number of probes waiting on to be the number of total paths to 
    // this particular destination (might be less than k, so not safe to use that)
    probeInfoTemp.numProbesWaiting = nodeToShortestPathsMap[destNode].size();
    transactionIdToProbeInfoMap[transactionId] = probeInfoTemp;
    return;
}

/* function that is called at the end of the simulation that
 * deletes any remaining messages in transactionIdToProbeInfoMap
 */
void hostNodeLandmarkRouting::finish() {
    for (auto it = transactionIdToProbeInfoMap.begin(); it != transactionIdToProbeInfoMap.end(); it++) {
        ProbeInfo *probeInfo = &(it->second);
        transactionMsg *transMsg = check_and_cast<transactionMsg*>(
            probeInfo->messageToSend->getEncapsulatedPacket());
        probeInfo->messageToSend->decapsulate();
        delete transMsg;
        delete probeInfo->messageToSend;
    }
    hostNodeBase::finish();
}

