#include "routerNode.h"

bool probesRecent(map<int, PathInfo> probes);

vector<string> split(string str, char delimiter);

vector<int> breadthFirstSearch(int sender, int receiver);

vector<int> breadthFirstSearchByGraph(int sender, int receiver, map<int, vector<int>> graph);

void generateChannelsBalancesMap(string);

void setNumNodes(string); //TODO: set numRouterNodes and numHostNodes

vector<int> dijkstra(int src, int dest);

void generateTransUnitList(string);

vector<int> getRoute(int sender, int receiver);

bool sortPriorityThenAmtFunction(const tuple<int,double, routerMsg*, Id> &a,
      const tuple<int,double, routerMsg*, Id> &b);

map<int, vector<pair<int,int>>> removeRoute( map<int, vector<pair<int,int>>> channels, vector<int> route);

vector<vector<int>> getKShortestRoutes(int sender, int receiver, int k);

vector<vector<int>> getKShortestRoutesLandmarkRouting(int sender, int receiver, int k);

vector<vector<int>> getKShortestRoutesLNDBaseline(int sender, int receiver, int k);

vector<int> dijkstraInputGraph(int src,  int dest, map<int, vector<pair<int,int>>> channels);

double minVectorElemDouble(vector<double> v);

double maxDouble(double x, double y);

void updateMaxTravelTime(vector<int> route);

void printChannels();

void printVector(vector<int> v);
