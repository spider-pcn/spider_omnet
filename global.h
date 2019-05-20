#include <stdio.h>
#include <string.h>
#include <omnetpp.h>
#include <vector>
#include <queue>
#include <stack>
#include "routerMsg_m.h"
#include "transactionMsg_m.h"
#include "ackMsg_m.h"
#include "updateMsg_m.h"
#include "timeOutMsg_m.h"
#include "probeMsg_m.h"
#include "priceUpdateMsg_m.h"
#include "priceQueryMsg_m.h"
#include "transactionSendMsg_m.h"
#include <iostream>
#include <algorithm>
#include <numeric>
#include <string>
#include <sstream>
#include <deque>
#include <map>
#include <fstream>
#include <list>
#include "structs/PaymentChannel.h"
#include "structs/PathInfo.h"
#include "structs/TransUnit.h"
#include "structs/CanceledTrans.h"
#include "structs/AckState.h"
#include "structs/SplitState.h"
#include "structs/DestInfo.h"
#include "structs/PathRateTuple.h"
#include "structs/ProbeInfo.h"

#define MSGSIZE 100
using namespace std;

struct LaterTransUnit
{
  bool operator()(const TransUnit& lhs, const TransUnit& rhs) const
  {
    return lhs.timeSent > rhs.timeSent;
  }
};

//global parameters
extern map<int, priority_queue<TransUnit, vector<TransUnit>, LaterTransUnit>> _transUnitList;
// extern map<int, deque<TransUnit>> _transUnitList;
extern map<int, set<int>> _destList;
extern map<int, map<double, SplitState>> _numSplits;
extern map<int, map<int, vector<vector<int>>>> _pathsMap;
extern int _numNodes;
//number of nodes in network
extern int _numRouterNodes;
extern int _numHostNodes;
extern map<int, vector<pair<int,int>>> _channels; //adjacency list format of graph edges of network
extern map<tuple<int,int>,double> _balances;
extern map<tuple<int,int>, double> _capacities;
extern double _statRate;
extern double _clearRate;
extern double _maxTravelTime;
//map of balances for each edge; key = <int,int> is <source, destination>
//extern bool withFailures;
extern bool _waterfillingEnabled;
extern bool _timeoutEnabled;
extern int _kValue; //for k shortest paths
extern double _simulationLength;
extern bool _landmarkRoutingEnabled;
extern bool _lndBaselineEnabled;
extern int _numAttemptsLNDBaseline;
//for lndbaseline
extern double _restorePeriod; 

extern bool _windowEnabled;

extern vector<tuple<int,int>> _landmarksWithConnectivityList;//pair: (number of edges, node number)
extern map<double, int> _transactionCompletionBySize;
extern map<double, int> _transactionArrivalBySize;

// for silentWhispers
extern vector<int> _landmarks;

//parameters for price scheme
extern bool _priceSchemeEnabled;
extern bool _splittingEnabled;

extern double _rho; // for nesterov computation
extern double _rhoLambda; // and accelerated Gradients
extern double _rhoMu;

extern double _eta; //for price computation
extern double _kappa; //for price computation
extern double _capacityFactor; //for price computation
extern bool _useQueueEquation;
extern double _tUpdate; //for triggering price updates at routers
extern double _tQuery; //for triggering price query probes
extern double _alpha; //parameter for rate updates
extern double _minPriceRate; // minimum rate to assign to all nodes when computing projections and such
extern double _delta; // round trip propagation delay
extern double _avgDelay;
extern double _xi; // how fast you want to drain the queue relative to network rtt - want this to be less than 1
extern double _routerQueueDrainTime;
extern int _serviceArrivalWindow;

// overall knobs
extern bool _signalsEnabled;
extern bool _loggingEnabled;
extern double _ewmaFactor;

// path choices knob
extern bool _widestPathsEnabled;
extern bool _kspYenEnabled;
extern bool _obliviousRoutingEnabled;

// queue knobs
extern bool _hasQueueCapacity;
extern int _queueCapacity;
extern double _epsilon;

// speeding up price scheme
extern bool _nesterov;
extern bool _secondOrderOptimization;

// experiments related parameters to control statistics
extern double _transStatStart;
extern double _transStatEnd;
extern double _waterfillingStartTime;
extern double _landmarkRoutingStartTime;
extern double _shortestPathStartTime;
extern double _shortestPathEndTime;
extern double _splitSize;

// rebalancing related parameters
extern bool _rebalancingEnabled;
extern double _rebalancingUpFactor;
extern double _queueDelayThreshold;
extern double _gamma;
extern double _maxGammaImbalanceQueueSize;

// DCTCP params
extern double _windowAlpha;
extern double _windowBeta;
extern double _qEcnThreshold;
