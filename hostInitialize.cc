#ifndef INITIALIZE_H
#define INITIALIZE_H
#include "hostInitialize.h"

bool probesRecent(unordered_map<int, PathInfo> probes){
    for (auto iter : probes){
        int key = iter.first;
        if ((iter.second).lastUpdated == -1  || ((simTime() - (iter.second).lastUpdated) > _maxTravelTime) )
            return false;
    }
    return true;
}


/* generate_channels_balances_map - reads from file and constructs adjacency list of graph topology (channels), and hash map
 *      of directed edges to initial balances, modifies global maps in place
 *      each line of file is of form
 *      [node1] [node2] [1->2 delay] [2->1 delay] [balance at node1 end] [balance at node2 end]
 */
void generateChannelsBalancesMap(string topologyFile) {
    string line;
    ifstream myfile (topologyFile);
    int lineNum = 0;
    int numEdges = 0;
    double sumDelays = 0.0;
    if (myfile.is_open())
    {
        while ( getline (myfile,line) )
        {
            lineNum++;
            vector<string> data = split(line, ' ');
            // parse all the landmarks from the first line
            if (lineNum == 1) {
                for (auto node : data) {
                    char nodeType = node.back();
                    int nodeNum = stoi((node).substr(0,node.size()-1)); 
                    if (nodeType == 'r') {
                        nodeNum = nodeNum + _numHostNodes;
                    }
                    _landmarks.push_back(nodeNum);
                    _landmarksWithConnectivityList.push_back(make_tuple(_channels[nodeNum].size(), nodeNum));
                }
                // don't do anything else
                continue;
            }
            //generate _channels - adjacency map
            char node1type = data[0].back();
            char node2type = data[1].back();

            if (_loggingEnabled) {
                cout <<"node2type: " << node2type << endl;
                cout <<"node1type: " << node1type << endl;
                cout << "data size" << data.size() << endl;
            }

            int node1 = stoi((data[0]).substr(0,data[0].size()-1)); //
            if (node1type == 'r')
                node1 = node1 + _numHostNodes;

            int node2 = stoi(data[1].substr(0,data[1].size()-1)); //
            if (node2type == 'r')
                node2 = node2 + _numHostNodes;

            int delay1to2 = stoi(data[2]);
            int delay2to1 = stoi(data[3]);
            if (_channels.count(node1)==0){ //node 1 is not in map
                vector<pair<int,int>> tempVector = {};
                tempVector.push_back(make_pair(node2,delay1to2));
                _channels[node1] = tempVector;
            }
            else //(node1 is in map)
                _channels[node1].push_back(make_pair(node2,delay1to2));

            if (_channels.count(node2)==0){ //node 1 is not in map
                vector<pair<int,int>> tempVector = {make_pair(node1,delay2to1)};
                _channels[node2] = tempVector;
            }
            else //(node2 is in map)
                _channels[node2].push_back(make_pair(node1, delay2to1));

            sumDelays += delay1to2 + delay2to1;
            numEdges += 2;
            //generate _balances map
            double balance1 = stod( data[4]);
            double balance2 = stod( data[5]);
            _balances[make_tuple(node1,node2)] = balance1;
            _balances[make_tuple(node2,node1)] = balance2;

            tuple<int, int> senderReceiverPair = (node1 < node2) ? make_tuple(node1, node2) :
                make_tuple(node2, node1);
            _capacities[senderReceiverPair] = balance1 + balance2;
            data = split(line, ' ');
        }

        myfile.close();
        _avgDelay = sumDelays/numEdges;
    }
    else 
        cout << "Unable to open file " << topologyFile << endl;

    cout << "finished generateChannelsBalancesMap whose size: " << _capacities.size() << endl;
    return;
}


/* set_num_nodes -
 */
