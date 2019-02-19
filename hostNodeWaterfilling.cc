#include "hostNodePriceWaterfilling.h"

// global parameters
// set to 1 to report exact instantaneous balances
double _ewmaFactor;

// parameters for smooth waterfilling
double _Tau;
double _Normalizer;


/* special type of time out message for waterfilling designed for a specific path so that
 * such messages will be sent on all paths considered for waterfilling
 */
routerMsg* hostNodeWaterfilling::generateTimeOutMessage(vector<int> path, 
        int transactionId, int receiver){
    char msgname[MSGSIZE];
    sprintf(msgname, "tic-%d-to-%d water-timeOutMsg", myIndex(), receiver);
    timeOutMsg *msg = new timeOutMsg(msgname);

    msg->setReceiver(receiver);
    msg->setTransactionId(transactionId);

    sprintf(msgname, "tic-%d-to-%d water-router-timeOutMsg", myIndex(), receiver);
    routerMsg *rMsg = new routerMsg(msgname);
    rMsg->setRoute(path);

    rMsg->setHopCount(0);
    rMsg->setMessageType(TIME_OUT_MSG);
    rMsg->encapsulate(msg);
    return rMsg;
}


/* generates the probe message for a particular destination and a particur path
 * identified by the list of hops and the path index
 */
