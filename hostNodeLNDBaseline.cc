#include "hostNodeLNDBaseline.h"

Define_Module(hostNodeLNDBaseline);

bool sortPrunedChannelsFunction(tuple<simtime_t, tuple<int,int>> x, tuple<simtime_t, tuple<int, int>> y);

//instaniate global parameter for hostNodeLNDBaseline
double _restorePeriod;
int _numAttemptsLNDBaseline;

void hostNodeLNDBaseline::initializeMyChannels(){
    //not going to store delay, because using BFS to find shortest paths
    _myChannels = {};
    for (auto nodeIter: _channels){
        int node = nodeIter.first;
        _myChannels[node] = {};
        vector<pair<int, int>> edgeDelayVec = nodeIter.second;
        for (auto edgeIter: edgeDelayVec){
            int destNode = edgeIter.first;
            _myChannels[node].push_back(destNode);
        }
    }
}


vector<int>  hostNodeLNDBaseline::generateNextPath(int destNodePath){

    if (_prunedChannelsHeap.size() > 0){
        tuple<simtime_t, tuple<int, int>> currentEdge =  _prunedChannelsHeap.front();

        cout << "generateNextPath0" << endl;
        while (_prunedChannelsHeap.size()>0 && (get<0>(currentEdge) + _restorePeriod < simTime())){
            //add back to _myChannels
            int sourceNode = get<0>(get<1>(currentEdge));
            int destNode = get<1>(get<1>(currentEdge));
            _myChannels[sourceNode].push_back(destNode);

            //erase, and make sure to restore heap properties
            pop_heap(_prunedChannelsHeap.begin(), _prunedChannelsHeap.end(), sortPrunedChannelsFunction);
            _prunedChannelsHeap.pop_back();        
            if (_prunedChannelsHeap.size()>0)
                currentEdge =  _prunedChannelsHeap.front();
        }
    }
    
    vector<int> resultPath = breadthFirstSearchByGraph(myIndex(),destNodePath, _myChannels);
    return resultPath;       
}

void hostNodeLNDBaseline::pruneEdge(int sourceNode, int destNode){
    //prune edge if not already pruned.
    tuple<int, int> edgeTuple = make_tuple(sourceNode, destNode);        

    auto iter = find(_myChannels[sourceNode].begin(), _myChannels[sourceNode].end(), destNode); 
    if (iter != _myChannels[sourceNode].end() )
    {
        //prune edge and add to heap
        _myChannels[sourceNode].erase(iter);
        //vector<tuple<simtime_t, tuple<int, int>>> _prunedChannelsHeap;
        _prunedChannelsHeap.push_back(make_tuple(simTime(), make_tuple(sourceNode, destNode)));
        push_heap(_prunedChannelsHeap.begin(), _prunedChannelsHeap.end(), sortPrunedChannelsFunction);
    }
    else{ //if already pruned, update the simtime and "resort" the heap
        auto iterHeap = find_if(_prunedChannelsHeap.begin(),
                _prunedChannelsHeap.end(),
                [&edgeTuple](const tuple<simtime_t, tuple<int,int>>& p)
                { return ((get<0>(get<1>(p)) == get<0>(edgeTuple)) && (get<1>(get<1>(p)) == get<1>(edgeTuple))); });

        if (iterHeap != _prunedChannelsHeap.end()){
            _prunedChannelsHeap.erase(iterHeap);
            _prunedChannelsHeap.push_back(make_tuple(simTime(), make_tuple(sourceNode, destNode)));
            make_heap(_prunedChannelsHeap.begin(), _prunedChannelsHeap.end(), sortPrunedChannelsFunction);
            //make_heap again because of deletion    
        }
    }
}

