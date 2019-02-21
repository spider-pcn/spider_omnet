#include "hostNodeLandmarkRouting.h"

// set of landmarks for landmark routing
vector<int> _landmarks;

/* overall controller for handling messages that dispatches the right function
 * based on message type in Landmark Routing
 */
void hostNodeLandmarkRouting::handleMessage(routerMsg *ttmsg) {
    switch(ttmsg->getMessageType()) {
        case PROBE_MSG:
             if (_loggingEnabled) cout<< "[HOST "<< myIndex() 
                 <<": RECEIVED PROBE MSG] "<< msg->getName() << endl;
             handleProbeMessage(ttmsg);
             if (_loggingEnabled) cout<< "[AFTER HANDLING:]  "<< endl;
             break;
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

    // if its at the sender, initiate probes, when they come back,
    // call normal handleTransactionMessage
    if (ttmsg->isSelfMessage()) { 
        statNumArrived[destination] += 1; 
        statRateArrived[destination] += 1; 
        statRateAttempted[destination] += 1; 

        // if destination hasn't been encountered, find paths
        if (nodeToShortestPathsMap.count(destNode) == 0 ){
            vector<vector<int>> kShortestRoutes = getKShortestRoutesLandmarkRouting(transMsg->getSender(), 
                    destNode, _kValue);
            initializePathInfoLandmarkRouting(kShortestRoutes, destNode);
        }
        
        initializeLandmarkRoutingProbes(ttmsg, transMsg->getTransactionId(), destNode);
    }
    else if (ttmsg->getHopCount() ==  ttmsg->getRoute().size() - 1) { 
        // txn is at destination, add to incoming units
        int prevNode = ttmsg->getRoute()[ttmsg->getHopCount()-1];
        map<Id, double> *incomingTransUnits = &(nodeToPaymentChannel[prevNode].incomingTransUnits);
        (*incomingTransUnits)[make_tuple(transMsg->getTransactionId(), transMsg->getHtlcIndex())] = 
            transMsg->getAmount();

        if (_timeoutEnabled) {
            auto iter = find_if(canceledTransactions.begin(),
               canceledTransactions.end(),
               [&transactionId](const tuple<int, simtime_t, int, int, int>& p)
               { return get<0>(p) == transactionId; });

            if (iter!=canceledTransactions.end()){
                canceledTransactions.erase(iter);
            }
        }

        // send ack
        routerMsg* newMsg =  generateAckMessage(ttmsg);
        forwardMessage(newMsg);
        return;
    }
    else {
        assert(false);
    }
}


/* handle Probe Message for Landmark Routing 
 * In essence, is waiting for all the probes, finding those paths 
 * with non-zero bottleneck balance and choosing one randomly from 
 * those to send the txn on
 */
void hostNodeLandmarkRouting::handleProbeMessage(routerMsg* ttmsg){
    probeMsg *pMsg = check_and_cast<probeMsg *>(ttmsg->getEncapsulatedPacket());
    if (simTime()> _simulationLength ){
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
       int transactionId = pMsg->getTransactionId();
       double bottleneck = minVectorElemDouble(pMsg->getPathBalances());
       
       transactionIdToProbeInfoMap[transactionId].probeReturnTimes[pathIdx] = simTime();
       transactionIdToProbeInfoMap[transactionId].numProbesWaiting = 
           transactionIdToProbeInfoMap[transactionId].numProbesWaiting - 1;
       transactionIdToProbeInfoMap[transactionId].probeBottlenecks[pathIdx] = bottleneck;

       //check to see if all probes are back
       //cout << "transactionId: " << pMsg->getTransactionId();
       //cout << "numProbesWaiting: " 
       // << transactionIdToProbeInfoMap[transactionId].numProbesWaiting << endl;

       // once all probes are back
       if (transactionIdToProbeInfoMap[transactionId].numProbesWaiting == 0){ 
           // find total number of paths
           int numPathsPossible = 0;
           for (auto bottleneck: transactionIdToProbeInfoMap[transactionId].probeBottlenecks){
               if (bottleneck > 0){
                   numPathsPossible++;
               }
           }

           // construct probabilities to sample from 
           vector<double> probabilities;
           for (auto bottleneck: transactionIdToProbeInfoMap[transactionId].probeBottlenecks){
                if (bottleneck > 0) {
                    probabilities.push_back(1/numPathsPossible);
                }
                else{
                    probabilities.push_back(0);
                }
            }
           int indexToUse = sampleFromDistribution(probabilities); 

           // send transaction on this path
           transactionMsg *transMsg = check_and_cast<transactionMsg*>(
                   transactionIdToProbeInfoMap[transactionId].messageToSend->
                   getEncapsulatedPacket());
           //cout << "sent transaction on path: ";
           //printVector(nodeToShortestPathsMap[transMsg->getReceiver()][indexToUse].path);
           transactionIdToProbeInfoMap[transactionId].messageToSend->
               setRoute(nodeToShortestPathsMap[transMsg->getReceiver()][indexToUse].path);
           handleTransactionMessage(transactionIdToProbeInfoMap[transactionId].messageToSend, true /*revisit*/);
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


/* initializes the table with the paths to use for Landmark Routing, everything else as 
 * to how many probes are in progress is initialized when probes are sent
 * This function only helps for memoization
 */
void hostNode::initializePathInfoLandmarkRouting(vector<vector<int>> kShortestPaths, 
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
void hostNode::initializeLandmarkRoutingProbes(routerMsg * msg, int transactionId, int destNode){
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