routerMsg* hostNodeWaterfilling::generateProbeMessage(int destNode, int pathIdx, vector<int> path){
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

/* handle Waterfilling probe Message
 * if it back at the sender, then update the bottleneck balances for this path 
 * otherwise, reverse and send back to sender
 */
void hostNodeWaterfilling::handleProbeMessage(routerMsg* ttmsg){
    probeMsg *pMsg = check_and_cast<probeMsg *>(ttmsg->getEncapsulatedPacket());
    if (simTime()> _simulationLength ){
       ttmsg->decapsulate();
       delete pMsg;
       delete ttmsg;
       return;
    }

    bool isReversed = pMsg->getIsReversed();
    int nextDest = ttmsg->getRoute()[ttmsg->getHopCount()+1];
    
    if (isReversed == true) { 
        int pathIdx = pMsg->getPathIndex();
        int destNode = pMsg->getReceiver();
        vector<double> pathBalances = pMsg->getPathBalances();
        double bottleneck = minVectorElemDouble(pathBalances);

        PathInfo* p = &(nodeToShortestPathsMap[destNode][pathIdx]);
        assert(p->path == pMsg->getPath());
        
        // update state for this path -time of probe, balance
        p->lastUpdated = simTime();
        p->bottleneck = bottleneck;
        p->pathBalances = pathBalances;
        p>isProbeOutstanding = false;
        
        if (_signalsEnabled) 
            emit(nodeToShortestPathsMap[destNode][pathIdx].probeBackPerDestPerPathSignal,pathIdx);

        if (destNodeToNumTransPending[destNode] > 0){
           //reset the probe message to send again
           nodeToShortestPathsMap[destNode][pathIdx].isProbeOutstanding = true;
           vector<int> route = ttmsg->getRoute();
           reverse(route.begin(), route.end());
           vector<double> tempPathBal = {};

           ttmsg->setRoute(route);
           ttmsg->setHopCount(0);
           pMsg->setIsReversed(false); 
           pMsg->setPathBalances(tempPathBal);
           forwardProbeMessage(ttmsg);
        }
        else{
           ttmsg->decapsulate();
           delete pMsg;
           delete ttmsg;
        }
    }
    else {
        //reverse and send message again
        pMsg->setIsReversed(true);
        ttmsg->setHopCount(0);
        vector<int> route = ttmsg->getRoute();
        reverse(route.begin(), route.end());
        ttmsg->setRoute(route);
        forwardProbeMessage(ttmsg);
    }
}


/* initialize probes along the paths specified to the destination node
 * and set up all the state in the table that maintains bottleneck balance
 * information across all paths to all destinations
 * also responsible for initializing signals
 */
void hostNode::initializeProbes(vector<vector<int>> kShortestPaths, int destNode){ 
    destNodeToLastMeasurementTime[destNode] = 0.0;

    for (int pathIdx = 0; pathIdx < kShortestPaths.size(); pathIdx++){
        PathInfo temp = {};
        nodeToShortestPathsMap[destNode][pathIdx] = temp;
        nodeToShortestPathsMap[destNode][pathIdx].path = kShortestPaths[pathIdx];
        nodeToShortestPathsMap[destNode][pathIdx].probability = 1.0 / kShortestPaths.size();

        //initialize signals
        simsignal_t signal;
        signal = registerSignalPerDestPath("bottleneck", pathIdx, destNode);
        nodeToShortestPathsMap[destNode][pathIdx].bottleneckPerDestPerPathSignal = signal;

        signal = registerSignalPerDestPath("rateCompleted", pathIdx, destNode);
        nodeToShortestPathsMap[destNode][pathIdx].rateCompletedPerDestPerPathSignal = signal;

        signal = registerSignalPerDestPath("probability", pathIdx, destNode);
        nodeToShortestPathsMap[destNode][pathIdx].probabilityPerDestPerPathSignal = signal;

        signal = registerSignalPerDestPath("rateAttempted", pathIdx, destNode);
        nodeToShortestPathsMap[destNode][pathIdx].rateAttemptedPerDestPerPathSignal = signal;

        // generate a probe message on this path
        routerMsg * msg = generateProbeMessage(destNode, pathIdx, kShortestPaths[pathIdx]);
        nodeToShortestPathsMap[destNode][pathIdx].isProbeOutstanding = true;
        forwardProbeMessage(msg);
    }
}



/* restart waterfilling probes once they have been stopped to a particular destination
 * TODO: might end up leaving multiple probes in flight to some destinations, but that's okay 
 * for now.
 */
void hostNodeWaterfilling::restartProbes(int destNode) {
    for (auto p: nodeToShortestPathsMap[destNode] ){
        PathInfo pathInformation = p.second;
        nodeToShortestPathsMap[destNode][p.first].isProbeOutstanding = true;
        routerMsg * msg = generateProbeMessage(destNode, p.first, p.second.path);
        forwardProbeMessage(msg);
    }
}


/* forwards probe messages for waterfilling alone that appends the current balance
 * to the list of balances
 */
void hostNodeWaterfilling::forwardProbeMessage(routerMsg *msg){
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


/* core waterfilling logic in deciding how to split a transaction across paths
 * based on the bottleneck balances on each of those paths
 * For now, in hte absence of splitting, transaction is sent in entirety either
 * on the path with highest bottleneck balance or the paths are sampled with a
 * probabilities based on the bottleneck balances in the smooth waterfilling 
 * case
 */
void hostNode::splitTransactionForWaterfilling(routerMsg * ttmsg){
    transactionMsg *transMsg = check_and_cast<transactionMsg *>(ttmsg->getEncapsulatedPacket());
    int destNode = transMsg->getReceiver();
    double remainingAmt = transMsg->getAmount();
    
    // if you want to choose at random between paths
    bool randomChoice = false; 
    
    map<int, double> pathMap = {}; //key is pathIdx, double is amt
    vector<double> bottleneckList;
   
    // fill up priority queue of balances
    priority_queue<pair<double,int>> pq;
    if (_loggingEnabled) cout << "bottleneck for node " <<  getIndex();
    for (auto iter: nodeToShortestPathsMap[destNode] ){
        int key = iter.first;
        double bottleneck = (iter.second).bottleneck;
        double inflight = (iter.second).sumOfTransUnitsInFlight;
        bottleneckList.push_back(bottleneck);
        if (_loggingEnabled) cout << bottleneck << " (" << key  << "," 
            << iter.second.lastUpdated<<"), ";
        
        pq.push(make_pair(bottleneck - inflight, key)); 
    }
    if (_loggingEnabled) cout << endl;


    double highestBal;
    double secHighestBal;
    double diffToSend;
    double amtToSend;
    int highestBalIdx;
    int numPaths = nodeToShortestPathsMap[destNode].size();

    if (randomChoice) {
        vector<double> probabilities (numPaths, 1.0/numPaths);
        int pathIndex = sampleFromDistribution(probabilities);
        pathMap[pathIndex] = pathMap[pathIndex] + remainingAmt;
        remainingAmt = 0;
    } 
    else if (_smoothWaterfillingEnabled) {
        highestBal = get<0>(pq.top());
        if (highestBal >= 0) {
            int pathIndex = updatePathProbabilities(bottleneckList, destNode);
            pathMap[pathIndex] = pathMap[pathIndex] + remainingAmt;
            remainingAmt = 0;
        }
    }
    else {
        // normal waterfilling - start filling with the path
        // with highest bottleneck balance and fill it till you get to 
        // the next path and so on
        while(pq.size()>0 && remainingAmt > SMALLEST_INDIVISIBLE_UNIT){
            highestBal = get<0>(pq.top());
            highestBalIdx = get<1>(pq.top());
            pq.pop();
            
            if (pq.size()==0){
             secHighestBal=0;
            }
            else{
             secHighestBal = get<0>(pq.top());
            }
            diffToSend = highestBal - secHighestBal;

            amtToSend = min(remainingAmt/(pathMap.size()+1),diffToSend);

            for (auto p: pathMap){
                pathMap[p.first] = p.second + amtToSend;
                remainingAmt = remainingAmt - amtToSend;
            }
            pathMap[highestBalIdx] = amtToSend;
            remainingAmt = remainingAmt - amtToSend;
        }
   
        // send all of the remaining amount beyond the indivisible unit on one path
        // the highest bal path as long as it has non zero balance
        if (remainingAmt > 0 && pq.size()>0 ) {
            highestBal = get<0>(pq.top());
            highestBalIdx = get<1>(pq.top());
               
            if (highestBal > 0) {
                pathMap[highestBalIdx] = pathMap[highestBalIdx] + remainingAmt;
                remainingAmt = 0;
            }
        } 
        else {
            cout << "PATHS NOT FOUND to " << destNode << "WHEN IT SHOULD HAVE BEEN";
            throw std::exception();
        }
    }
    
    if (remainingAmt == 0) {
       statRateAttempted[destNode] = statRateAttempted[destNode] + 1;
    }
    transMsg->setAmount(remainingAmt);
    
    for (auto p: pathMap){
        int pathIndex = p.first;
        double amtOnPath = p.second;
        if (amtOnPath > 0){
            tuple<int,int> key = make_tuple(transMsg->getTransactionId(), pathIndex); 

            //update the data structure keeping track of how much sent and received on each path
            if (transPathToAckState.count(key) == 0){
                AckState temp = {};
                temp.amtSent = amtOnPath;
                temp.amtReceived = 0;
                transPathToAckState[key] = temp;
            }
            else {
                transPathToAckState[key].amtSent =  transPathToAckState[key].amtSent + amtOnPath;
            }
            
            PathInfo *pathInfo = &(nodeToShortestPathsMap[destNode][pathIndex]);
            routerMsg* waterMsg = generateWaterfillingTransactionMessage(amtOnPath, 
                 pathInfo->path, pathIndex, transMsg);
            
            if (_signalsEnabled) emit(pathPerTransPerDestSignals[destNode], pathIndex);
            
            // increment numAttempted per path
            pathInfo->statRateAttempted += 1;
            handleTransactionMessage(waterMsg);
            
            // incrementInFlight balance
            pathInfo->sumOfTransUnitsInFlight += p.second;
        }
    }
}


/* computes the updated path probabilities based on the current state of 
 * bottleneck link balances and returns the next path index to send the transaction 
 * on in accordance to the latest rate
 * acts as a helper for smooth waterfilling
 */
int hostNode::updatePathProbabilities(vector<double> bottleneckBalances, int destNode) {
    double averageBottleneck = accumulate(bottleneckBalances.begin(), 
            bottleneckBalances.end(), 0.0)/bottleneckBalances.size(); 
                
    double timeSinceLastTxn = simTime().dbl() - destNodeToLastMeasurementTime[destNode];
    destNodeToLastMeasurementTime[destNode] = simTime().dbl();

    // compute new porbabailities based on adjustment factor and expression
    vector<double> probabilities;
    int i = 0;
    for (auto b : bottleneckBalances) {
        probabilities.push_back(nodeToShortestPathsMap[destNode][i].probability + 
            (1 - exp(-1 * timeSinceLastTxn/_Tau))*(b - averageBottleneck)/_Normalizer);
        probabilities[i] = max(0.0, probabilities[i]);
        i++;
    }
    double sumProbabilities = accumulate(probabilities.begin(), probabilities.end(), 0.0); 
    
    // normalize them to 1 and update the stored probabilities
    for (i = 0; i < probabilities.size(); i++) {
        probabilities[i] /= sumProbabilities;
        nodeToShortestPathsMap[destNode][i].probability = probabilities[i];
    }
    return sampleFromDistribution(probabilities);
}