void setNumNodes(string topologyFile){
    int maxHostNode = -1;
    int maxRouterNode = -1;
    string line;
    int lineNum = 0;
    ifstream myfile (topologyFile);
    if (myfile.is_open())
    {
        while ( getline (myfile,line) )
        {
            lineNum++;
            // skip landmark line
            if (lineNum == 1) {
                continue;
            }
            vector<string> data = split(line, ' ');
            //generate channels - adjacency map
            char node1type = data[0].back();
            //  cout <<"node1type: " << node1type << endl;
            char node2type = data[1].back();
            //   cout <<"node2type: " << node2type << endl;

            int node1 = stoi((data[0]).substr(0,data[0].size()-1)); //
            if (node1type == 'r' && node1 > maxRouterNode){
                maxRouterNode = node1;
                //node1 = node1+ _numHostNodes;
            }
            else if (node1type == 'e' && node1 > maxHostNode){
                maxHostNode = node1;
            }

            int node2 = stoi(data[1].substr(0,data[1].size()-1)); //
            if (node2type == 'r' && node2 > maxRouterNode){
                maxRouterNode = node2;
                //node2 = node2 + _numHostNodes;
            }
            else if (node2type == 'e' && node2 > maxHostNode){
                maxHostNode = node2;
            }
        }
        myfile.close();
    }
    else 
        cout << "Unable to open file" << topologyFile << endl;
    _numHostNodes = maxHostNode + 1;
    _numRouterNodes = maxRouterNode + 1;
    _numNodes = _numHostNodes + _numRouterNodes;
    return;
}

/*
 *  generate_trans_unit_list - reads from file and generates global transaction unit job list.
 *      each line of file is of form:
 *      [amount] [timeSent] [sender] [receiver] [priorityClass]
 */
//Radhika: do we need to pass global variables as arguments?
void generateTransUnitList(string workloadFile){
    string line;
    ifstream myfile (workloadFile);
    double lastTime = -1; 
    int lineNum = 0;
    if (myfile.is_open())
    {
        while ( getline (myfile,line) )
        {
            vector<string> data = split(line, ' ');
            lineNum++;

            //data[0] = amount, data[1] = timeSent, data[2] = sender, data[3] = receiver, data[4] = priority class; (data[5] = time out)
            double amount = stod(data[0]);
            double timeSent = stod(data[1]);
            int sender = stoi(data[2]);
            int receiver = stoi(data[3]);
            int priorityClass = stoi(data[4]);
            double timeOut=-1;
            double largerTxnID = lineNum;
            double hasTimeOut = _timeoutEnabled;
            
            if (timeSent >= _transStatStart && timeSent <= _transStatEnd) {
                if (_transactionArrivalBySize.count(amount) > 0) {
                    _transactionArrivalBySize[amount] += 1;
                }
                else {
                    _transactionCompletionBySize[amount] = 0;
                    _transactionArrivalBySize[amount] = 1;
                    _txnAvgCompTimeBySize[amount] = 0;
                }
            }

            if (data.size()>5 && _timeoutEnabled){
                timeOut = stoi(data[5]);
                //cout << "timeOut: " << timeOut << endl;
            }
            else if (_timeoutEnabled) {
                timeOut = 5.0;
            }

            if (_waterfillingEnabled) { 
                if (timeSent < _waterfillingStartTime || timeSent > _shortestPathEndTime) {
                    continue;
                 }
            }
            else if (_landmarkRoutingEnabled || _lndBaselineEnabled) { 
                if (timeSent < _landmarkRoutingStartTime || timeSent > _shortestPathEndTime) 
                    continue;
            }
            else if (!_priceSchemeEnabled && !_dctcpEnabled) {// shortest path
                if (timeSent < _shortestPathStartTime || timeSent > _shortestPathEndTime) 
                    continue;
            }
            
            if (timeSent > lastTime)
                 lastTime = timeSent;
            // instantiate all the transUnits that need to be sent
            int numSplits = 0;
            double totalAmount = amount;
            while (amount >= _splitSize && (_waterfillingEnabled || _priceSchemeEnabled || _lndBaselineEnabled || _dctcpEnabled || _celerEnabled)
                   && _splittingEnabled) {
                TransUnit tempTU = TransUnit(_splitSize, timeSent, 
                        sender, receiver, priorityClass, hasTimeOut, timeOut, largerTxnID);
                amount -= _splitSize;
                _transUnitList[sender].push(tempTU);
                numSplits++;
            }
            if (amount > 0) {
                TransUnit tempTU = TransUnit(amount, timeSent, sender, receiver, priorityClass, 
                        hasTimeOut, timeOut, largerTxnID);
                _transUnitList[sender].push(tempTU);
                numSplits++;
            }

            // push the transUnit into a priority queue indexed by the sender, 
            _destList[sender].insert(receiver);
            
            SplitState temp = {};
            temp.numTotal = numSplits;
            temp.numReceived = 0;
            temp.numArrived = 0;
            temp.numAttempted = 0;
            temp.totalAmount = totalAmount; 
            temp.firstAttemptTime = -1;
            _numSplits[sender][largerTxnID] = temp;

        }
        //cout << "finished generateTransUnitList" << endl;
        myfile.close();
        /*if (lastTime + 5 < _simulationLength) {
            cout << "Insufficient txns" << endl;
            assert(false);
        }*/
    }
    else 
        cout << "Unable to open file" << workloadFile << endl;
    return;
}


