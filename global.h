//global parameters
extern vector<TransUnit> _transUnitList;
extern int _numNodes;
//number of nodes in network
extern int _numRouterNodes;
extern int _numHostNodes;
extern map<int, vector<pair<int,int>>> _channels; //adjacency list format of graph edges of network
extern map<tuple<int,int>,double> _balances;
extern double _statRate;
//map of balances for each edge; key = <int,int> is <source, destination>
//extern bool withFailures;
extern bool _useWaterfilling;
extern int _kValue; //for k shortest paths
extern double _simulationLength;

