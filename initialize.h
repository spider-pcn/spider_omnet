#include "routerNode.h"


vector<string> split(string str, char delimiter);
vector<int> breadthFirstSearch(int sender, int receiver);
void generate_channels_balances_map(string, map<int, vector<pair<int,int>>> &channels, map<tuple<int,int>,double> &balances);
void set_num_nodes(string);
vector<int> dijkstra(int src, int dest);
void generate_trans_unit_list(string, vector<transUnit> &trans_unit_list);
vector<int> get_route(int sender, int receiver);
bool sortFunction(const tuple<int,double, routerMsg*> &a,
      const tuple<int,double, routerMsg*> &b);