/* updateMaxTravelTime - calculate max travel time, called on each new route, and updates _maxTravelTime value
 */
void updateMaxTravelTime(vector<int> route){
    int nextNode;
    vector<pair<int,int>>* channel; 
    double maxTime = 0;

    for (int i=0; i< ( route.size()-1) ; i++){
        //find the propogation delay per edge of route
        //Radhika TODO: might be better to store channel map indexed using both nodes. check if it breaks anything.
        channel = &(_channels[route[i]]);
        nextNode = route[i+1];

        auto it = find_if( (*channel).begin(), (*channel).end(),
                [&nextNode](const pair<int, int>& element){ return element.first == nextNode;} );
        if (it != (*channel).end()){
            double deltaTime = it->second;
            if (deltaTime > _maxOneHopDelay)
                _maxOneHopDelay = deltaTime/1000;
            maxTime = maxTime + deltaTime;
        }
        else{
            cout << "ERROR IN updateMaxTravelTime, could not find edge" << endl;
            cout << "node to node " << route[i] << "," << route[i+1] << endl;
        }
    }
    maxTime = (maxTime)/1000 *2; //double for round trip, and convert from milliseconds to seconds
    if (maxTime > _maxTravelTime){
        _maxTravelTime = maxTime;
    }
    _delta = _maxTravelTime;
    return;
}


/*get_route- take in sender and receiver graph indicies, and returns
 *  BFS shortest path from sender to receiver in form of node indicies,
 *  includes sender and reciever as first and last entry
 */
vector<int> getRoute(int sender, int receiver){
    vector<int> route = dijkstraInputGraph(sender, receiver, _channels);
    updateMaxTravelTime(route);
    return route;
}

double bottleneckOnPath(vector<int> route) {
   double min = 10000000;
    // ignore end hosts
    for (int i = 1; i < route.size() - 2; i++) {
        double cap = _balances[make_tuple(i, i + 1)] + _balances[make_tuple(i+1, i)];
        if (cap < min)
            min = cap;
    }
    return min;
}

/* find the bottleneck "capacity" on the path
 * so that windows are not allowed to grow larger than them 
 */
double bottleneckCapacityOnPath(vector<int> route) {
   double min = 10000000;
    // ignore end hosts
    for (int i = 1; i < route.size() - 2; i++) {
        int thisNode = route[i];
        int nextNode = route[i + 1];
        tuple<int, int> senderReceiverTuple = (thisNode < nextNode) ? make_tuple(thisNode, nextNode) :
                make_tuple(nextNode, thisNode);
        double cap = _capacities[senderReceiverTuple];
        if (cap < min)
            min = cap;
    }
    return min;
}

void updateCannonicalRTT(vector<int> route) {
        // update cannonical RTT
        double sumRTT = (route.size() - 1) * 2 * _avgDelay / 1000.0;
        sumRTT += _cannonicalRTT * _totalPaths;
        _totalPaths += 1;
        _cannonicalRTT = sumRTT / _totalPaths;
}

