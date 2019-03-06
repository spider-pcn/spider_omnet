#include "hostNodeLNDBaseline.h"

// set of landmarks for landmark routing

Define_Module(hostNodeLNDBaseline);

bool sortPrunedChannelsFunction(tuple<simtime_t, tuple<int,int>> x, tuple<simtime_t, tuple<int, int>> y);
 

//instaniate global parameter for hostNodeLNDBaseline
double _restorePeriod;
int _numAttemptsLNDBaseline;

void hostNodeLNDBaseline::initializeMyChannels(){
    //not going to store delay, because using BFS to find shortest paths
    for (auto nodeIter: _channels){
        _myChannels = {};
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
            
        while (_prunedChannelsHeap.size()>0 && get<0>(currentEdge) + _restorePeriod < simTime()){
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
    { //if already pruned, update the simtime and "resort" the heap
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
        else{
            cout << "pruned edge not found in _prunedChannelsHeap list" << endl;
            endSimulation();
        }
    }
    else{
        _myChannels[sourceNode].erase(iter);
        //vector<tuple<simtime_t, tuple<int, int>>> _prunedChannelsHeap;
        _prunedChannelsHeap.push_back(make_tuple(simTime(), make_tuple(sourceNode, destNode)));
        push_heap(_prunedChannelsHeap.begin(), _prunedChannelsHeap.end(), sortPrunedChannelsFunction);
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
    //initialize my channels
    initializeMyChannels(); //std objects do a deep copy

    //initialize heap for channels we pruned 
    _prunedChannelsHeap = {};
    make_heap(_prunedChannelsHeap.begin(), _prunedChannelsHeap.end(), sortPrunedChannelsFunction);
    //std::is_heap(begin(numbers), end(numbers))
    //make_heap(v1.begin(), v1.end()); 
    //cout << v1.front() << endl; 
    //push_heap()
    //pop_heap()

    /*
    // Initializing a vector 
    vector<int> v1 = {20, 30, 40, 25, 15}; 

     */

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
        /*
        if (nodeToShortestPathsMap.count(destNode) == 0 ){
            vector<vector<int>> kShortestRoutes = getKShortestRoutes(transMsg->getSender(), 
                    destNode, _kValue);
            initializePathInfoLNDBaseline(kShortestRoutes, destNode);
        }
        */
        vector<int> newRoute = generateNextPath(destNode);
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
        if (newIndex >= _numAttemptsLNDBaseline)
        { //no paths attempts left, fail the transaction, broadcast number of paths attempted
            aMsg->decapsulate();
            delete transMsg;
            if (_signalsEnabled)
                emit(pathPerTransPerDestSignals[aMsg->getReceiver()], numPathsAttempted);
            hostNodeBase::handleAckMessageSpecialized(msg);
        }
        else
        {
            char msgname[MSGSIZE];
            sprintf(msgname, "tic-%d-to-%d transactionMsg", transMsg->getSender(), transMsg->getReceiver());
            routerMsg* ttmsg = new routerMsg(msgname);
            transMsg->setPathIndex(newIndex);
            vector<int> newRoute = generateNextPath(transMsg->getReceiver());
            if (newRoute.size() == 0)
            {
                aMsg->decapsulate();
                delete transMsg;
                if (_signalsEnabled)
                    emit(pathPerTransPerDestSignals[aMsg->getReceiver()], numPathsAttempted);
                hostNodeBase::handleAckMessageSpecialized(msg);
                return;
            }
            ttmsg->setRoute(newRoute);
            cout << "new route for "<< transMsg->getSender() << " to "<< transMsg->getReceiver() << endl;
            printVector(ttmsg->getRoute());
            ttmsg->setHopCount(0);
            ttmsg->setMessageType(TRANSACTION_MSG);
            aMsg->decapsulate();

            hostNodeBase::handleAckMessage(msg);

            ttmsg->encapsulate(transMsg);
            handleTransactionMessage(ttmsg, true);
        }
    }
    return;
}