bool sortPrunedChannelsFunction(tuple<simtime_t, tuple<int,int>> x, tuple<simtime_t, tuple<int, int>> y){
    simtime_t xTime = get<0>(x);
    simtime_t yTime = get<0>(y);
    return xTime>yTime; //makes smallest simtime_t appear first
} 




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

    _restorePeriod = 1.0;
    _numAttemptsLNDBaseline = 4;
    initializeMyChannels(); 

    //initialize heap for channels we pruned 
    _prunedChannelsHeap = {};
    make_heap(_prunedChannelsHeap.begin(), _prunedChannelsHeap.end(), sortPrunedChannelsFunction);
}

routerMsg *hostNodeLNDBaseline::generateAckMessage(routerMsg* ttmsg, bool isSuccess) {
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
    aMsg->encapsulate(transMsg);
    msg->encapsulate(aMsg);
    delete ttmsg;
    return msg;
}


/* main routine for handling a new transaction under lnd baseline 
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

       vector<int> newRoute = generateNextPath(destNode);
        transMsg->setPathIndex(-1);
        if (newRoute.size()>0)
        {
            printVector(newRoute);
            transMsg->setPathIndex(0);
            ttmsg->setRoute(newRoute);
            handleTransactionMessage(ttmsg);
        }
        else
        {
            routerMsg * failedAckMsg = generateAckMessage(ttmsg, false);
            handleAckMessageSpecialized(failedAckMsg);
        }
    }
    else{
        handleTransactionMessage(ttmsg);
    }
}


void hostNodeLNDBaseline::handleAckMessageSpecialized(routerMsg *msg)
{
    ackMsg *aMsg = check_and_cast<ackMsg *>(msg->getEncapsulatedPacket());
    transactionMsg *transMsg = check_and_cast<transactionMsg *>(aMsg->getEncapsulatedPacket());
    //guaranteed to be at last step of the path

    //get current index; increment it, see if there is a path for the incremented index
    int numPathsAttempted = aMsg->getPathIndex() + 1;
    if (aMsg->getIsSuccess() || numPathsAttempted == _numAttemptsLNDBaseline)
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
        if (msg->getRoute().size()>0)
        {        //prune edge

            int failedHopNum = aMsg->getFailedHopNum();
            int failedSource = msg->getRoute()[failedHopNum];
            int failedDest = msg->getRoute()[failedHopNum-1];
            pruneEdge(failedSource, failedDest);
            printChannels(_myChannels);

            char msgname[MSGSIZE];
            sprintf(msgname, "tic-%d-to-%d transactionMsg", transMsg->getSender(), transMsg->getReceiver());
            routerMsg* ttmsg = new routerMsg(msgname);
            transMsg->setPathIndex(newIndex);
            vector<int> newRoute = generateNextPath(transMsg->getReceiver());

            if (newRoute.size() == 0)
            {
                statNumFailed[aMsg->getReceiver()] = statNumFailed[aMsg->getReceiver()] + 1;
                statRateFailed[aMsg->getReceiver()] = statRateFailed[aMsg->getReceiver()] + 1;
                aMsg->decapsulate();
                delete transMsg;
                if (_signalsEnabled)
                    emit(pathPerTransPerDestSignals[aMsg->getReceiver()], numPathsAttempted);
                msg->decapsulate();
                delete aMsg;
                delete msg;
                return;
            }
            ttmsg->setRoute(newRoute);
            ttmsg->setHopCount(0);
            ttmsg->setMessageType(TRANSACTION_MSG);
            aMsg->decapsulate();
            hostNodeBase::handleAckMessage(msg);
            ttmsg->encapsulate(transMsg);
            handleTransactionMessage(ttmsg, true);
        }
        else
        {   statNumFailed[aMsg->getReceiver()] = statNumFailed[aMsg->getReceiver()] + 1;
            statRateFailed[aMsg->getReceiver()] = statRateFailed[aMsg->getReceiver()] + 1;
            aMsg->decapsulate();
            delete transMsg;
            if (_signalsEnabled)
                emit(pathPerTransPerDestSignals[aMsg->getReceiver()], numPathsAttempted);
            msg->decapsulate();
            delete aMsg;
            delete msg;
        }
    }

}