vector<vector<int>> getKShortestRoutes(int sender, int receiver, int k){
    //do searching without regard for channel capacities, DFS right now
    if (_loggingEnabled) {
        printf("sender: %i; receiver: %i \n ", sender, receiver);
        cout<<endl;
    }
    vector<vector<int>> shortestRoutes = {};
    vector<int> route;
    auto tempChannels = _channels;
    for ( int it = 0; it < k; it++ ){
        route = dijkstraInputGraph(sender, receiver, tempChannels);
        
        if (route.size() <= 1){
            return shortestRoutes;
        }
        else{
            updateMaxTravelTime(route);
            updateCannonicalRTT(route);
            shortestRoutes.push_back(route);
        }
        if (_loggingEnabled) {
            cout << "getKShortestRoutes 1" <<endl;
            cout << "route size: " << route.size() << endl;
            cout << "getKShortestRoutes 2" <<endl;
        }
        tempChannels = removeRoute(tempChannels,route);
        cout << "Route Num " << it + 1 << " " << " ";
        printVector(route);

    }
    if (_loggingEnabled)
        cout << "Number of Routes between " << sender << " and " << receiver << " is " << shortestRoutes.size() << endl;
    return shortestRoutes;
}

void initializePathMaps(string filename) {
    string line;
    int lineNum = 0;
    ifstream myfile (filename);
    if (myfile.is_open())
    {
        int sender = -1;
        int receiver = -1;
        vector<vector<int>> pathList;
        while ( getline (myfile,line) ) 
        {
            vector<string> data = split(line, ' ');
            lineNum++;
            if (data[0].compare("pair") == 0) {
                if (lineNum > 1) {
                    _pathsMap[sender][receiver] = pathList;
                    // cout << data[0] <<  " " << data[1] << endl;
                }
                sender = stoi(data[1]);
                receiver = stoi(data[2]);
                pathList.clear();
                //cout << " sender " << sender << " receiver " << receiver << endl;
            }
            else {
                vector<int> newPath;
                for (string d : data) {
                    newPath.push_back(stoi(d));
                }
                pathList.push_back(newPath);
                if (_loggingEnabled) 
                    printVector(newPath);
            }
        }
        
        // push the last path in
        _pathsMap[sender][receiver] = pathList;
    }
    else {
        cout << "unable to open paths file " << filename << endl;
    }
}


vector<vector<int>> getKPaths(int sender, int receiver, int k) {
    if (!_widestPathsEnabled && !_kspYenEnabled && !_obliviousRoutingEnabled && !_heuristicPathsEnabled) 
        return getKShortestRoutes(sender, receiver, k);

    if (_pathsMap.empty()) {
        cout << "path Map uninitialized" << endl;
        throw std::exception();
    }

    if (_pathsMap.count(sender) == 0) {
        cout << " sender " << sender << " has no paths at all " << endl;
        throw std::exception();
    }

    if (_pathsMap[sender].count(receiver) == 0) {
        cout << " sender " << sender << " has no paths to receiver " << receiver << endl;
        throw std::exception();
    }
    
    vector<vector<int>> finalPaths;
    int numPaths = 0;
    double sumRTT = 0;
    for (vector<int> path : _pathsMap[sender][receiver]) {
        if (numPaths >= k)
            break;
        numPaths++;
        finalPaths.push_back(path);
        updateMaxTravelTime(path);
        updateCannonicalRTT(path);
    }

    return finalPaths;
}

// get the next path after the kth one when changing paths
tuple<int, vector<int>> getNextPath(int sender, int receiver, int k) {
    if (!_widestPathsEnabled && !_kspYenEnabled && !_obliviousRoutingEnabled && !_heuristicPathsEnabled) {
        cout << "no path Map" << endl;
        throw std::exception();
    }

    if (_pathsMap.empty()) {
        cout << "path Map uninitialized" << endl;
        throw std::exception();
    }

    if (_pathsMap.count(sender) == 0) {
        cout << " sender " << sender << " has no paths at all " << endl;
        throw std::exception();
    }

    if (_pathsMap[sender].count(receiver) == 0) {
        cout << " sender " << sender << " has no paths to receiver " << receiver << endl;
        throw std::exception();
    }
        
    if (_pathsMap[sender][receiver].size() >= k + 2) 
        return make_tuple(k + 1, _pathsMap[sender][receiver][k + 1]);
    else 
        return make_tuple(0, _pathsMap[sender][receiver][0]);
}

bool vectorContains(vector<int> smallVector, vector<vector<int>> bigVector) {
    for (auto v : bigVector) {
        if (v == smallVector)
            return true;
    }
    return false;
}

