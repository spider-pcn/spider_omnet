#include "routerNodeBase.h"


//initialization functions
bool probesRecent(unordered_map<int, PathInfo> probes);
void generateChannelsBalancesMap(string);
void setNumNodes(string);
void updateMaxTravelTime(vector<int> route);
void updateCannonicalRTT(vector<int> route);

//get path functions
vector<int> getRoute(int sender, int receiver);
vector<vector<int>> getKShortestRoutes(int sender, int receiver, int k);
vector<vector<int>> getKShortestRoutesLandmarkRouting(int sender, int receiver, int k);
vector<vector<int>> getKPaths(int sender, int receiver, int k);
tuple<int, vector<int>> getNextPath(int sender, int receiver, int k);
void initializePathMaps(string filename);

//search functions
vector<int> breadthFirstSearch(int sender, int receiver);
vector<int> breadthFirstSearchByGraph(int sender, int receiver, unordered_map<int, set<int>> graph);
vector<int> dijkstra(int src, int dest);

void generateTransUnitList(string);
vector<string> split(string str, char delimiter);
vector<int> getRoute(int sender, int receiver);
double bottleneckCapacityOnPath(vector<int> thisPath);

// scheudling functions for the queue 
bool sortPriorityThenAmtFunction(const tuple<int,double, routerMsg*, Id, simtime_t> &a,
      const tuple<int,double, routerMsg*, Id, simtime_t> &b);
bool sortFIFO(const tuple<int,double, routerMsg*, Id, simtime_t> &a,
      const tuple<int,double, routerMsg*, Id, simtime_t> &b);
bool sortLIFO(const tuple<int,double, routerMsg*, Id, simtime_t> &a,
      const tuple<int,double, routerMsg*, Id, simtime_t> &b);
bool sortSPF(const tuple<int,double, routerMsg*, Id, simtime_t> &a,
      const tuple<int,double, routerMsg*, Id, simtime_t> &b);
bool sortEDF(const tuple<int,double, routerMsg*, Id, simtime_t> &a,
      const tuple<int,double, routerMsg*, Id, simtime_t> &b);

unordered_map<int, vector<pair<int,int>>> removeRoute( unordered_map<int, vector<pair<int,int>>> channels, vector<int> route);
vector<int> dijkstraInputGraph(int src,  int dest, unordered_map<int, vector<pair<int,int>>> channels);
double minVectorElemDouble(vector<double> v);
double maxDouble(double x, double y);

//logging functions
void printChannels();
void printVector(vector<int> v);
void printVectorReverse(vector<int> v);
void printChannels(unordered_map<int, vector<int>> channels);
int minInt(int x, int y);

double getTotalAmount(unordered_map<Id, double> v);
double getTotalAmount(vector<tuple<int, double, routerMsg*, Id, simtime_t >> queue);
