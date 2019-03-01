#include "hostNodeLNDBaseline.h"

// set of landmarks for landmark routing

Define_Module(hostNodeLNDBaseline);

/* initialization function to initialize parameters */
void hostNodeLNDBaseline::initialize(){
    hostNodeBase::initialize();
    //initialize WF specific signals with all other nodes in graph
    for (int i = 0; i < _numHostNodes; ++i) {
        //signal used for number of paths attempted per transaction per source-dest pair
        simsignal_t signal;
        signal = registerSignalPerDest("pathPerTrans", i, "");
        pathPerTransPerDestSignals.push_back(signal);
    }
}



/* main routine for handling a new transaction under lnd baseline 
 * In particular, initiate probes at sender and send acknowledgements
 * at the receiver
 */
void hostNodeLNDBaseline::handleTransactionMessageSpecialized(routerMsg* ttmsg){
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
            vector<vector<int>> kShortestRoutes = getKShortestRoutesLNDBaseline(transMsg->getSender(), 
                    destNode, _kValue);
            initializePathInfoLNDBaseline(kShortestRoutes, destNode);
        }
        transMsg->setPathIndex(-1);
        if (nodeToShortestPathsMap[destNode].size()>0)
        {
            transMsg->setPathIndex(0);
            ttmsg->setRoute(nodeToShortestPathsMap[destNode][0].path);
            handleTransactionMessage(ttmsg);
        }
        else
        {
            routerMsg * failedAckMsg = generateAckMessage(ttmsg, false);
            handleAckMessageSpecialized(failedAckMsg);
        }
    }
    else
        handleTransactionMessage(ttmsg);
}



/* initializes the table with the paths to use for LND Baseline, 
 * make sure that kShortest paths is sorted from shortest to longest paths
 * This function only helps for memoization
 */
void hostNodeLNDBaseline::initializePathInfoLNDBaseline(vector<vector<int>> kShortestPaths, 
        int  destNode){ 
    nodeToShortestPathsMap[destNode] = {};
    for (int pathIdx = 0; pathIdx < kShortestPaths.size(); pathIdx++){
        PathInfo temp = {};
        nodeToShortestPathsMap[destNode][pathIdx] = temp;
        nodeToShortestPathsMap[destNode][pathIdx].path = kShortestPaths[pathIdx];
    }
    return;
}


void hostNodeLNDBaseline::handleAckMessageSpecialized(routerMsg *msg)
{
    ackMsg *aMsg = check_and_cast<ackMsg *>(msg->getEncapsulatedPacket());
    cout << "LND Baseline before ack message encap trans cast" << endl;
    transactionMsg *transMsg = check_and_cast<transactionMsg *>(aMsg->getEncapsulatedPacket());
    cout << "LND Baseline after ack message encap trans cast" << endl;
    //guaranteed to be at last step of the path

    //get current index; increment it, see if there is a path for the incremented index
    int numPathsAttempted = aMsg->getPathIndex() + 1;
    if (aMsg->getIsSuccess())
    {
        aMsg->decapsulate();
        delete transMsg;
        if (_signalsEnabled)
            emit (pathPerTransPerDestSignals[aMsg->getReceiver()], numPathsAttempted);
        hostNodeBase::handleAckMessageSpecialized(msg);
    }
    else
    {
        int newIndex = aMsg->getPathIndex() + 1;
        if (newIndex >= nodeToShortestPathsMap[aMsg->getReceiver()].size())
        { //no paths left, fail the transaction, broadcast number of paths attempted
            aMsg->decapsulate();
            delete transMsg;
            if (_signalsEnabled)
                emit(pathPerTransPerDestSignals[aMsg->getReceiver()], numPathsAttempted);
            hostNodeBase::handleAckMessageSpecialized(msg);
        }
        else
        {
            transMsg->setPathIndex(newIndex);
            msg->setRoute(nodeToShortestPathsMap[aMsg->getReceiver()][newIndex].path);
            msg->setHopCount(0);
            msg->setMessageType(TRANSACTION_MSG);
            aMsg->decapsulate();
            msg->decapsulate();
            msg->encapsulate(transMsg);
            delete aMsg; //TODO: need to fix, some parts of handleAckMessageSpecialized needs to be implemented
                //Example: incrementing back funds in case of failure
                //TODO: keep the transaction message, and make a new router message?
            handleTransactionMessage(msg);
        }
    }
    return;
}