vector<vector<int>> getKShortestRoutesLandmarkRouting(int sender, int receiver, int k){
    int landmark;
    vector<int> pathSenderToLandmark;
    vector<int> pathLandmarkToReceiver;
    vector<vector<int>> kRoutes = {};
    int numPaths = minInt(_landmarksWithConnectivityList.size(), k);
    for (int i=0; i< numPaths; i++){
        landmark = get<1>(_landmarksWithConnectivityList[i]);
        pathSenderToLandmark = breadthFirstSearch(sender, landmark); //use breadth first search
        pathLandmarkToReceiver = breadthFirstSearch(landmark, receiver); //use breadth first search
			
	pathSenderToLandmark.insert(pathSenderToLandmark.end(), 
                pathLandmarkToReceiver.begin() + 1, pathLandmarkToReceiver.end());
        if ((pathSenderToLandmark.size() < 2 ||  pathLandmarkToReceiver.size() < 2 || 
                    vectorContains(pathSenderToLandmark, kRoutes))) { 
            if (numPaths < _landmarksWithConnectivityList.size()) {
                numPaths++;
            }
        } else {
            kRoutes.push_back(pathSenderToLandmark);
        } 
    }
    return kRoutes;
}



vector<int> breadthFirstSearchByGraph(int sender, int receiver, unordered_map<int, set<int>> graph){
    //TODO: fix, and add to header
    deque<vector<int>> nodesToVisit = {};
    bool visitedNodes[_numNodes];
    for (int i=0; i<_numNodes; i++){
        visitedNodes[i] =false;
    }
    visitedNodes[sender] = true;

    vector<int> temp;
    temp.push_back(sender);
    nodesToVisit.push_back(temp);

    while ((int) nodesToVisit.size() > 0){
        vector<int> current = nodesToVisit[0];
        nodesToVisit.pop_front();
        int lastNode = current.back();
        for (auto iter = graph[lastNode].begin(); iter != graph[lastNode].end(); iter++){
            int thisNode = *iter;
            if (!visitedNodes[thisNode]){
                temp = current; // assignment copies in case of vector
                temp.push_back(thisNode);
                nodesToVisit.push_back(temp);
                visitedNodes[thisNode] = true;
                if (thisNode == receiver)
                    return temp;
            } 
        }
    }
    vector<int> empty = {};
    return empty;
}


vector<int> breadthFirstSearch(int sender, int receiver){
    deque<vector<int>> nodesToVisit;
    bool visitedNodes[_numNodes];
    for (int i=0; i<_numNodes; i++){
        visitedNodes[i] =false;
    }
    visitedNodes[sender] = true;

    vector<int> temp;
    temp.push_back(sender);
    nodesToVisit.push_back(temp);

    while ((int)nodesToVisit.size()>0){
        vector<int> current = nodesToVisit[0];
        nodesToVisit.pop_front();
        int lastNode = current.back();
        for (int i=0; i<(int)_channels[lastNode].size();i++){
            if (!visitedNodes[_channels[lastNode][i].first]){
                temp = current; // assignment copies in case of vector
                temp.push_back(_channels[lastNode][i].first);
                nodesToVisit.push_back(temp);
                visitedNodes[_channels[lastNode][i].first] = true;
                if (_channels[lastNode][i].first==receiver)
                    return temp;
            } //end if (!visitedNodes[_channels[lastNode][i]])
        }//end for (i)
    }//end while
    vector<int> empty = {};
    return empty;
}

template <class T,class S> struct pair_equal_to : binary_function <T,pair<T,S>,bool> {
    bool operator() (const T& y, const pair<T,S>& x) const
    {
        return x.first==y;
    }
};

/* removeRoute - function used to remove paths found to get k shortest disjoint paths
 */
unordered_map<int, vector<pair<int,int>>> removeRoute( unordered_map<int, vector<pair<int,int>>> channels, vector<int> route){
    int start, end;
    for (int i=0; i< (route.size() -1); i++){
        start = route[i];
        end = route[i+1];
        //only erase if edge is between two router nodes
        if (start >= _numHostNodes && end >= _numHostNodes) {
            vector< pair <int, int> >::iterator it = find_if(channels[start].begin(),channels[start].end(),
                    bind1st(pair_equal_to<int,int>(),end));
            channels[start].erase(it);
        }
    }
    return channels;
}
int minInt(int x, int y){
    if (x< y) return x;
    return y;
}
/* split - same as split function in python, convert string into vector of strings using delimiter
 */
