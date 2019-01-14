#ifndef INITIALIZE_H
#define INITIALIZE_H
#include "hostInitialize.h"

double minVectorElemDouble(vector<double> v){
   double min = v[0];
   for (double d: v){
      if (d<min){
         min=d;
      }
   }
   return min;
}

double maxDouble(double x, double y){
    if (x>y) return x;
    return y;
}

bool probesRecent(map<int, PathInfo> probes){
   for (auto iter : probes){
      int key = iter.first;
      if ((iter.second).lastUpdated == -1  || ((simTime() - (iter.second).lastUpdated) > _maxTravelTime) ){
         return false;
      }

   }
   return true;
}

/*get_route- take in sender and receiver graph indicies, and returns
 *  BFS shortest path from sender to receiver in form of node indicies,
 *  includes sender and reciever as first and last entry
 */
vector<int> getRoute(int sender, int receiver){
   //do searching without regard for channel capacities, DFS right now


   // printf("sender: %i; receiver: %i \n [", sender, receiver);
   //vector<int> route =  breadthFirstSearch(sender, receiver);
   vector<int> route = dijkstraInputGraph(sender, receiver, _channels);
	 //Radhika TODO: seems like the dijkstra algorithm for the same sender 
	 //is being called again and again for different destinations. Can optimize. 


   /*
      for (int i=0; i<(int)route.size(); i++){
      printf("%i, ", route[i]);
      }
      printf("] \n");

    */

   return route;




   //return get_k_shortest_routes(sender, receiver, 2);
}



template <class T,class S> struct pair_equal_to : binary_function <T,pair<T,S>,bool> {
   bool operator() (const T& y, const pair<T,S>& x) const
   {
      return x.first==y;
   }
};

map<int, vector<pair<int,int>>> removeRoute( map<int, vector<pair<int,int>>> channels, vector<int> route){
   for (int i=0; i< (route.size() -1); i++){
    //  cout << i << "-th iteration out of "<< (route.size() - 1) << endl;
      int start = route[i];
      int end = route[i+1];

      //only erase if edge is between two router nodes
      if (start >= _numHostNodes && end >= _numHostNodes) {
        vector< pair <int, int> >::iterator it = find_if(channels[start].begin(),channels[start].end(),bind1st(pair_equal_to<int,int>(),end));
        channels[start].erase(it);
      }
   }

   return channels;
}



