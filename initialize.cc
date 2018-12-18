#ifndef INITIALIZE_H
#define INITIALIZE_H
#include "initialize.h"

/*get_route- take in sender and receiver graph indicies, and returns
 *  BFS shortest path from sender to receiver in form of node indicies,
 *  includes sender and reciever as first and last entry
 */
vector<int> getRoute(int sender, int receiver){
   //do searching without regard for channel capacities, DFS right now

   printf("sender: %i; receiver: %i \n [", sender, receiver);
   //vector<int> route =  breadthFirstSearch(sender, receiver);
   vector<int> route = dijkstra(sender, receiver);

   return route;
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
   int min = INT_MAX, min_index;

   for (int v = 0; v < numNodes; v++)
      if (sptSet[v] == false &&
            dist[v] <= min)
         min = dist[v], min_index = v;

   return min_index;
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
   for (int i = 0; i < numNodes; i++)
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
}

// Function that implements Dijkstra's  single source shortest path algorithm for a graph represented
// using adjacency matrix representation
vector<int> dijkstra(int src,  int dest)
{

   // The output array. dist[i] will hold the shortest distance from src to i
   int dist[numNodes];

   // sptSet[i] will true if vertex i is included / in shortest path tree or shortest distance from src to i is finalized
   bool sptSet[numNodes];

   // Parent array to store shortest path tree
   int parent[numNodes];

   // Initialize all distances as INFINITE and stpSet[] as false
   for (int i = 0; i < numNodes; i++)
   {
      parent[src] = -1;
      dist[i] = INT_MAX;
      sptSet[i] = false;
   }

   // Distance of source vertex from itself is always 0
   dist[src] = 0;

   // Find shortest path for all vertices
   for (int count = 0; count < numNodes - 1; count++)
   {
      // Pick the minimum distance vertex from the set of vertices not yet processed.
      // u is always equal to src in first iteration.
      int u = minDistance(dist, sptSet);

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

   return getPath(parent, dest);

}




vector<int> breadthFirstSearch(int sender, int receiver){
   deque<vector<int>> nodesToVisit;
   bool visitedNodes[numNodes];
   for (int i=0; i<numNodes; i++){
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
      for (int i=0; i<(int)channels[lastNode].size();i++){

         if (!visitedNodes[channels[lastNode][i].first]){
            temp = current; // assignment copies in case of vector
            temp.push_back(channels[lastNode][i].first);
            nodesToVisit.push_back(temp);
            visitedNodes[channels[lastNode][i].first] = true;

            if (channels[lastNode][i].first==receiver){

               return temp;
            } //end if (channels[lastNode][i]==receiver)
         } //end if (!visitedNodes[channels[lastNode][i]])
      }//end for (i)
   }//end while
   vector<int> empty;
   return empty;
}


/* set_num_nodes -
 */
void setNumNodes(string topologyFile){
   int maxNode = -1;
   string line;
   ifstream myfile (topologyFile);
   if (myfile.is_open())
   {
      while ( getline (myfile,line) )
      {
         vector<string> data = split(line, ' ');
         //generate channels - adjacency map
         int node1 = stoi(data[0]); //
         int node2 = stoi(data[1]); //
         if (node1>maxNode){
            maxNode = node1;
         }
         if (node2>maxNode){
            maxNode = node2;
         }
      }
      myfile.close();
   }

   else cout << "Unable to open file";
   numNodes = maxNode + 1;
   return;
}


/* generate_channels_balances_map - reads from file and constructs adjacency list of graph topology (channels), and hash map
 *      of directed edges to initial balances, modifies global maps in place
 *      each line of file is of form
 *      [node1] [node2] [1->2 delay] [2->1 delay] [balance at node1 end] [balance at node2 end]
 */
void generateChannelsBalancesMap(string topologyFile, map<int, vector<pair<int,int>>> &channels, map<tuple<int,int>,double> &balances){
   string line;
   ifstream myfile (topologyFile);
   if (myfile.is_open())
   {
      while ( getline (myfile,line) )
      {
         vector<string> data = split(line, ' ');

         //generate channels - adjacency map
         int node1 = stoi(data[0]); //
         int node2 = stoi(data[1]); //
         int delay1to2 = stoi(data[2]);
         int delay2to1 = stoi(data[3]);
         if (channels.count(node1)==0){ //node 1 is not in map
            vector<pair<int,int>> tempVector = {};
            tempVector.push_back(make_pair(node2,delay1to2));
            channels[node1] = tempVector;
         }
         else{ //(node1 is in map)
            channels[node1].push_back(make_pair(node2,delay1to2));
         }

         if (channels.count(node2)==0){ //node 1 is not in map
            vector<pair<int,int>> tempVector = {make_pair(node1,delay2to1)};
            channels[node2] = tempVector;
         }
         else{ //(node1 is in map)
            channels[node2].push_back(make_pair(node1, delay2to1));
         }


         //generate balances map
         double balance1 = stod( data[4]);
         double balance2 = stod( data[5]);
         balances[make_tuple(node1,node2)] = balance1;
         balances[make_tuple(node2,node1)] = balance2;
      }
      myfile.close();
   }

   else cout << "Unable to open file";
   return;
}

/*
 *  generate_trans_unit_list - reads from file and generates global transaction unit job list.
 *      each line of file is of form:
 *      [amount] [timeSent] [sender] [receiver] [priorityClass]
 */
void generateTransUnitList(string workloadFile, vector<transUnit> &trans_unit_list){
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
            cout << "timeOut: " << timeOut << endl;
         }

         // instantiate all the transUnits that need to be sent
         transUnit tempTU = transUnit(amount, timeSent, sender, receiver, priorityClass, hasTimeOut, timeOut);

         // add all the transUnits into global list
         trans_unit_list.push_back(tempTU);

      }
      myfile.close();
   }

   else cout << "Unable to open file";
   return;

}

/*
 * sortFunction - helper function used to sort queued transUnit list by ascending priorityClass, then by
 *      ascending amount
 *      note: largest element gets accessed first
 */
bool sortPriorityThenAmtFunction(const tuple<int,double, routerMsg*> &a,
      const tuple<int,double, routerMsg*> &b)
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