vector<string> split(string str, char delimiter){
    vector<string> internal;
    stringstream ss(str); // Turn the string into a stream.
    string tok;
    while(getline(ss, tok, delimiter)) {
        internal.push_back(tok);
    }
    return internal;
}


/*  A utility function to find the vertex with minimum distance
 * value, from the set of vertices not yet included in shortest path tree
 */
int minDistance(int dist[],
        bool sptSet[])
{

    // Initialize min value
    int min = INT_MAX;
    int min_index = -1;

    for (int v = 0; v < _numNodes; v++)
        if (sptSet[v] == false &&
                dist[v] <= min)
            min = dist[v], min_index = v;

    if (min == INT_MAX){
        return -1;
    }
    else{
        return min_index;
    }
}

// Function to print shortest
// path from source to j
// using parent array
void printPath(int parent[], int j)
{

    // Base Case : If j is source
    if (parent[j] == - 1)
        return;

    printPath(parent, parent[j]);

    printf("%d ", j);
}


vector<int> getPath(int parent[], int j)
{
    vector<int> result = {};
    // Base Case : If j is source
    if (parent[j] == - 1){
        result.push_back(j);
        return result;
    }
    else if (j == -2){
        vector<int> empty = {};
        return empty;

    }

    result = getPath(parent, parent[j]);
    result.push_back(j);
    return result;
}

// A utility function to print
// the constructed distance
// array
void printSolution(int dist[], int source,
        int parent[])
{
    int src = source;
    printf("Vertex\t Distance\tPath");
    for (int i = 0; i < _numNodes; i++)
    {
        printf("\n%d -> %d \t\t %d\t\t%d ",
                src, i, dist[i], src);
        printPath(parent, i);

        printf("\n getResultSolution: \n");
        vector<int> resultVect = getPath(parent, i);
        for (int i =0; i<resultVect.size(); i++){
            printf("%i, ", resultVect[i]);
        }
    }
    cout << "end print solution " << endl;
}

vector<int> dijkstraInputGraph(int src,  int dest, unordered_map<int, vector<pair<int,int>>> channels){
    // The output array. dist[i] will hold the shortest distance from src to i
    int dist[_numNodes];

    // sptSet[i] will true if vertex i is included / in shortest path tree or shortest distance from src to i is finalized
    bool sptSet[_numNodes];

    // Parent array to store shortest path tree
    int parent[_numNodes];

    // Initialize all distances as INFINITE and stpSet[] as false
    for (int i = 0; i < _numNodes; i++)
    {
        dist[i] = INT_MAX;
        parent[i] = -2;
        sptSet[i] = false;
    }

    // Parent of source is -1 (used for identifying source later) 
    parent[src] = -1;
    // Distance of source vertex from itself is always 0
    dist[src] = 0;

    // Find shortest path for all vertices
    for (int count = 0; count < _numNodes - 1; count++)
    {
        // Pick the minimum distance vertex from the set of vertices not yet processed.
        // u is always equal to src in first iteration.
        int u = minDistance(dist, sptSet);
        if (u == -1){
            vector<int> empty = {};
            return empty;
        }

        // Mark the picked vertex as processed
        sptSet[u] = true;

        vector<pair<int,int>>::iterator vectIter;
        // Update dist value of the adjacent vertices of the picked vertex.
        for (vectIter = channels[u].begin(); vectIter != channels[u].end(); vectIter++){

            // Update dist[v] only if is not in sptSet, there is an edge from u to v, and
            // total weight of path from src to v through u is smaller than current value of dist[v]
            if (!sptSet[vectIter->first]){
                if(dist[u] + (vectIter->second) < dist[vectIter->first]){
                    parent[vectIter->first] = u;
                    dist[vectIter->first] = dist[u] + vectIter->second;

                }
            }
        }
    }
    return getPath(parent, dest);
}