void updateMaxTravelTime(vector<int> route){



    double maxTime = 0;

    for (int i=0; i< ( route.size()-1) ; i++){

        //map<int, vector<pair<int,int>>> _channels;
        //Radhika TODO: might be better to store channel map indexed using both nodes. check if it breaks anything.
        vector<pair<int,int>>* channel = &(_channels[route[i]]);
        int nextNode = route[i+1];

        auto it = find_if( (*channel).begin(), (*channel).end(),
            [&nextNode](const pair<int, int>& element){ return element.first == nextNode;} );
        if (it != (*channel).end()){
            double deltaTime = it->second;
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

    return;
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

vector<vector<int>> getKShortestRoutes(int sender, int receiver, int k){
   //do searching without regard for channel capacities, DFS right now

   if (_loggingEnabled) {
       printf("sender: %i; receiver: %i \n ", sender, receiver);
       cout<<endl;
   }
   //vector<int> route =  breadthFirstSearch(sender, receiver);
   // print channels
   vector<vector<int>> shortestRoutes = {};
   vector<int> route;
   auto tempChannels = _channels;

   for (int it = 0; it<k; it++){

       /*
      printf("%d print of channels\n", it );
      for (auto i : tempChannels){
         printf("key: %d [",i.first);
         for (auto k: i.second){
            printf("(%d, %d) ",get<0>(k), get<1>(k));
         }

         printf("] \n");
      }
      cout<<endl;
    */

      route = dijkstraInputGraph(sender, receiver, tempChannels);

     
      if (route.size() <= 1){
         //cout << "Number of Routes between " << sender << " and " << receiver << " is " << shortestRoutes.size() << endl;
         return shortestRoutes;
      }
      else{
          updateMaxTravelTime(route);
         shortestRoutes.push_back(route);
      }
      if (_loggingEnabled) {
          cout << "getKShortestRoutes 1" <<endl;
          cout << "route size: " << route.size() << endl;
          cout << "getKShortestRoutes 2" <<endl;
      }

      // remove channels
      tempChannels = removeRoute(tempChannels,route);
   }
    
   
   if (_loggingEnabled)
       cout << "Number of Routes between " << sender << " and " << receiver << " is " << shortestRoutes.size() << endl;
   return shortestRoutes;
}


vector<string> split(string str, char delimiter){
   vector<string> internal;
   stringstream ss(str); // Turn the string into a stream.
   string tok;

   while(getline(ss, tok, delimiter)) {
      internal.push_back(tok);
   }
   return internal;
}


// A utility function to find the
// vertex with minimum distance
// value, from the set of vertices
// not yet included in shortest
// path tree
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

/*
   struct CompareFirst
   {
   CompareFirst(int val) : val_(val) {}
   bool operator()(const std::pair<int,char>& elem) const {
   return val_ == elem.first;
   }
   private:
   int val_;
   };
 */

vector<int> dijkstraInputGraph(int src,  int dest, map<int, vector<pair<int,int>>> channels){
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

   // print the constructed
   // distance array
   /*
   for (int ka=0; ka< _numNodes; ka++){
     printf("[%i]: %i,  ", ka, parent[ka] );
     }

    printSolution(dist,src,  parent);
    */

   //printf("\n");

   return getPath(parent, dest);


}

void dijkstraInputGraphTemp(int src,  int dest, map<int, vector<pair<int,int>>> channels){
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


         for (int ka=0; ka<_numNodes; ka++){
            printf("[%i]: %i,  ", ka, parent[ka] );

         }
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

   // print the constructed
   // distance array
   for (int ka=0; ka<_numNodes; ka++){
      printf("[%i]: %i,  ", ka, parent[ka] );

   }

   // printSolution(dist,src,  parent);

   //printf("\n");

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

   // print the constructed
   // distance array
   /*for (int ka=0; ka<numNodes; ka++){
     printf("[%i]: %i,  ", ka, parent[ka] );
     }*/

   // printSolution(dist,src,  parent);

   //printf("\n");

   return getPath(parent, dest);

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

            if (_channels[lastNode][i].first==receiver){

               return temp;
            } //end if (_channels[lastNode][i]==receiver)
         } //end if (!visitedNodes[_channels[lastNode][i]])
      }//end for (i)
   }//end while
   vector<int> empty;
   return empty;
}


/* set_num_nodes -
 */
