#include "routerNode.h"


//initialization functions
bool probesRecent(map<int, PathInfo> probes);
void generateChannelsBalancesMap(string);
void setNumNodes(string);
void generateTransUnitList(string);
void updateMaxTravelTime(vector<int> route);

//get path functions
vector<int> getRoute(int sender, int receiver);
vector<vector<int>> getKShortestRoutes(int sender, int receiver, int k);
vector<vector<int>> getKShortestRoutesLandmarkRouting(int sender, int receiver, int k);
vector<vector<int>> getKShortestRoutesLNDBaseline(int sender, int receiver, int k);

//search functions
vector<int> breadthFirstSearch(int sender, int receiver);
vector<int> breadthFirstSearchByGraph(int sender, int receiver, map<int, vector<int>> graph);
vector<int> dijkstra(int src, int dest);


//helper functions
vector<string> split(string str, char delimiter);
bool sortPriorityThenAmtFunction(const tuple<int,double, routerMsg*, Id> &a,
      const tuple<int,double, routerMsg*, Id> &b);
map<int, vector<pair<int,int>>> removeRoute( map<int, vector<pair<int,int>>> channels, vector<int> route);
vector<int> dijkstraInputGraph(int src,  int dest, map<int, vector<pair<int,int>>> channels);
double minVectorElemDouble(vector<double> v);
double maxDouble(double x, double y);

//logging functions
void printChannels();
void printVector(vector<int> v);
void printChannels(map<int, vector<int>> channels);
