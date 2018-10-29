#ifndef INITIALIZE_H
#define INITIALIZE_H
#include "initialize.h"





/*get_route- take in sender and receiver graph indicies, and returns
 *  BFS shortest path from sender to receiver in form of node indicies,
 *  includes sender and reciever as first and last entry
 */
vector<int> get_route(int sender, int receiver){
  //do searching without regard for channel capacities, DFS right now

  // printf("sender: %i; receiver: %i \n [", sender, receiver);
   vector<int> route =  breadthFirstSearch(sender, receiver);
/*
   for (int i=0; i<(int)route.size(); i++){
        printf("%i, ", route[i]);
    }
    printf("] \n");
*/
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

            if (!visitedNodes[channels[lastNode][i]]){
                temp = current; // assignment copies in case of vector
                temp.push_back(channels[lastNode][i]);
                nodesToVisit.push_back(temp);
                visitedNodes[channels[lastNode][i]] = true;

                if (channels[lastNode][i]==receiver){

                    return temp;
                } //end if (channels[lastNode][i]==receiver)
            } //end if (!visitedNodes[channels[lastNode][i]])
         }//end for (i)
      }//end while
   vector<int> empty;
   return empty;
}


/* generate_channels_balances_map - reads from file and constructs adjacency list of graph topology (channels), and hash map
 *      of directed edges to initial balances, modifies global maps in place
 *      each line of file is of form
 *      [node1] [node2] [1->2 delay] [2->1 delay] [balance at node1 end] [balance at node2 end]
 */
void generate_channels_balances_map(map<int, vector<int>> &channels, map<tuple<int,int>,double> &balances){
      string line;
      ifstream myfile ("sample-topology.txt");
      if (myfile.is_open())
      {
        while ( getline (myfile,line) )
        {
          vector<string> data = split(line, ' ');
           for (int i=0; i<data.size(); i++){
               cout << data[i] << "\n";

           }
          //generate channels - adjacency map
          int node1 = stoi(data[0]); //
          int node2 = stoi(data[1]); //
          if (channels.count(node1)==0){ //node 1 is not in map
              vector<int> tempVector = {node2};
              channels[node1] = tempVector;
          }
          else{ //(node1 is in map)
              if ( find(channels[node1].begin(), channels[node1].end(), node2) == channels[node1].end() ){ //(node2 is not already in node1's adjacency list')
                 channels[node1].push_back(node2);
              }
          }

          if (channels.count(node2)==0){ //node 1 is not in map
                      vector<int> tempVector = {node1};
                      channels[node2] = tempVector;
                  }
                  else{ //(node1 is in map)
                      if ( find(channels[node2].begin(), channels[node2].end(), node1) == channels[node2].end() ){ //(node2 is not already in node1's adjacency list')
                         channels[node2].push_back(node1);
                      }
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
void generate_trans_unit_list(vector<transUnit> &trans_unit_list){
          string line;
          ifstream myfile ("sample-workload.txt");
          if (myfile.is_open())
          {
            while ( getline (myfile,line) )
            {
              vector<string> data = split(line, ' ');
              for (int i=0; i< data.size(); i++){
                  cout<< data[i]<<endl;
              }
              //data[0] = amount, data[1] = timeSent, data[2] = sender, data[3] = receiver, data[4] = priority class
              double amount = stod(data[0]);
              double timeSent = stod(data[1]);
              int sender = stoi(data[2]);
              int receiver = stoi(data[3]);
              int priorityClass = stoi(data[4]);

              // instantiate all the transUnits that need to be sent
              transUnit tempTU = transUnit(amount, timeSent, sender, receiver, priorityClass);

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
 */
bool sortFunction(const tuple<int,double, routerMsg*> &a,
      const tuple<int,double, routerMsg*> &b)
{
   if (get<0>(a) < get<0>(b)){
      return true;
   }
   else if (get<0>(a) == get<0>(b)){
      return (get<1>(a) < get<1>(b));
   }
   return false;
}

#endif