void dijkstraInputGraphTemp(int src,  int dest, unordered_map<int, vector<pair<int,int>>> channels){
    // The output array. dist[i] will hold the shortest distance from src to i
    int dist[_numNodes];

    // sptSet[i] will true if vertex i is included / in shortest path tree or shortest distance from src to i is finalized
    bool sptSet[_numNodes];

    // Parent array to store shortest path tree
    int parent[_numNodes];

    // Initialize all distances as INFINITE and stpSet[] as false
    for (int i = 0; i < _numNodes; i++)
    {
        parent[src] = -1;
        parent[i] = -2;
        dist[i] = INT_MAX;
        sptSet[i] = false;
    }

    // Distance of source vertex from itself is always 0
    dist[src] = 0;

    // Find shortest path for all vertices
    for (int count = 0; count < _numNodes - 1; count++)
    {
        // Pick the minimum distance vertex from the set of vertices not yet processed.
        // u is always equal to src in first iteration.
        int u = minDistance(dist, sptSet);
        if (u==-1){
            vector<int> empty = {};
            return ;

        }

        // Mark the picked vertex as processed
        sptSet[u] = true;

        vector<pair<int,int>>::iterator vectIter;
        // Update dist value of the adjacent vertices of the picked vertex.
        for (vectIter = channels[u].begin(); vectIter != channels[u].end(); vectIter++){

            /*for (int ka=0; ka<_numNodes; ka++){
                printf("[%i]: %i,  ", ka, parent[ka] );

            }*/
            // Update dist[v] only if is not in sptSet, there is an edge from u to v, and
            // total weight of path from src to v through u is smaller than current value of dist[v]

            // find first element with first == 42
            if (!sptSet[vectIter->first]){
                if(dist[u] + (vectIter->second) < dist[vectIter->first]){
                    parent[vectIter->first] = u;
                    dist[vectIter->first] = dist[u] + vectIter->second;

                }
            }
        }
    }

    // print the constructed
    // distance array
    /*for (int ka=0; ka<_numNodes; ka++)
        printf("[%i]: %i,  ", ka, parent[ka] );*/
    return;// getPath(parent, dest);
}
// Function that implements Dijkstra's  single source shortest path algorithm for a graph represented
// using adjacency matrix representation
vector<int> dijkstra(int src,  int dest)
{

    // The output array. dist[i] will hold the shortest distance from src to i
    int dist[_numNodes];

    // sptSet[i] will true if vertex i is included / in shortest path tree or shortest distance from src to i is finalized
    bool sptSet[_numNodes];

    // Parent array to store shortest path tree
    int parent[_numNodes];

    // Initialize all distances as INFINITE and stpSet[] as false
    for (int i = 0; i < _numNodes; i++)
    {
        parent[src] = -1;
        dist[i] = INT_MAX;
        sptSet[i] = false;
    }

    // Distance of source vertex from itself is always 0
    dist[src] = 0;

    // Find shortest path for all vertices
    for (int count = 0; count < _numNodes - 1; count++)
    {
        // Pick the minimum distance vertex from the set of vertices not yet processed.
        // u is always equal to src in first iteration.
        int u = minDistance(dist, sptSet);

        // Mark the picked vertex as processed
        sptSet[u] = true;

        vector<pair<int,int>>::iterator vectIter;
        // Update dist value of the adjacent vertices of the picked vertex.
        for (vectIter = _channels[u].begin(); vectIter != _channels[u].end(); vectIter++){

            // Update dist[v] only if is not in sptSet, there is an edge from u to v, and
            // total weight of path from src to v through u is smaller than current value of dist[v]

            // find first element with first == 42
            //= find_if(channels[u].begin(),channels[u].end(), CompareFirst(v));
            if (!sptSet[vectIter->first]){
                //if (vectIter != channels[u].end() ){
                if(dist[u] + (vectIter->second) < dist[vectIter->first]){
                    parent[vectIter->first] = u;
                    dist[vectIter->first] = dist[u] + vectIter->second;
                    //  }

            }
            }
        }
    }
    return getPath(parent, dest);
}

