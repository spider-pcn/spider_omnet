#include "routerNode.h"


vector<string> split(string str, char delimiter);
vector<int> breadthFirstSearch(int sender, int receiver);
void generateChannelsBalancesMap(string, map<int, vector<pair<int,int>>> &channels, map<tuple<int,int>,double> &balances);
void setNumNodes(string);
vector<int> dijkstra(int src, int dest);
void generateTransUnitList(string, vector<transUnit> &trans_unit_list);
vector<int> getRoute(int sender, int receiver);
bool sortPriorityThenAmtFunction(const tuple<int,double, routerMsg*> &a,
      const tuple<int,double, routerMsg*> &b);