void setNumNodes(string topologyFile){
   //TEMPORARILY hardcode
    /*
   _numNodes = 5;
   _numHostNodes = 2;
   _numRouterNodes = 3;
   return;
    */

   int maxHostNode = -1;
   int maxRouterNode = -1;
   string line;
   ifstream myfile (topologyFile);
   if (myfile.is_open())
   {
      while ( getline (myfile,line) )
      {
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

   else cout << "Unable to open file" << topologyFile << endl;
   _numHostNodes = maxHostNode + 1;
   _numRouterNodes = maxRouterNode + 1;

   _numNodes = _numHostNodes + _numRouterNodes;
   return;
}


/* generate_channels_balances_map - reads from file and constructs adjacency list of graph topology (channels), and hash map
 *      of directed edges to initial balances, modifies global maps in place
 *      each line of file is of form
 *      [node1] [node2] [1->2 delay] [2->1 delay] [balance at node1 end] [balance at node2 end]
 */
void generateChannelsBalancesMap(string topologyFile) {
   //TEMPORARILY - hardcode _channels map
   /*
    vector<pair<int,int>> tempVector0 = {};
   tempVector0.push_back(make_pair(2,30));
   _channels[0] = tempVector0;

   vector<pair<int,int>> tempVector1 = {};
   tempVector1.push_back(make_pair(4,30));
   _channels[1] = tempVector1;


   vector<pair<int,int>> tempVector2 = {};
   tempVector2.push_back(make_pair(0,30));
   tempVector2.push_back(make_pair(3,30));
   _channels[2] = tempVector2;


   vector<pair<int,int>> tempVector3 = {};
   tempVector3.push_back(make_pair(2,30));
   tempVector3.push_back(make_pair(4,30));
   _channels[3] = tempVector3;


   vector<pair<int,int>> tempVector4 = {};
   tempVector4.push_back(make_pair(3,30));
   tempVector4.push_back(make_pair(1,30));
   _channels[4] = tempVector4;

   //TEMPORARILY - hardcode balances map
   for (auto start: _channels){
      for (auto end: _channels[start.first]){

         _balances[make_tuple(start.first , get<0>(end))] = 100 ;
      }
   }
   return;
   */


   string line;
   ifstream myfile (topologyFile);
   if (myfile.is_open())
   {
      while ( getline (myfile,line) )
      {
         vector<string> data = split(line, ' ');

         //generate _channels - adjacency map
         char node1type = data[0].back();
         char node2type = data[1].back();

         if (_loggingEnabled) {
            cout <<"node2type: " << node2type << endl;
            cout <<"node1type: " << node1type << endl;
            cout << "data size" << data.size() << endl;
         }

         int node1 = stoi((data[0]).substr(0,data[0].size()-1)); //
         if (node1type == 'r'){
             node1 = node1 + _numHostNodes;
         }

         int node2 = stoi(data[1].substr(0,data[1].size()-1)); //
         if (node2type == 'r'){
                      node2 = node2 + _numHostNodes;
         }


         int delay1to2 = stoi(data[2]);
         int delay2to1 = stoi(data[3]);
         if (_channels.count(node1)==0){ //node 1 is not in map
            vector<pair<int,int>> tempVector = {};
            tempVector.push_back(make_pair(node2,delay1to2));
            _channels[node1] = tempVector;
         }
         else{ //(node1 is in map)
            _channels[node1].push_back(make_pair(node2,delay1to2));
         }

         if (_channels.count(node2)==0){ //node 1 is not in map
            vector<pair<int,int>> tempVector = {make_pair(node1,delay2to1)};
            _channels[node2] = tempVector;
         }
         else{ //(node1 is in map)
            _channels[node2].push_back(make_pair(node1, delay2to1));
         }


         //generate _balances map
         double balance1 = stod( data[4]);
         double balance2 = stod( data[5]);
         _balances[make_tuple(node1,node2)] = balance1;
         _balances[make_tuple(node2,node1)] = balance2;
      }
      myfile.close();
   }

   else cout << "Unable to open file " << topologyFile << endl;

   cout << "finished generateChannelsBalancesMap" << endl;
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
   if (myfile.is_open())
   {
      while ( getline (myfile,line) )
      {
         vector<string> data = split(line, ' ');

         //data[0] = amount, data[1] = timeSent, data[2] = sender, data[3] = receiver, data[4] = priority class; (data[5] = time out)
         double amount = stod(data[0]);
         double timeSent = stod(data[1]);
         int sender = stoi(data[2]);
         int receiver = stoi(data[3]);
         int priorityClass = stoi(data[4]);
         double timeOut=-1;
         double hasTimeOut = false;
         if (data.size()>5){
            hasTimeOut = true;
            timeOut = stoi(data[5]);
            //cout << "timeOut: " << timeOut << endl;
         }

         // instantiate all the transUnits that need to be sent
         TransUnit tempTU = TransUnit(amount, timeSent, sender, receiver, priorityClass, hasTimeOut, timeOut);

         // add all the transUnits into global per-sender map
         _transUnitList[sender].push_back(tempTU);

      }
      myfile.close();
   }

   else cout << "Unable to open file" << workloadFile << endl;
   return;

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

#endif