bool sortHighToLowConnectivity(tuple<int,int> x, tuple<int,int> y){
    if (get<0>(x) > get<0>(y)) 
        return true;
    else if (get<0>(x) < get<0>(y)) 
        return false;
    else
        return get<1>(x) < get<1>(y);
}

/*
 * sortFunction - helper function used to sort queued transUnit list by ascending priorityClass, then by
 *      ascending amount
 *      note: largest element gets accessed first
 */
bool sortPriorityThenAmtFunction(const tuple<int,double, routerMsg*, Id> &a,
        const tuple<int,double, routerMsg*, Id> &b)
{
    if (get<0>(a) < get<0>(b)){
        return false;
    }
    else if (get<0>(a) == get<0>(b)){
        return (get<1>(a) > get<1>(b));
    }
    return true;
}


double minVectorElemDouble(vector<double> v){
    double min = v[0];
    for (double d: v){
        if (d < min)
            min=d;
    }
    return min;
}



double maxDouble(double x, double y){
    if (x>y) return x;
    return y;
}


void printChannels(){
    printf("print of channels\n" );
    for (auto i : _channels){
        printf("key: %d [",i.first);
        for (auto k: i.second){
            printf("(%d, %d) ",get<0>(k), get<1>(k));
        }
        printf("] \n");
    }
    cout<<endl;
}


void printVector(vector<int> v){
    for (auto temp : v) {
        cout << temp << ", ";
    }
    cout << endl;
}

void printVectorReverse(vector<int> v){
    for (auto it = v.rbegin(); it != v.rend(); ++it) {
        cout << *it << ", ";
    }
    cout << endl;
}



/*
 * sortFunction - helper function used to sort queued transUnit list by ascending priorityClass, then by
 *      ascending amount
 *      note: largest element gets accessed first
 */
bool sortPriorityThenAmtFunction(const tuple<int,double, routerMsg*, Id, simtime_t> &a,
      const tuple<int,double, routerMsg*, Id, simtime_t> &b)
{
   if (get<0>(a) < get<0>(b)){
      return false;
   }
   else if (get<0>(a) == get<0>(b)){
      return (get<1>(a) > get<1>(b));
   }
   return true;
}



/*
 * sortFunction - to do FIFO sorting 
 */
bool sortFIFO(const tuple<int,double, routerMsg*, Id, simtime_t> &a,
      const tuple<int,double, routerMsg*, Id, simtime_t> &b)
{
    return (get<4>(a).dbl() < get<4>(b).dbl());
}

/*
 * sortFunction - to do LIFO sorting 
 */
bool sortLIFO(const tuple<int,double, routerMsg*, Id, simtime_t> &a,
      const tuple<int,double, routerMsg*, Id, simtime_t> &b)
{
    return (get<4>(a).dbl() > get<4>(b).dbl());
}

/*
 * sortFunction - to do shortest payment first sorting 
 */
bool sortSPF(const tuple<int,double, routerMsg*, Id, simtime_t> &a,
      const tuple<int,double, routerMsg*, Id, simtime_t> &b)
{
    transactionMsg *transA = check_and_cast<transactionMsg *>((get<2>(a))->getEncapsulatedPacket());
    transactionMsg *transB = check_and_cast<transactionMsg *>((get<2>(b))->getEncapsulatedPacket());
    
    SplitState splitInfoA = _numSplits[transA->getSender()][transA->getLargerTxnId()];
    SplitState splitInfoB = _numSplits[transB->getSender()][transB->getLargerTxnId()];

    if (splitInfoA.totalAmount != splitInfoB.totalAmount)
        return splitInfoA.totalAmount < splitInfoB.totalAmount;
    else
        return (get<4>(a).dbl() > get<4>(b).dbl());
}

/*
 * sortFunction - to do earliest deadline first sorting 
 */
bool sortEDF(const tuple<int,double, routerMsg*, Id, simtime_t> &a,
      const tuple<int,double, routerMsg*, Id, simtime_t> &b)
{
    transactionMsg *transA = check_and_cast<transactionMsg *>((get<2>(a))->getEncapsulatedPacket());
    transactionMsg *transB = check_and_cast<transactionMsg *>((get<2>(b))->getEncapsulatedPacket());
    
    return (transA->getTimeSent() < transB->getTimeSent());
}

#endif
