#include "routerNode.h"

//global parameters
vector<transUnit> trans_unit_list; //list of all transUnits
int numNodes;
//number of nodes in network
map<int, vector<pair<int,int>>> channels; //adjacency list format of graph edges of network
map<tuple<int,int>,double> balances;
//map of balances for each edge; key = <int,int> is <source, destination>
double statRate;
