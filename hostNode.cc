#include "hostNode.h"
#include <queue>
#include "hostInitialize.h"

//global parameters
map<int, priority_queue<TransUnit, vector<TransUnit>, LaterTransUnit>> _transUnitList;
int _numNodes;
int _numRouterNodes;
int _numHostNodes;
double _maxTravelTime;
map<int, vector<pair<int,int>>> _channels; //adjacency list format of graph edges of network
map<tuple<int,int>,double> _balances;
//map of balances for each edge; key = <int,int> is <source, destination>
double _statRate;
double _clearRate;
int _kValue;
double _simulationLength;
bool _waterfillingEnabled;
bool _smoothWaterfillingEnabled;
bool _timeoutEnabled;
bool _loggingEnabled;
bool _signalsEnabled;
bool _landmarkRoutingEnabled;

vector<tuple<int,int>> _landmarksWithConnectivityList = {};

// set of landmarks for landmark routing
vector<int> _landmarks;

// set to 1 to report exact instantaneous balances
double _ewmaFactor;

// parameters for smooth waterfilling
double _Tau;
double _Normalizer;

//global parameters for price scheme
bool _priceSchemeEnabled;
double _eta; //for price computation
double _kappa; //for price computation
double _tUpdate; //for triggering price updates at routers
double _tQuery; //for triggering price query probes at hosts
double _alpha;
double _gamma;
double _zeta;
double _minPriceRate;

double _epsilon; // for all precision errors

//global parameters for fixed size queues
bool _hasQueueCapacity;
int _queueCapacity;

#define MSGSIZE 100
#define SMALLEST_INDIVISIBLE_UNIT 1

Define_Module(hostNode);

int hostNode::myIndex(){
   return getIndex();
}

void hostNode::deleteMessagesInQueues(){
   for (auto iter = nodeToPaymentChannel.begin(); iter!=nodeToPaymentChannel.end(); iter++){
      int key = iter->first;
      for (auto temp = (nodeToPaymentChannel[key].queuedTransUnits).begin();
            temp!= (nodeToPaymentChannel[key].queuedTransUnits).end(); ){
         routerMsg * rMsg = get<2>(*temp);
         auto tMsg = rMsg->getEncapsulatedPacket();
         rMsg->decapsulate();
         delete tMsg;
         delete rMsg;
         temp = (nodeToPaymentChannel[key].queuedTransUnits).erase(temp);
      }
   }

   // remove any waiting transactions too
    for (auto iter = nodeToDestInfo.begin(); iter!=nodeToDestInfo.end(); iter++){
      int dest = iter->first;
      while ((nodeToDestInfo[dest].transWaitingToBeSent).size() > 0) {
         routerMsg * rMsg = nodeToDestInfo[dest].transWaitingToBeSent.front();
         auto tMsg = rMsg->getEncapsulatedPacket();
         rMsg->decapsulate();
         delete tMsg;
         delete rMsg;
         nodeToDestInfo[dest].transWaitingToBeSent.pop_front();
      }
   }


   //check queue sizes after deletion:
   /*
   cout << "myIndex: " << myIndex();
   for (auto iter = nodeToPaymentChannel.begin(); iter!=nodeToPaymentChannel.end(); iter++){
        int key = iter->first;
        cout << " (" << key << ": size " << nodeToPaymentChannel[key].queuedTransUnits.size() << ")" ;
     }
   cout << endl;
    */
}


routerMsg * hostNode::generatePriceQueryMessage(vector<int> path, int pathIdx){
   int destNode = path[path.size()-1];
   char msgname[MSGSIZE];
   sprintf(msgname, "tic-%d-to-%d priceQueryMsg [idx %d]", myIndex(), destNode, pathIdx);
   priceQueryMsg *pqMsg = new priceQueryMsg(msgname);
   pqMsg->setPathIndex(pathIdx);
   pqMsg->setIsReversed(false);
   pqMsg->setZValue(0);

   sprintf(msgname, "tic-%d-to-%d router-priceQueryMsg [idx %d]", myIndex(), destNode, pathIdx);
   routerMsg *rMsg = new routerMsg(msgname);
   rMsg->setRoute(path);

   rMsg->setHopCount(0);
   rMsg->setMessageType(PRICE_QUERY_MSG);

   rMsg->encapsulate(pqMsg);
   return rMsg;
}

routerMsg * hostNode::generateProbeMessage(int destNode, int pathIdx, vector<int> path){
   char msgname[MSGSIZE];
   sprintf(msgname, "tic-%d-to-%d probeMsg [idx %d]", myIndex(), destNode, pathIdx);
   probeMsg *pMsg = new probeMsg(msgname);
   pMsg->setSender(myIndex());
   pMsg->setPathIndex(pathIdx);
   pMsg->setReceiver(destNode);
   pMsg->setIsReversed(false);
   vector<double> pathBalances;
   pMsg->setPathBalances(pathBalances);
   pMsg->setPath(path);

   sprintf(msgname, "tic-%d-to-%d router-probeMsg [idx %d]", myIndex(), destNode, pathIdx);
   routerMsg *rMsg = new routerMsg(msgname);
   rMsg->setRoute(path);

   rMsg->setHopCount(0);
   rMsg->setMessageType(PROBE_MSG);

   rMsg->encapsulate(pMsg);
   return rMsg;
}

bool hostNode:: printNodeToPaymentChannel(){
   bool invalidKey = false;
   printf("print of channels\n" );
   for (auto i : nodeToPaymentChannel){
      printf("(key: %d )",i.first);
      if (i.first<0) invalidKey = true;
   }
   cout<<endl;
   return !invalidKey;
}

void hostNode::forwardProbeMessage(routerMsg *msg){
   // Increment hop count.
   msg->setHopCount(msg->getHopCount()+1);
   //use hopCount to find next destination
   int nextDest = msg->getRoute()[msg->getHopCount()];
   probeMsg *pMsg = check_and_cast<probeMsg *>(msg->getEncapsulatedPacket());

   if (pMsg->getIsReversed() == false){
      vector<double> *pathBalances = & ( pMsg->getPathBalances());
      (*pathBalances).push_back(nodeToPaymentChannel[nextDest].balanceEWMA);
   }
   if (_loggingEnabled) cout << "forwardProbeMsg send:" << simTime() << endl;
   send(msg, nodeToPaymentChannel[nextDest].gate);
}

void printVectorDouble(vector<double> v){
   for (auto temp : v){
      cout << temp << ", ";
   }
   cout << endl;
}


void hostNode::handleProbeMessageLandmarkRouting(routerMsg* ttmsg){
   probeMsg *pMsg = check_and_cast<probeMsg *>(ttmsg->getEncapsulatedPacket());
   if (simTime()> _simulationLength ){
      ttmsg->decapsulate();
      delete pMsg;
      delete ttmsg;
      return;
   }

   bool isReversed = pMsg->getIsReversed();
   int nextDest = ttmsg->getRoute()[ttmsg->getHopCount()+1];
   if (isReversed == true){ //store times into private map, delete message



      int pathIdx = pMsg->getPathIndex();
      int destNode = pMsg->getReceiver();
      int transactionId = pMsg->getTransactionId();
      transactionIdToProbeInfoMap[transactionId].probeReturnTimes[pathIdx] = simTime();
      transactionIdToProbeInfoMap[transactionId].numProbesWaiting = transactionIdToProbeInfoMap[transactionId].numProbesWaiting- 1;
      double bottleneck = minVectorElemDouble(pMsg->getPathBalances());
      transactionIdToProbeInfoMap[transactionId].probeBottlenecks[pathIdx] = bottleneck;

      //check to see if all probes are back
      //cout << "transactionId: " << pMsg->getTransactionId();
      //cout << "numProbesWaiting: " <<  transactionIdToProbeInfoMap[transactionId].numProbesWaiting << endl;

      if (transactionIdToProbeInfoMap[transactionId].numProbesWaiting == 0){ //all back
          //forwardTransactionMessage(transactionIdToProbeInfoMap[transactionId].messageToSend);
          //set path for transaction using vibhaa's function
          int numPathsPossible = 0;
          for (auto bottleneck: transactionIdToProbeInfoMap[transactionId].probeBottlenecks){
              if (bottleneck > 0){
                  numPathsPossible++;
              }
          }

           //  virtual int sampleFromDistribution(vector<double> probabilities);


          vector<double> probabilities;
          //construct thing to sample from
          for (auto bottleneck: transactionIdToProbeInfoMap[transactionId].probeBottlenecks){
               if (bottleneck > 0){
                   probabilities.push_back(1/numPathsPossible);

                }
               else{
                   probabilities.push_back(0);
               }
           }

          int indexToUse = sampleFromDistribution(probabilities); //TODO: check that this returns the INDEX (so starts at 0)
          transactionMsg *transMsg = check_and_cast<transactionMsg *>(transactionIdToProbeInfoMap[transactionId].messageToSend->getEncapsulatedPacket());
          //cout << "sent transaction on path: ";
          //printVector(nodeToShortestPathsMap[transMsg->getReceiver()][indexToUse].path);

          transactionIdToProbeInfoMap[transactionId].messageToSend->setRoute(nodeToShortestPathsMap[transMsg->getReceiver()][indexToUse].path);
          handleTransactionMessage(transactionIdToProbeInfoMap[transactionId].messageToSend);

          //TODO: KATHY left off

          //call handleTransactionMessage() <--- will increment num attempted statistics appropriately


      }
      ttmsg->decapsulate();
           delete pMsg;
           delete ttmsg;

   }
   else{ //reverse and send message again
      pMsg->setIsReversed(true);
      ttmsg->setHopCount(0);
      vector<int> route = ttmsg->getRoute();
      reverse(route.begin(), route.end());
      ttmsg->setRoute(route);
      forwardProbeMessage(ttmsg);
   }
}



void hostNode::handleProbeMessage(routerMsg* ttmsg){
   probeMsg *pMsg = check_and_cast<probeMsg *>(ttmsg->getEncapsulatedPacket());
   if (simTime()> _simulationLength ){
      ttmsg->decapsulate();
      delete pMsg;
      delete ttmsg;
      return;
   }

   bool isReversed = pMsg->getIsReversed();
   int nextDest = ttmsg->getRoute()[ttmsg->getHopCount()+1];
   if (isReversed == true){ //store times into private map, delete message


      
      int pathIdx = pMsg->getPathIndex();
      int destNode = pMsg->getReceiver();
      nodeToShortestPathsMap[destNode][pathIdx].isProbeOutstanding = false;
      
      PathInfo * p = &(nodeToShortestPathsMap[destNode][pathIdx]);
      assert(p->path == pMsg->getPath());
      p->lastUpdated = simTime();
      vector<double> pathBalances = pMsg->getPathBalances();
      double bottleneck = minVectorElemDouble(pathBalances);
      p->bottleneck = bottleneck;
      p->pathBalances = pathBalances;
      
      if (_signalsEnabled) emit(nodeToShortestPathsMap[destNode][pathIdx].probeBackPerDestPerPathSignal,pathIdx);

      if (destNodeToNumTransPending[destNode] > 0){
         //reset the probe message
         vector<int> route = ttmsg->getRoute();
         reverse(route.begin(), route.end());
         ttmsg->setRoute(route);
         ttmsg->setHopCount(0);
         pMsg->setIsReversed(false);
         vector<double> tempPathBal = {};
         pMsg->setPathBalances(tempPathBal);
         nodeToShortestPathsMap[destNode][pathIdx].isProbeOutstanding = true;
         forwardProbeMessage(ttmsg);
      }
      else{
         ttmsg->decapsulate();
         delete pMsg;
         delete ttmsg;

      }
   }
   else{ //reverse and send message again
      pMsg->setIsReversed(true);
      ttmsg->setHopCount(0);
      vector<int> route = ttmsg->getRoute();
      reverse(route.begin(), route.end());
      ttmsg->setRoute(route);
      forwardProbeMessage(ttmsg);
   }
}

void hostNode:: restartProbes(int destNode){

   for (auto p: nodeToShortestPathsMap[destNode] ){
      PathInfo pathInformation = p.second;
      //if (pathInformation.isProbeOutstanding == false) {
        nodeToShortestPathsMap[destNode][p.first].isProbeOutstanding = true;
        routerMsg * msg = generateProbeMessage(destNode, p.first, p.second.path);
        forwardProbeMessage(msg);
      //}
   }

}

// check if all the provided rates are non-negative and also verify
// that their sum is less than the demand, return false otherwise
bool hostNode::ratesFeasible(vector<PathRateTuple> actualRates, double demand) {
    double sumRates = 0;
    for (auto a : actualRates) {
        double rate = get<1>(a);
        if (rate < (0 - _epsilon))
            return false;
        sumRates += rate;
    }
    return (sumRates <= (demand + _epsilon));
}

// computes the projection of the given recommended rates onto the demand d_ij per source
vector<PathRateTuple> hostNode::computeProjection(vector<PathRateTuple> recommendedRates, double demand) {
    auto compareRates = [](PathRateTuple rate1, PathRateTuple rate2) {
            return (get<1>(rate1) < get<1>(rate2));
    };

    auto nuFeasible = [](double nu, double nuLeft, double nuRight) {
            return (nu >= -_epsilon && nu >= (nuLeft - _epsilon)  && nu <= (nuRight + _epsilon));
    };


    sort(recommendedRates.begin(), recommendedRates.end(), compareRates);
    double nu = 0.0;
    vector<PathRateTuple> actualRates;

    // initialize all rates to zero
    vector<PathRateTuple> zeroRates;
    for (int i = 0; i < recommendedRates.size(); i++) {
        zeroRates.push_back(make_tuple(i, 0.0));
    }

    // if everything is negative (aka the largest is negative), just return 0s
    if (get<1>(recommendedRates[recommendedRates.size() - 1]) < 0 + _epsilon){
        return zeroRates;
    }

    // consider nu  = 0 and see if everything checks out
    actualRates = zeroRates;
    int firstPositiveIndex = -1;
    int i = 0;
    for (auto p : recommendedRates) {
        double rate = get<1>(p);
        int pathIndex = get<0>(p);
        if (rate > 0) {
            if (firstPositiveIndex == -1) 
                firstPositiveIndex = i;
            actualRates[pathIndex] = make_tuple(pathIndex, rate);
        }
        i++;
    }
    if (ratesFeasible(actualRates, demand))
        return actualRates;

    // atleast something should be positive if you got this far
    assert(firstPositiveIndex >= 0);


    // now go through all intervals between adjacent 2* recommended rates and see if any of them
    // can give you a valid assignment of actual rates and nu
    i = firstPositiveIndex; 
    double nuLeft = 0;
    double nuRight = 0;
    while (i < recommendedRates.size()) {
        // start over checking with a new nu interval
        actualRates = zeroRates;
        nuLeft = nuRight; 
        nuRight = 2*get<1>(recommendedRates[i]);

        // find sum of all elements that are to the right of nuRight
        double sumOfRightElements = 0.0;
        for (int j = i; j < recommendedRates.size(); j++)
            sumOfRightElements += get<1>(recommendedRates[j]);     
        nu = (sumOfRightElements - demand) * 2.0/(recommendedRates.size() - i);

        // given this nu, compute the actual rates for those elements to the right of nuRight
        for (auto p : recommendedRates) {
            double rate = get<1>(p);
            int pathIndex = get<0>(p);
            if (2*rate > nuLeft) {
                actualRates[pathIndex] = make_tuple(pathIndex, rate - nu/2.0);
            }
        }

        // check if these rates are feasible and nu actually falls in the right interval 
        if (ratesFeasible(actualRates, demand) && nuFeasible(nu, nuLeft, nuRight)) 
            return actualRates;

        // otherwise move on
        i++;
    }

    // should never be reached
    assert(false);
    return zeroRates;
}


void hostNode::initializeProbes(vector<vector<int>> kShortestPaths, int destNode){ 
    //maybe less than k routes
    destNodeToLastMeasurementTime[destNode] = 0.0;

   for (int pathIdx = 0; pathIdx < kShortestPaths.size(); pathIdx++){
      //map<int, map<int, PathInfo>> nodeToShortestPathsMap;
      //Radhika TODO: PathInfo can be a struct and need not be a class. 
      PathInfo temp = {};
      nodeToShortestPathsMap[destNode][pathIdx] = temp;
      nodeToShortestPathsMap[destNode][pathIdx].path = kShortestPaths[pathIdx];
      nodeToShortestPathsMap[destNode][pathIdx].probability = 1.0 / kShortestPaths.size();


      //initialize signals
      char signalName[64];
      simsignal_t signal;
      cProperty *statisticTemplate;

      // bottleneckPerDest signal
      if (destNode<_numHostNodes){
         sprintf(signalName, "bottleneckPerDestPerPath_%d(host %d)", pathIdx, destNode);
      }
      else{
         sprintf(signalName, "bottleneckPerDestPerPath_%d(router %d [%d] )", pathIdx, destNode - _numHostNodes, destNode);
      }
      signal = registerSignal(signalName);
      statisticTemplate = getProperties()->get("statisticTemplate", "bottleneckPerDestPerPathTemplate");
      getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
      nodeToShortestPathsMap[destNode][pathIdx].bottleneckPerDestPerPathSignal = signal;

      if (destNode<_numHostNodes){
         sprintf(signalName, "probeBackPerDestPerPath_%d(host %d)", pathIdx, destNode);
      }
      else{
         sprintf(signalName, "probeBackPerDestPerPath_%d(router %d [%d] )", pathIdx, destNode - _numHostNodes, destNode);
      }
      signal = registerSignal(signalName);
      statisticTemplate = getProperties()->get("statisticTemplate", "probeBackPerDestPerPathTemplate");
      getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
      nodeToShortestPathsMap[destNode][pathIdx].probeBackPerDestPerPathSignal = signal;

      if (destNode<_numHostNodes){
         sprintf(signalName, "rateCompletedPerDestPerPath_%d(host %d)", pathIdx, destNode);
      }
      else{
         sprintf(signalName, "rateCompletedPerDestPerPath_%d(router %d [%d] )", pathIdx, destNode - _numHostNodes, destNode);
      }

      signal = registerSignal(signalName);
      statisticTemplate = getProperties()->get("statisticTemplate", "rateCompletedPerDestPerPathTemplate");
      getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
      nodeToShortestPathsMap[destNode][pathIdx].rateCompletedPerDestPerPathSignal = signal;

      // probabilityPerDest signal
      if (destNode<_numHostNodes){
         sprintf(signalName, "probabilityPerDestPerPath_%d(host %d)", pathIdx, destNode);
      }
      else{
         sprintf(signalName, "probabilityPerDestPerPath_%d(router %d [%d] )", pathIdx, destNode - _numHostNodes, destNode);
      }
      signal = registerSignal(signalName);
      statisticTemplate = getProperties()->get("statisticTemplate", "probabilityPerDestPerPathTemplate");
      getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
      nodeToShortestPathsMap[destNode][pathIdx].probabilityPerDestPerPathSignal = signal;


      if (destNode<_numHostNodes){
         sprintf(signalName, "rateAttemptedPerDestPerPath_%d(host %d)", pathIdx, destNode);
      }
      else{
         sprintf(signalName, "rateAttemptedPerDestPerPath_%d(router %d [%d] )", pathIdx, destNode - _numHostNodes, destNode);
      }

      signal = registerSignal(signalName);
      statisticTemplate = getProperties()->get("statisticTemplate", "rateAttemptedPerDestPerPathTemplate");
      getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
      nodeToShortestPathsMap[destNode][pathIdx].rateAttemptedPerDestPerPathSignal = signal;

      routerMsg * msg = generateProbeMessage(destNode, pathIdx, kShortestPaths[pathIdx]);
      nodeToShortestPathsMap[destNode][pathIdx].isProbeOutstanding = true;
      forwardProbeMessage(msg);
   }
}



void hostNode::initializePriceProbes(vector<vector<int>> kShortestPaths, int destNode){ //maybe less than k routes
   for (int pathIdx = 0; pathIdx < kShortestPaths.size(); pathIdx++){
      PathInfo temp = {};
      nodeToShortestPathsMap[destNode][pathIdx] = temp;
      nodeToShortestPathsMap[destNode][pathIdx].path = kShortestPaths[pathIdx];
      routerMsg * triggerTransSendMsg = generateTriggerTransactionSendMessage(kShortestPaths[pathIdx], pathIdx, destNode);
      nodeToShortestPathsMap[destNode][pathIdx].triggerTransSendMsg = triggerTransSendMsg;
      nodeToShortestPathsMap[destNode][pathIdx].rateToSendTrans = _minPriceRate;

      //initialize signals
      char signalName[64];
      simsignal_t signal;
      cProperty *statisticTemplate;

      //rateCompletedPerDestPerPath signal
      if (destNode<_numHostNodes){
         sprintf(signalName, "rateCompletedPerDestPerPath_%d(host %d)", pathIdx, destNode);
      }
      else{
         sprintf(signalName, "rateCompletedPerDestPerPath_%d(router %d [%d] )", pathIdx, destNode - _numHostNodes, destNode);
      }
      signal = registerSignal(signalName);
      statisticTemplate = getProperties()->get("statisticTemplate", "rateCompletedPerDestPerPathTemplate");
      getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
      nodeToShortestPathsMap[destNode][pathIdx].rateCompletedPerDestPerPathSignal = signal;

      //rateAttemptedPerDestPerPath signal
      if (destNode<_numHostNodes){
         sprintf(signalName, "rateAttemptedPerDestPerPath_%d(host %d)", pathIdx, destNode);
      }
      else{
         sprintf(signalName, "rateAttemptedPerDestPerPath_%d(router %d [%d] )", pathIdx, destNode - _numHostNodes, destNode);
      }
      signal = registerSignal(signalName);
      statisticTemplate = getProperties()->get("statisticTemplate", "rateAttemptedPerDestPerPathTemplate");
      getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
      nodeToShortestPathsMap[destNode][pathIdx].rateAttemptedPerDestPerPathSignal = signal;

      //rateToSendTransPerPath signal
      if (destNode<_numHostNodes){
         sprintf(signalName, "rateToSendTransPerPath_%d(host %d)", pathIdx, destNode);
      }
      else{
         sprintf(signalName, "rateToSendTransPerPath_%d(router %d [%d] )", pathIdx, destNode - _numHostNodes, destNode);
      }
      signal = registerSignal(signalName);
      statisticTemplate = getProperties()->get("statisticTemplate", "rateToSendTransPerPathTemplate");
      getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
      nodeToShortestPathsMap[destNode][pathIdx].rateToSendTransSignal = signal;

      //timeToNextSendPerPath signal
      if (destNode<_numHostNodes){
         sprintf(signalName, "timeToNextSendPerPath_%d(host %d)", pathIdx, destNode);
      }
      else{
         sprintf(signalName, "timeToNextSendPerPath_%d(router %d [%d] )", pathIdx, destNode - _numHostNodes, destNode);
      }
      signal = registerSignal(signalName);
      statisticTemplate = getProperties()->get("statisticTemplate", "timeToNextSendPerPathTemplate");
      getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
      nodeToShortestPathsMap[destNode][pathIdx].timeToNextSendSignal = signal;

      //sumOfTransUnitsInFlightPerPath signal
      if (destNode<_numHostNodes){
         sprintf(signalName, "sumOfTransUnitsInFlightPerPath_%d(host %d)", pathIdx, destNode);
      }
      else{
         sprintf(signalName, "sumOfTransUnitsInFlightPerPath_%d(router %d [%d] )", pathIdx, destNode - _numHostNodes, destNode);
      }
      signal = registerSignal(signalName);
      statisticTemplate = getProperties()->get("statisticTemplate", "sumOfTransUnitsInFlightPerPathTemplate");
      getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
      nodeToShortestPathsMap[destNode][pathIdx].sumOfTransUnitsInFlightSignal = signal;
      
      //priceLastSeenPerPath signal
      if (destNode<_numHostNodes){
         sprintf(signalName, "priceLastSeenPerPath_%d(host %d)", pathIdx, destNode);
      }
      else{
         sprintf(signalName, "priceLastSeenPerPath_%d(router %d [%d] )", pathIdx, destNode - _numHostNodes, destNode);
      }
      signal = registerSignal(signalName);
      statisticTemplate = getProperties()->get("statisticTemplate", "priceLastSeenPerPathTemplate");
      getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
      nodeToShortestPathsMap[destNode][pathIdx].priceLastSeenSignal = signal;

      //isSendTimerSetPerPath signal
      if (destNode < _numHostNodes){
         sprintf(signalName, "isSendTimerSetPerPath_%d(host %d)", pathIdx, destNode);
      }
      else{
         sprintf(signalName, "isSendTimerSetPerPath_%d(router %d [%d] )", pathIdx, destNode - _numHostNodes, destNode);
      }
      signal = registerSignal(signalName);
      statisticTemplate = getProperties()->get("statisticTemplate", "isSendTimerSetPerPathTemplate");
      getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
      nodeToShortestPathsMap[destNode][pathIdx].isSendTimerSetSignal = signal;

   }
}

/* initialize() -
 */
void hostNode::initialize()
{
   string topologyFile_ = par("topologyFile");
   string workloadFile_ = par("workloadFile");

   completionTimeSignal = registerSignal("completionTime");

   successfulDoNotSendTimeOut = {};

   if (getIndex() == 0){  //main initialization for global parameters
      _simulationLength = par("simulationLength");
      _statRate = par("statRate");
      _clearRate = par("timeoutClearRate");
      _waterfillingEnabled = par("waterfillingEnabled");
      _smoothWaterfillingEnabled = par("smoothWaterfillingEnabled");
      _timeoutEnabled = par("timeoutEnabled");
      _signalsEnabled = par("signalsEnabled");
      _loggingEnabled = par("loggingEnabled");
      _priceSchemeEnabled = par("priceSchemeEnabled");

        _hasQueueCapacity = true;
      _queueCapacity = 0;
      
      _landmarkRoutingEnabled = false;
      if (_landmarkRoutingEnabled){
          _hasQueueCapacity = true;
          _queueCapacity = 0;
          _timeoutEnabled = false;
      }

      // price scheme parameters
      _eta = 0.02;
      _kappa = 0.02;
      _tUpdate = 0.8;
      _tQuery = 0.8;
      _alpha = 0.02;
      _gamma = 0.2; // ewma factor to compute per path rates
      _zeta = 0.001; // ewma for d_ij every source dest demand
      _minPriceRate = 1;

      _epsilon = pow(1, -6);
      
      // smooth waterfilling parameters
      _Tau = 10;
      _Normalizer = 100; // TODO: C from discussion with Mohammad)

      _ewmaFactor = 1; // EWMA factor for balance information on probes

      if (_waterfillingEnabled || _priceSchemeEnabled || _landmarkRoutingEnabled){
         _kValue = par("numPathChoices");
      }
      _maxTravelTime = 0.0; 
      setNumNodes(topologyFile_);
      // add all the TransUnits into global list
      generateTransUnitList(workloadFile_);

      //create "channels" map - from topology file
      //create "balances" map - from topology file
      generateChannelsBalancesMap(topologyFile_);
      /*
      cout << "landmarks: ";
      for (auto l: _landmarks)
          cout << l << ", "
      cout << endl;
      */
   }


   //Create nodeToPaymentChannel map
   const char * gateName = "out";
   cGate *destGate = NULL;

   int i = 0;
   int gateSize = gate(gateName, 0)->size();

   do {
      destGate = gate(gateName, i++);
      cGate *nextGate = destGate->getNextGate();
      if (nextGate ) {
         PaymentChannel temp =  {};
         temp.gate = destGate;

         bool isHost = nextGate->getOwnerModule()->par("isHost");
         int key = nextGate->getOwnerModule()->getIndex();
         if (!isHost){
            key = key + _numHostNodes;
         }
         nodeToPaymentChannel[key] = temp;
      }
   } while (i < gateSize);

   //initialize everything for adjacent nodes/nodes with payment channel to me
   for(auto iter = nodeToPaymentChannel.begin(); iter != nodeToPaymentChannel.end(); ++iter)
   {
      int key =  iter->first; //node

      //fill in balance field of nodeToPaymentChannel
      nodeToPaymentChannel[key].balance = _balances[make_tuple(myIndex(),key)];
      nodeToPaymentChannel[key].balanceEWMA = nodeToPaymentChannel[key].balance;

      // intialize capacity and other price variables
      double balanceOpp =  _balances[make_tuple(key, myIndex())];
      nodeToPaymentChannel[key].totalCapacity = nodeToPaymentChannel[key].balance + balanceOpp;
      nodeToPaymentChannel[key].lambda = 0;
      nodeToPaymentChannel[key].muLocal = 0;
      nodeToPaymentChannel[key].muRemote = 0;

      //initialize queuedTransUnits
      vector<tuple<int, double , routerMsg *, Id>> temp;
      make_heap(temp.begin(), temp.end(), sortPriorityThenAmtFunction);
      nodeToPaymentChannel[key].queuedTransUnits = temp;

      //register signals
      char signalName[64];
      simsignal_t signal;
      cProperty *statisticTemplate;

      //numInQueuePerChannel signal
      if (key<_numHostNodes) {
         sprintf(signalName, "numInQueuePerChannel(host %d)", key);
      }
      else{
         sprintf(signalName, "numInQueuePerChannel(router %d [%d] )", key - _numHostNodes, key);
      }

      signal = registerSignal(signalName);
      statisticTemplate = getProperties()->get("statisticTemplate", "numInQueuePerChannelTemplate");
      getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
      nodeToPaymentChannel[key].numInQueuePerChannelSignal = signal;

      //numProcessedPerChannel signal
      if (key<_numHostNodes) {
         sprintf(signalName, "numProcessedPerChannel(host %d)", key);
      }
      else {
         sprintf(signalName, "numProcessedPerChannel(router %d [%d])", key - _numHostNodes, key);
      }
      signal = registerSignal(signalName);
      statisticTemplate = getProperties()->get("statisticTemplate", "numProcessedPerChannelTemplate");
      getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
      nodeToPaymentChannel[key].numProcessedPerChannelSignal = signal;

      //statNumProcessed int
      nodeToPaymentChannel[key].statNumProcessed = 0;

      //balancePerChannel signal

      if (key<_numHostNodes) {
         sprintf(signalName, "balancePerChannel(host %d)", key);
      }
      else {
         sprintf(signalName, "balancePerChannel(router %d [%d])", key - _numHostNodes, key);
      }

      signal = registerSignal(signalName);
      statisticTemplate = getProperties()->get("statisticTemplate", "balancePerChannelTemplate");
      getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
      nodeToPaymentChannel[key].balancePerChannelSignal = signal;

      //numSentPerChannel signal
      if (key < _numHostNodes) {
         sprintf(signalName, "numSentPerChannel(host %d)", key);
      }
      else {
         sprintf(signalName, "numSentPerChannel(router %d [%d])", key-_numHostNodes, key);
      }
      signal = registerSignal(signalName);
      statisticTemplate = getProperties()->get("statisticTemplate", "numSentPerChannelTemplate");
      getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
      nodeToPaymentChannel[key].numSentPerChannelSignal = signal;

      //statNumSent int
      nodeToPaymentChannel[key].statNumSent = 0;

      if (_priceSchemeEnabled){
         if (key<_numHostNodes) {
            sprintf(signalName, "nValuePerChannel(host %d)", key);
         }
         else {
            sprintf(signalName, "nValuePerChannel(router %d [%d])", key - _numHostNodes, key);
         }
         signal = registerSignal(signalName);
         statisticTemplate = getProperties()->get("statisticTemplate", "nValuePerChannelTemplate");
         getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
         nodeToPaymentChannel[key].nValueSignal = signal;

         if (key<_numHostNodes) {
            sprintf(signalName, "xLocalPerChannel(host %d)", key);
         }
         else {
            sprintf(signalName, "xLocalPerChannel(router %d [%d])", key - _numHostNodes, key);
         }
         signal = registerSignal(signalName);
         statisticTemplate = getProperties()->get("statisticTemplate", "xLocalPerChannelTemplate");
         getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
         nodeToPaymentChannel[key].xLocalSignal = signal;

         if (key<_numHostNodes) {
            sprintf(signalName, "lambdaPerChannel(host %d)", key);
         }
         else {
            sprintf(signalName, "lambdaPerChannel(router %d [%d])", key - _numHostNodes, key);
         }
         signal = registerSignal(signalName);
         statisticTemplate = getProperties()->get("statisticTemplate", "lambdaPerChannelTemplate");
         getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
         nodeToPaymentChannel[key].lambdaSignal = signal;

         if (key<_numHostNodes) {
            sprintf(signalName, "muLocalPerChannel(host %d)", key);
         }
         else {
            sprintf(signalName, "muLocalPerChannel(router %d [%d])", key - _numHostNodes, key);
         }
         signal = registerSignal(signalName);
         statisticTemplate = getProperties()->get("statisticTemplate", "muLocalPerChannelTemplate");
         getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
         nodeToPaymentChannel[key].muLocalSignal = signal;

         if (key<_numHostNodes) {
            sprintf(signalName, "muRemotePerChannel(host %d)", key);
         }
         else {
            sprintf(signalName, "muRemotePerChannel(router %d [%d])", key - _numHostNodes, key);
         }
         signal = registerSignal(signalName);
         statisticTemplate = getProperties()->get("statisticTemplate", "muRemotePerChannelTemplate");
         getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
         nodeToPaymentChannel[key].muRemoteSignal = signal;
      }
   }

   //initialize signals with all other nodes in graph
   for (int i = 0; i < _numHostNodes; ++i) {
      char signalName[64];
      simsignal_t signal;
      cProperty *statisticTemplate;

      //rateCompletedPerDest signal
      sprintf(signalName, "rateCompletedPerDest_Total(host node %d)", i);
      signal = registerSignal(signalName);
      statisticTemplate = getProperties()->get("statisticTemplate", "rateCompletedPerDestTemplate");
      getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
      rateCompletedPerDestSignals.push_back(signal);

      statRateCompleted.push_back(0);

      //numCompletedPerDest signal
      sprintf(signalName, "numCompletedPerDest_Total(host node %d)", i);
      signal = registerSignal(signalName);
      statisticTemplate = getProperties()->get("statisticTemplate", "numCompletedPerDestTemplate");
      getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
      numCompletedPerDestSignals.push_back(signal);

      statNumCompleted.push_back(0);


      //rateAttemptedPerDest signal
      sprintf(signalName, "rateAttemptedPerDest_Total(host node %d)", i);
      signal = registerSignal(signalName);
      statisticTemplate = getProperties()->get("statisticTemplate", "rateAttemptedPerDestTemplate");
      getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
      rateAttemptedPerDestSignals.push_back(signal);
      statRateAttempted.push_back(0);
      
      //rateArrivedPerDest signal
      sprintf(signalName, "rateArrivedPerDest_Total(host node %d)", i);
      signal = registerSignal(signalName);
      statisticTemplate = getProperties()->get("statisticTemplate", "rateArrivedPerDestTemplate");
      getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
      rateArrivedPerDestSignals.push_back(signal);
      statRateArrived.push_back(0);

      //numAttemptedPerDest signal
      sprintf(signalName, "numArrivedPerDest_Total(host node %d)", i);
      signal = registerSignal(signalName);
      statisticTemplate = getProperties()->get("statisticTemplate", "numArrivedPerDestTemplate");
      getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
      numArrivedPerDestSignals.push_back(signal);

      statNumArrived.push_back(0);

      //rateFailedPerDest signal
      sprintf(signalName, "rateFailedPerDest(host node %d)", i);
      signal = registerSignal(signalName);
      statisticTemplate = getProperties()->get("statisticTemplate", "rateFailedPerDestTemplate");
      getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
      rateFailedPerDestSignals.push_back(signal);

      statRateFailed.push_back(0);

      //numFailedPerDest signal
      sprintf(signalName, "numFailedPerDest_Total(host node %d)", i);
      signal = registerSignal(signalName);
      statisticTemplate = getProperties()->get("statisticTemplate", "numFailedPerDestTemplate");
      getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
      numFailedPerDestSignals.push_back(signal);

      statNumFailed.push_back(0);

      //pathPerTransPerDest signal
      sprintf(signalName, "pathPerTransPerDest(host node %d)", i);
      signal = registerSignal(signalName);
      statisticTemplate = getProperties()->get("statisticTemplate", "pathPerTransPerDestTemplate");
      getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
      pathPerTransPerDestSignals.push_back(signal);

      //numTimedOutPerDest signal
      sprintf(signalName, "numTimedOutPerDest_Total(host node %d)", i);
      signal = registerSignal(signalName);
      statisticTemplate = getProperties()->get("statisticTemplate", "numTimedOutPerDestTemplate");
      getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
      numTimedOutPerDestSignals.push_back(signal);
      statNumTimedOut.push_back(0);

      // demandPerDest signal
      sprintf(signalName, "demandEstimatePerDest(host node %d)", i);
      signal = registerSignal(signalName);
      statisticTemplate = getProperties()->get("statisticTemplate", "demandEstimatePerDestTemplate");
      getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
      demandEstimatePerDestSignals.push_back(signal);

      //numPendingPerDest signal
      sprintf(signalName, "numPendingPerDest_total(host node %d)", i);
      signal = registerSignal(signalName);
      statisticTemplate = getProperties()->get("statisticTemplate", "numPendingPerDestTemplate");
      getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
      numPendingPerDestSignals.push_back(signal);

      //numWaitingInSenderQueue signal
      sprintf(signalName, "numWaitingPerDest_total(host node %d)", i);
      signal = registerSignal(signalName);
      statisticTemplate = getProperties()->get("statisticTemplate", "numWaitingPerDestTemplate");
      getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
      numWaitingPerDestSignals.push_back(signal);

      //numtimedoutperdestatsender  signal
      sprintf(signalName, "numTimedOutAtSenderPerDest(host node %d)", i);
      signal = registerSignal(signalName);
      statisticTemplate = getProperties()->get("statisticTemplate", 
              "numTimedOutAtSenderPerDestTemplate");
      getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
      numTimedOutAtSenderSignals.push_back(signal);
      statNumTimedOutAtSender.push_back(0);

      //fracSuccessfulPerDest signal
      sprintf(signalName, "fracSuccessfulPerDest_Total(host node %d)", i);
      signal = registerSignal(signalName);
      statisticTemplate = getProperties()->get("statisticTemplate", "fracSuccessfulPerDestTemplate");
      getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
      fracSuccessfulPerDestSignals.push_back(signal);
   }

    // generate first transaction
   generateNextTransaction();
   //cout << "[host id] " << myIndex() << ": finished creating trans messages." << endl;

   //get stat message
   routerMsg *statMsg = generateStatMessage();
   scheduleAt(simTime()+0, statMsg);

   if (_timeoutEnabled){
      routerMsg *clearStateMsg = generateClearStateMessage();
      scheduleAt(simTime()+_clearRate, clearStateMsg);
   }

   if (_priceSchemeEnabled){
      routerMsg *triggerPriceUpdateMsg = generateTriggerPriceUpdateMessage();
      scheduleAt(simTime() + _tUpdate, triggerPriceUpdateMsg );

      routerMsg *triggerPriceQueryMsg = generateTriggerPriceQueryMessage();
      scheduleAt(simTime() + _tQuery, triggerPriceQueryMsg );
   }
}

void hostNode::generateNextTransaction() {
      if (_transUnitList[myIndex()].empty())
          return;
      TransUnit j = _transUnitList[myIndex()].top();

      double timeSent = j.timeSent;

      // cout << timeSent << " " << j.sender << " " << j.receiver << endl;

      routerMsg *msg = generateTransactionMessage(j); //TODO: flag to whether to calculate path

      if (_waterfillingEnabled || _priceSchemeEnabled || _landmarkRoutingEnabled){
         vector<int> blankPath = {};
         //Radhika TODO: maybe no need to compute path to begin with?
         msg->setRoute(blankPath);
      }

      scheduleAt(timeSent, msg);

      if (_timeoutEnabled && !_priceSchemeEnabled && j.hasTimeOut){
         routerMsg *toutMsg = generateTimeOutMessage(msg);
         scheduleAt(timeSent + j.timeOut, toutMsg );
      }
      _transUnitList[myIndex()].pop();
}

routerMsg *hostNode::generateTriggerPriceQueryMessage(){
   char msgname[MSGSIZE];
   sprintf(msgname, "node-%d triggerPriceQueryMsg", myIndex());
   routerMsg *rMsg = new routerMsg(msgname);
   rMsg->setMessageType(TRIGGER_PRICE_QUERY_MSG);
   return rMsg;
}

routerMsg *hostNode::generateTriggerPriceUpdateMessage(){
   char msgname[MSGSIZE];
   sprintf(msgname, "node-%d triggerPriceUpdateMsg", myIndex());
   routerMsg *rMsg = new routerMsg(msgname);
   rMsg->setMessageType(TRIGGER_PRICE_UPDATE_MSG);
   return rMsg;
}


void hostNode::handleMessage(cMessage *msg)
{
   bool toPrint = false;
   //cout << "Host Time:" << simTime() << endl;
   //cout << "msg: " << msg->getName() << endl;
   routerMsg *ttmsg = check_and_cast<routerMsg *>(msg);
   
   //Radhika TODO: figure out what's happening here.
   if (simTime() > _simulationLength){
       auto encapMsg = (ttmsg->getEncapsulatedPacket());
          ttmsg->decapsulate();
          delete ttmsg;
          delete encapMsg;
          return;


      int splitTrans = 0;
      for (auto p: transactionIdToNumHtlc){
         if (p.second>1) splitTrans++;
      }
      if (_loggingEnabled) {
         cout << endl;
         cout << "number greater than 1: " << splitTrans << endl;
      }
      cout << "maxTravelTime:" << _maxTravelTime << endl;
      //endSimulation();


   }

   if (ttmsg->getMessageType()==ACK_MSG){ //preprocessor constants defined in routerMsg_m.h
      if (_loggingEnabled) cout << "[HOST "<< myIndex() <<": RECEIVED ACK MSG] " << msg->getName() << endl;
      if (_timeoutEnabled){
         if(_loggingEnabled) cout << "handleAckMessageTimeOut" << endl;
         handleAckMessageTimeOut(ttmsg);
      }
      if (_waterfillingEnabled){
         handleAckMessageWaterfilling(ttmsg);
      }
      else if (_priceSchemeEnabled){ 
         handleAckMessagePriceScheme(ttmsg);
      }
      else{
         if(_loggingEnabled) cout << "handleAckMessageShortestPath" << endl;
         handleAckMessageShortestPath(ttmsg);
      }
      handleAckMessage(ttmsg);
      if (_loggingEnabled) cout << "[AFTER HANDLING:]" <<endl;
   }
   else if(ttmsg->getMessageType()==TRANSACTION_MSG){
      if (_loggingEnabled) cout<< "[HOST "<< myIndex() <<": RECEIVED TRANSACTION MSG]  "<< msg->getName() <<endl;

      transactionMsg *transMsg = check_and_cast<transactionMsg *>(ttmsg->getEncapsulatedPacket());
      if (transMsg->isSelfMessage() && simTime() == transMsg->getTimeSent()) {
          // new transaction so generate the next one
          generateNextTransaction();
      }

      if (_timeoutEnabled && handleTransactionMessageTimeOut(ttmsg)){
         return;
      }

      if (_waterfillingEnabled && (ttmsg->getRoute().size() == 0)){
         handleTransactionMessageWaterfilling(ttmsg);
      }
      else if (_landmarkRoutingEnabled){
          handleTransactionMessageLandmarkRouting(ttmsg);
      }
      else if (_priceSchemeEnabled){
         handleTransactionMessagePriceScheme(ttmsg);
      }
      else{

         handleTransactionMessage(ttmsg);
      }

      if (_loggingEnabled) cout<< "[AFTER HANDLING:] "<<endl;
   }
   else if(ttmsg->getMessageType()==UPDATE_MSG){
      if (_loggingEnabled) cout<< "[HOST "<< myIndex() <<": RECEIVED UPDATE MSG] "<< msg->getName() << endl;
      handleUpdateMessage(ttmsg);
      if (_loggingEnabled) cout<< "[AFTER HANDLING:]  "<<endl;
   }
   else if (ttmsg->getMessageType() ==STAT_MSG){
      if (_loggingEnabled) cout<< "[HOST "<< myIndex() <<": RECEIVED STAT MSG] "<< msg->getName() << endl;
      if (_priceSchemeEnabled){
         handleStatMessagePriceScheme(ttmsg);
      }
      handleStatMessage(ttmsg);
      if (_loggingEnabled) cout<< "[AFTER HANDLING:]  "<<endl;
   }
   else if (ttmsg->getMessageType() == TIME_OUT_MSG){
      if (_loggingEnabled) cout<< "[HOST "<< myIndex() <<": RECEIVED TIME_OUT_MSG] "<<msg->getName() << endl;
      if (!_timeoutEnabled){
         cout << "timeout message generated when it shouldn't have" << endl;
         return;
      }

      if (_waterfillingEnabled){
         handleTimeOutMessageWaterfilling(ttmsg);
      }
      else{
         handleTimeOutMessageShortestPath(ttmsg);
      }
      if (_loggingEnabled) cout<< "[AFTER HANDLING:]  "<<endl;
   }
   else if (ttmsg->getMessageType() == PROBE_MSG){
      if (_loggingEnabled) cout<< "[HOST "<< myIndex() <<": RECEIVED PROBE_MSG] "<< msg->getName() << endl;
      if (_landmarkRoutingEnabled){
          handleProbeMessageLandmarkRouting(ttmsg);
      }
      else{
          handleProbeMessage(ttmsg);
      }
      if (_loggingEnabled) cout<< "[AFTER HANDLING:]  "<<endl;
   }
   else if (ttmsg->getMessageType() == CLEAR_STATE_MSG){
      if (_loggingEnabled) cout<< "[HOST "<< myIndex() <<": RECEIVED CLEAR_STATE_MSG] " << msg->getName() << endl;
      if (_priceSchemeEnabled){
         //handleClearStateMessagePriceScheme(ttmsg); //clears the transactions queued in nodeToDestInfo
         //TODO: need to write function
      }
      if (_waterfillingEnabled) {
          handleClearStateMessageWaterfilling(ttmsg);
      }

      handleClearStateMessage(ttmsg);
      if (_loggingEnabled) cout<< "[AFTER HANDLING:]  "<<endl;
   }
   else if (ttmsg->getMessageType() == TRIGGER_PRICE_UPDATE_MSG){
      if (_loggingEnabled)  cout<< "[HOST "<< myIndex() <<": RECEIVED TRIGGER_PRICE_UPDATE_MSG] "<< msg->getName()<<endl;
      handleTriggerPriceUpdateMessage(ttmsg);
      if (_loggingEnabled) cout<< "[AFTER HANDLING:]  "<<endl;
   }
   else if (ttmsg->getMessageType() == PRICE_UPDATE_MSG){
      if (_loggingEnabled)  cout<< "[HOST "<< myIndex() <<": RECEIVED PRICE_UPDATE_MSG] "<< msg->getName()<<endl;
      handlePriceUpdateMessage(ttmsg);
      if (_loggingEnabled) cout<< "[AFTER HANDLING:]  "<<endl;
   }
   else if (ttmsg->getMessageType() == TRIGGER_PRICE_QUERY_MSG){
      if (_loggingEnabled)  cout<< "[HOST "<< myIndex() <<": RECEIVED TRIGGER_PRICE_QUERY_MSG] "<< msg->getName()<<endl;
      handleTriggerPriceQueryMessage(ttmsg);
      if (_loggingEnabled) cout<< "[AFTER HANDLING:]  "<<endl;
   }
   else if (ttmsg->getMessageType() == PRICE_QUERY_MSG){
      if (_loggingEnabled)  cout<< "[HOST "<< myIndex() <<": RECEIVED PRICE_QUERY_MSG] "<< msg->getName()<<endl;
      handlePriceQueryMessage(ttmsg);
      if (_loggingEnabled) cout<< "[AFTER HANDLING:]  "<<endl;
   }
   else if (ttmsg->getMessageType() == TRIGGER_TRANSACTION_SEND_MSG){
      if (_loggingEnabled)  cout<< "[HOST "<< myIndex() <<": RECEIVED TRIGGER_TRANSACTION_SEND_MSG] "<< msg->getName()<<endl;
      handleTriggerTransactionSendMessage(ttmsg);
      if (_loggingEnabled) cout<< "[AFTER HANDLING:]  "<<endl;
   }

}

void hostNode::handleTriggerTransactionSendMessage(routerMsg* ttmsg){
   transactionSendMsg *tsMsg = check_and_cast<transactionSendMsg *>(ttmsg->getEncapsulatedPacket());

   vector<int> path = tsMsg->getTransactionPath();
   int pathIndex = tsMsg->getPathIndex();
   int destNode = tsMsg->getReceiver();

   // TODO: vibhaa clean this up
   if (nodeToDestInfo[destNode].transWaitingToBeSent.size() > 0){ //has trans to send
      //remove the transaction $tu$ at the head of the queue
      routerMsg *msgToSend = nodeToDestInfo[destNode].transWaitingToBeSent.front();
      nodeToDestInfo[destNode].transWaitingToBeSent.pop_front();
      transactionMsg *transMsg = check_and_cast<transactionMsg *>(msgToSend->getEncapsulatedPacket());
     
      // if there is a txn to send send it considering time outs from this point onwards
      if (msgToSend != NULL) {
          //Send the transaction $tu$ along the corresponding path.
          transMsg->setPathIndex(pathIndex);
          msgToSend->setRoute(path);
          msgToSend->setHopCount(0);

          // increment the number of units sent on a particular payment channel
          int nextNode = path[1];
          nodeToPaymentChannel[nextNode].nValue = nodeToPaymentChannel[nextNode].nValue + 1;

          // increment number of units sent to a particular destination on a particular path
          nodeToShortestPathsMap[destNode][pathIndex].nValue += 1;

          // increment amount in inflght on this path
          nodeToShortestPathsMap[destNode][pathIndex].sumOfTransUnitsInFlight =
                    nodeToShortestPathsMap[destNode][pathIndex].sumOfTransUnitsInFlight + transMsg->getAmount();
          
          //generate time out message here, when path is decided
          if (_timeoutEnabled) {
            routerMsg *toutMsg = generateTimeOutMessage(msgToSend);
            scheduleAt(simTime() + transMsg->getTimeOut(), toutMsg );
          }

          // cannot be cancelled at this point
          // // TODO: should you be calling handle Transaction Message or pushing into a queue for the node to payment 
          // channel ? all other schemes call handleTransactionMessage
          forwardTransactionMessage(msgToSend);

          // update the number attempted to this destination and on this path
          nodeToShortestPathsMap[destNode][pathIndex].statRateAttempted =
                nodeToShortestPathsMap[destNode][pathIndex].statRateAttempted + 1;
          statRateAttempted[destNode] += 1;

          //Update the  “time when next transaction can be sent” to (“current time” +
          //(“amount in the transaction $tu$ that is currently being sent” / “rate”)).
          double rateToSendTrans = nodeToShortestPathsMap[destNode][pathIndex].rateToSendTrans;
          if (rateToSendTrans == 0){
             rateToSendTrans = 1; //TODO: fix constant
          }
          nodeToShortestPathsMap[destNode][pathIndex].timeToNextSend =
             simTime() + min(transMsg->getAmount()/rateToSendTrans, 1.0);

          //If there are more transactions queued up, set a timer to send the “trigger
          //sending transaction” self-message at the “time when next transaction can
          //be sent” with this path and destination identifiers.
          if (nodeToDestInfo[destNode].transWaitingToBeSent.size() > 0){
            scheduleAt(nodeToShortestPathsMap[destNode][pathIndex].timeToNextSend, ttmsg);
          }
          else {
            nodeToShortestPathsMap[destNode][pathIndex].isSendTimerSet = false;
          }
      }
      else {
          // no uncancelled transactions
          nodeToShortestPathsMap[destNode][pathIndex].isSendTimerSet = false;
      }
   }
   else{ //no trans to send
      nodeToShortestPathsMap[destNode][pathIndex].isSendTimerSet = false;
   }
}



void hostNode::handlePriceQueryMessage(routerMsg* ttmsg){
   priceQueryMsg *pqMsg = check_and_cast<priceQueryMsg *>(ttmsg->getEncapsulatedPacket());

   bool isReversed = pqMsg->getIsReversed();
   if (!isReversed && ttmsg->getHopCount() == 0){ //is at hopcount 0
      int nextNode = ttmsg->getRoute()[ttmsg->getHopCount()+1];
      double zOld = pqMsg->getZValue();
      double lambda = nodeToPaymentChannel[nextNode].lambda;
      double muLocal = nodeToPaymentChannel[nextNode].muLocal;
      double muRemote = nodeToPaymentChannel[nextNode].muRemote;
      double zNew =  zOld + (2 * lambda) + muLocal  - muRemote;
      pqMsg->setZValue(zNew);
      forwardMessage(ttmsg);
   }
   else if (!isReversed){ //is at destination
      pqMsg->setIsReversed(true);
      vector<int> route = ttmsg->getRoute();
      reverse(route.begin(), route.end());
      ttmsg->setRoute(route);
      ttmsg->setHopCount(0);
      forwardMessage(ttmsg);
   }
   else{ //is back at sender
      double demand;

      double zValue = pqMsg->getZValue();
      int destNode = ttmsg->getRoute()[0];
      int routeIndex = pqMsg->getPathIndex();

      nodeToShortestPathsMap[destNode][routeIndex].priceLastSeen = zValue;
      //double oldRate = nodeToShortestPathsMap[destNode][routeIndex].xPath;
      double oldRate = nodeToShortestPathsMap[destNode][routeIndex].rateToSendTrans;
      nodeToShortestPathsMap[destNode][routeIndex].rateToSendTrans =
         maxDouble(oldRate + _alpha*(1-zValue), 0);

      // compute the projection of this new rate along with old rates
      vector<PathRateTuple> pathRateTuples;
      for (auto p : nodeToShortestPathsMap[destNode]) {
          int pathIndex = p.first;
          double rate = p.second.rateToSendTrans;

          PathRateTuple newTuple = make_tuple(pathIndex, rate);
          pathRateTuples.push_back(newTuple);
      }
      vector<PathRateTuple> projectedRates = 
          computeProjection(pathRateTuples, nodeToDestInfo[destNode].demand);

      // reassign all path's rates to the projected rates and make sure it is atleast one for every path
      for (auto p : projectedRates) {
          int pathIndex = get<0>(p);
          double rate = get<1>(p);

          nodeToShortestPathsMap[destNode][pathIndex].rateToSendTrans = maxDouble(rate, _minPriceRate);
      }
            
      //delete both messages
      ttmsg->decapsulate();
      delete pqMsg;
      delete ttmsg;
   }
}

void hostNode::handlePriceUpdateMessage(routerMsg* ttmsg){
   priceUpdateMsg *puMsg = check_and_cast<priceUpdateMsg *>(ttmsg->getEncapsulatedPacket());
   double xRemote = puMsg->getXLocal();
   int sender = ttmsg->getRoute()[0];

   //Update $\lambda$, $mu_local$ and $mu_remote$
   double xLocal = nodeToPaymentChannel[sender].xLocal;
   double cValue = nodeToPaymentChannel[sender].totalCapacity;
   double oldLambda = nodeToPaymentChannel[sender].lambda;
   double oldMuLocal = nodeToPaymentChannel[sender].muLocal;
   double oldMuRemote = nodeToPaymentChannel[sender].muRemote;
   double delta = _maxTravelTime;

   nodeToPaymentChannel[sender].lambda = maxDouble(oldLambda + _eta*(xLocal + xRemote - (cValue/delta)),0);
   nodeToPaymentChannel[sender].muLocal = maxDouble(oldMuLocal + _kappa*(xLocal - xRemote) , 0);
   nodeToPaymentChannel[sender].muRemote = maxDouble(oldMuRemote + _kappa*(xRemote - xLocal) , 0);

    //delete both messages
    ttmsg->decapsulate();
    delete puMsg;
    delete ttmsg;

}

void hostNode::handleTriggerPriceQueryMessage(routerMsg* ttmsg){
   if (simTime() > _simulationLength){
      delete ttmsg;
   }
   else{
      scheduleAt(simTime()+_tQuery, ttmsg);
   }

   for (auto it = destNodeToNumTransPending.begin(); it!=destNodeToNumTransPending.end(); it++){

      if (it->first == myIndex()){ //TODO: fix this error
         /*
            cout << "myIndex(): " << myIndex() << "; destination: " << it->first << endl;
            for (auto p: nodeToShortestPathsMap[it->first] ){
            vector<int> path = p.second.path;
            printVector(path);
            }
          */
         continue;

      }
      if (it->second>0){ //if we have transactions pending
         for (auto p = nodeToShortestPathsMap[it->first].begin() ; p!= nodeToShortestPathsMap[it->first].end(); p++){
            //p is per path in the destNode
            int routeIndex = p->first;
            PathInfo pInfo = nodeToShortestPathsMap[it->first][p->first];
            routerMsg * msg = generatePriceQueryMessage(pInfo.path, routeIndex);
            handlePriceQueryMessage(msg);
         }
      }
   }
}

void hostNode::handleTriggerPriceUpdateMessage(routerMsg* ttmsg){
   if (simTime() > _simulationLength){
      delete ttmsg;
   }
   else{
      scheduleAt(simTime()+_tUpdate, ttmsg);
   }

   for ( auto it = nodeToPaymentChannel.begin(); it!= nodeToPaymentChannel.end(); it++){      
      nodeToPaymentChannel[it->first].xLocal =   nodeToPaymentChannel[it->first].nValue / _tUpdate;
      nodeToPaymentChannel[it->first].nValue = 0;
      if (it->first<0){
         printNodeToPaymentChannel();
         endSimulation();
      }
      routerMsg * priceUpdateMsg = generatePriceUpdateMessage(nodeToPaymentChannel[it->first].xLocal, it->first);
      sendUpdateMessage(priceUpdateMsg);
   }

   // also update the xPath for a given destination and path to use for your next rate computation
   for ( auto it = nodeToShortestPathsMap.begin(); it != nodeToShortestPathsMap.end(); it++ ) {
       int destNode = it->first;
       for (auto p : nodeToShortestPathsMap[destNode]) {
          int pathIndex = p.first;
          double latestRate = nodeToShortestPathsMap[destNode][pathIndex].nValue / _tUpdate;
          double oldRate = nodeToShortestPathsMap[destNode][pathIndex].xPath;

          nodeToShortestPathsMap[destNode][pathIndex].nValue = 0; 
          nodeToShortestPathsMap[destNode][pathIndex].xPath = (1 - _gamma) * oldRate + _gamma * latestRate; 
       }


       DestInfo* destInfo = &(nodeToDestInfo[destNode]);
       double newDemand = destInfo->transSinceLastInterval/_tUpdate;
       destInfo->demand = (1 - _zeta) * destInfo->demand + _zeta * newDemand;
       destInfo->transSinceLastInterval = 0;
   }
}

routerMsg * hostNode::generatePriceUpdateMessage(double xLocal, int reciever){
   char msgname[MSGSIZE];

   sprintf(msgname, "tic-%d-to-%d priceUpdateMsg", myIndex(), reciever);

   routerMsg *rMsg = new routerMsg(msgname);

   vector<int> route={myIndex(),reciever};
   rMsg->setRoute(route);
   rMsg->setHopCount(0);
   rMsg->setMessageType(PRICE_UPDATE_MSG);

   priceUpdateMsg *puMsg = new priceUpdateMsg(msgname);

   puMsg->setXLocal(xLocal);
   rMsg->encapsulate(puMsg);
   return rMsg;
}


void hostNode::handleClearStateMessageWaterfilling(routerMsg *ttsmg) {
        for ( auto it = canceledTransactions.begin(); it!= canceledTransactions.end(); it++){
            //iterate through all canceledTransactions
            int transactionId = get<0>(*it);
            simtime_t msgArrivalTime = get<1>(*it);
            int prevNode = get<2>(*it);
            int nextNode = get<3>(*it);
            int destNode = get<4>(*it);

            if (simTime() > (msgArrivalTime + _maxTravelTime)){
              for (auto p : nodeToShortestPathsMap[destNode]) {
                    int pathIndex = p.first;
                    tuple<int,int> key = make_tuple(transactionId, pathIndex);
                    if (transPathToAckState.count(key) != 0) {
                        nodeToShortestPathsMap[destNode][pathIndex].sumOfTransUnitsInFlight -= 
                        (transPathToAckState[key].amtSent - transPathToAckState[key].amtReceived);
                        transPathToAckState.erase(key);
                    }
              }
            }
        }
}



void hostNode::handleClearStateMessage(routerMsg* ttmsg){
   if (simTime() > _simulationLength){
      delete ttmsg;
   }
   else{
      scheduleAt(simTime()+_clearRate, ttmsg);
   }

   for ( auto it = canceledTransactions.begin(); it!= canceledTransactions.end(); ){ //iterate through all canceledTransactions
      int transactionId = get<0>(*it);
      simtime_t msgArrivalTime = get<1>(*it);
      int prevNode = get<2>(*it);
      int nextNode = get<3>(*it);
      int destNode = get<4>(*it);


      if (simTime() > (msgArrivalTime + _maxTravelTime)){ //we can process it

         if (nextNode != -1){
            vector<tuple<int, double, routerMsg*, Id>>* queuedTransUnits = &(nodeToPaymentChannel[nextNode].queuedTransUnits);
            //sort_heap((*queuedTransUnits).begin(), (*queuedTransUnits).end());

            auto iterQueue = find_if((*queuedTransUnits).begin(),
                  (*queuedTransUnits).end(),
                  [&transactionId](const tuple<int, double, routerMsg*, Id>& p)
                  { return (get<0>(get<3>(p)) == transactionId); });
            while (iterQueue != (*queuedTransUnits).end()){
                routerMsg * rMsg = get<2>(*iterQueue);
                       auto tMsg = rMsg->getEncapsulatedPacket();
                       rMsg->decapsulate();
                       delete tMsg;
                       delete rMsg;
               iterQueue =   (*queuedTransUnits).erase(iterQueue);


               iterQueue = find_if((*queuedTransUnits).begin(),
                     (*queuedTransUnits).end(),
                     [&transactionId](const tuple<int, double, routerMsg*, Id>& p)
                     { return (get<0>(get<3>(p)) == transactionId); });
            }

            make_heap((*queuedTransUnits).begin(), (*queuedTransUnits).end(), sortPriorityThenAmtFunction);
            //end queue searching
         }

         if (prevNode != -1){
            map<tuple<int,int>, double> *incomingTransUnits = &(nodeToPaymentChannel[prevNode].incomingTransUnits);
            auto iterIncoming = find_if((*incomingTransUnits).begin(),
                  (*incomingTransUnits).end(),
                  [&transactionId](const pair<tuple<int, int >, double> & p)
                  { return get<0>(p.first) == transactionId; });
            while (iterIncoming != (*incomingTransUnits).end()){

               iterIncoming = (*incomingTransUnits).erase(iterIncoming);
               tuple<int, int> tempId = make_tuple(transactionId, get<1>(iterIncoming->first));

               iterIncoming = find_if((*incomingTransUnits).begin(),
                     (*incomingTransUnits).end(),
                     [&transactionId](const pair<tuple<int, int >, double> & p)
                     { return get<0>(p.first) == transactionId; });
            }
         }

         if (nextNode != -1){
            map<tuple<int,int>, double> *outgoingTransUnits = &(nodeToPaymentChannel[nextNode].outgoingTransUnits);
            auto iterOutgoing = find_if((*outgoingTransUnits).begin(),
                  (*outgoingTransUnits).end(),
                  [&transactionId](const pair<tuple<int, int >, double> & p)
                  { return get<0>(p.first) == transactionId; });
            while (iterOutgoing != (*outgoingTransUnits).end()){
               double amount = iterOutgoing -> second;
               iterOutgoing = (*outgoingTransUnits).erase(iterOutgoing);
              
               double updatedBalance = nodeToPaymentChannel[nextNode].balance + amount;
               nodeToPaymentChannel[nextNode].balance = updatedBalance; 
               nodeToPaymentChannel[nextNode].balanceEWMA = 
          (1 -_ewmaFactor) * nodeToPaymentChannel[nextNode].balanceEWMA + (_ewmaFactor) * updatedBalance;

               iterOutgoing = find_if((*outgoingTransUnits).begin(),
                     (*outgoingTransUnits).end(),
                     [&transactionId](const pair<tuple<int, int >, double> & p)
                     { return get<0>(p.first) == transactionId; });
            }
         }
         it = canceledTransactions.erase(it);

         // we've actually canceleld the transaction and didn't receive an ack or anything
         statNumTimedOut[destNode] = statNumTimedOut[destNode]  + 1;

      }
      else{
         it++;
      }
   }
}


routerMsg *hostNode::generateStatMessage(){
   char msgname[MSGSIZE];
   sprintf(msgname, "node-%d statMsg", myIndex());
   routerMsg *rMsg = new routerMsg(msgname);
   rMsg->setMessageType(STAT_MSG);
   return rMsg;
}

int maxTwoNum(int a, int b){
   if (a>b){
      return a;
   }
   return b;
}


void hostNode::handleStatMessagePriceScheme(routerMsg* ttmsg){
   if (_signalsEnabled) {
      for ( auto it = nodeToPaymentChannel.begin(); it!= nodeToPaymentChannel.end(); it++){ //iterate through all adjacent nodes

         int node = it->first; //key

         PaymentChannel* p = &(nodeToPaymentChannel[node]);

         //statistics for price scheme per payment channel
         simsignal_t nValueSignal;
         simsignal_t xLocalSignal;
         simsignal_t lambdaSignal;
         simsignal_t muLocalSignal;
         simsignal_t muRemoteSignal;

         emit(p->nValueSignal, p->nValue);
         emit(p->xLocalSignal, p->xLocal);
         emit(p->lambdaSignal, p->lambda);
         emit(p->muLocalSignal, p->muLocal);
         emit(p->muRemoteSignal, p->muRemote);
      }


      for (auto it = 0; it<_numHostNodes; it++){ //iterate through all nodes in graph

         if (it != getIndex()){
            if (nodeToShortestPathsMap.count(it) > 0) {
               for (auto p: nodeToShortestPathsMap[it]){
                  int pathIndex = p.first;
                  PathInfo *pInfo = &(p.second);

                  //signals for price scheme per path
                  emit(pInfo->rateToSendTransSignal, pInfo->rateToSendTrans);
                  emit(pInfo->timeToNextSendSignal, pInfo->timeToNextSend);
                  emit(pInfo->sumOfTransUnitsInFlightSignal, pInfo->sumOfTransUnitsInFlight);
                  emit(pInfo->priceLastSeenSignal, pInfo->priceLastSeen);
                  emit(pInfo->isSendTimerSetSignal, pInfo->isSendTimerSet);

               }
            }
         }//end if (nodeToShortestPathsMap.count(it) > 0)
      }//end for
   } // end if (_loggingEnabled)
}

void hostNode::finish(){
    deleteMessagesInQueues();
}

void hostNode::handleStatMessage(routerMsg* ttmsg){
   if (simTime() > _simulationLength){
      delete ttmsg;

   }
   else{
      scheduleAt(simTime()+_statRate, ttmsg);
   }

   if (_signalsEnabled) {
      for ( auto it = nodeToPaymentChannel.begin(); it!= nodeToPaymentChannel.end(); it++){ //iterate through all adjacent nodes

         int node = it->first; //key
         emit(nodeToPaymentChannel[node].numInQueuePerChannelSignal, (nodeToPaymentChannel[node].queuedTransUnits).size());

         emit(nodeToPaymentChannel[node].balancePerChannelSignal, nodeToPaymentChannel[node].balance);
         
         emit(nodeToPaymentChannel[node].numProcessedPerChannelSignal, nodeToPaymentChannel[node].statNumProcessed);
         nodeToPaymentChannel[node].statNumProcessed = 0;

         emit(nodeToPaymentChannel[node].numSentPerChannelSignal, nodeToPaymentChannel[node].statNumSent);
         nodeToPaymentChannel[node].statNumSent = 0;
      }
   }

   for (auto it = 0; it<_numHostNodes; it++){ //iterate through all nodes in graph
      if (it != getIndex()){
         if (nodeToShortestPathsMap.count(it) > 0) {
            for (auto p: nodeToShortestPathsMap[it]){
               if (_signalsEnabled) {
                   emit(p.second.bottleneckPerDestPerPathSignal, p.second.bottleneck);
                   emit(p.second.probabilityPerDestPerPathSignal, p.second.probability);
               }

               //emit rateCompleted per path
               emit(p.second.rateCompletedPerDestPerPathSignal, p.second.statRateCompleted);
               nodeToShortestPathsMap[it][p.first].statRateCompleted = 0;

               //emit rateAttempted per path
               emit(p.second.rateAttemptedPerDestPerPathSignal, p.second.statRateAttempted);
               nodeToShortestPathsMap[it][p.first].statRateAttempted = 0;
            }
         }


         emit(rateAttemptedPerDestSignals[it], statRateAttempted[it]);
         statRateAttempted[it] = 0;
         
         emit(rateArrivedPerDestSignals[it], statRateArrived[it]);
         statRateArrived[it] = 0;
         
         emit(rateCompletedPerDestSignals[it], statRateCompleted[it]);
         statRateCompleted[it] = 0;

         if (_signalsEnabled) {
            emit(numArrivedPerDestSignals[it], statNumArrived[it]);
         if (_hasQueueCapacity){

                    emit(numFailedPerDestSignals[it], statNumFailed[it]);

                    emit(rateFailedPerDestSignals[it], statRateFailed[it]);
           }

            emit(numCompletedPerDestSignals[it], statNumCompleted[it]);
            emit(numTimedOutPerDestSignals[it], statNumTimedOut[it]);
            emit(numTimedOutAtSenderSignals[it], statNumTimedOutAtSender[it]);
            emit(demandEstimatePerDestSignals[it], nodeToDestInfo[it].demand); 
            emit(numPendingPerDestSignals[it], destNodeToNumTransPending[it]);
            emit(numWaitingPerDestSignals[it], nodeToDestInfo[it].transWaitingToBeSent.size()); 

            int frac = ((100*statNumCompleted[it])/(maxTwoNum(statNumArrived[it],1)));
            emit(fracSuccessfulPerDestSignals[it],frac);

         }


      }//end if

   }//end for

}

void hostNode::forwardTimeOutMessage(routerMsg* msg){
   // Increment hop count.
   msg->setHopCount(msg->getHopCount()+1);
   //use hopCount to find next destination
   int nextDest = msg->getRoute()[msg->getHopCount()];
   if (_loggingEnabled) cout << "forwardTimeOutMsg send: " << simTime() << endl;
   send(msg, nodeToPaymentChannel[nextDest].gate);
}


void hostNode::forwardMessage(routerMsg* msg){
   // Increment hop count.
   msg->setHopCount(msg->getHopCount()+1);
   //use hopCount to find next destination
   int nextDest = msg->getRoute()[msg->getHopCount()];
   if (_loggingEnabled) cout << "forwardMsg send: "<< simTime() << endl;
   send(msg, nodeToPaymentChannel[nextDest].gate);
}

void hostNode::handleTimeOutMessageWaterfilling(routerMsg* ttmsg){
   timeOutMsg *toutMsg = check_and_cast<timeOutMsg *>(ttmsg->getEncapsulatedPacket());

   if ((ttmsg->getHopCount())==0){ //is at the sender
      //TODO: add to canceledTrans set
      int transactionId = toutMsg->getTransactionId();
      int destination = toutMsg->getReceiver();
      // map<int, map<int, PathInfo>> nodeToShortestPathsMap = {};
      for (auto p : (nodeToShortestPathsMap[destination])){
         int pathIndex = p.first;
         tuple<int,int> key = make_tuple(transactionId, pathIndex);
         if(_loggingEnabled) cout << "transPathToAckState.count(key): " << transPathToAckState.count(key) << endl;
         if(_loggingEnabled) cout << "transactionId: " << transactionId << "; pathIndex: " << pathIndex << endl;
         if(transPathToAckState[key].amtSent == transPathToAckState[key].amtReceived){
            //do not generate time out msg for path
         }
         else{
            routerMsg* waterTimeOutMsg = generateWaterfillingTimeOutMessage(
                  nodeToShortestPathsMap[destination][p.first].path, transactionId, destination);
            // Radhika: what if a transaction on two different paths have same next hop?
            int nextNode = (waterTimeOutMsg->getRoute())[waterTimeOutMsg->getHopCount()+1];
            CanceledTrans ct = make_tuple(toutMsg->getTransactionId(),simTime(),-1, nextNode, destination);
            canceledTransactions.insert(ct);

            forwardTimeOutMessage(waterTimeOutMsg);
         }
      }
      delete ttmsg;
   }
   else{

      CanceledTrans ct = make_tuple(toutMsg->getTransactionId(),simTime(),(ttmsg->getRoute())[ttmsg->getHopCount()-1],-1, 
              toutMsg->getReceiver());
      canceledTransactions.insert(ct);
      //is at the destination
      ttmsg->decapsulate();
      delete toutMsg;
      delete ttmsg;
   }//end else ((ttmsg->getHopCount())==0)
}

/*
 *  handleTimeOutMessage -
 */
void hostNode::handleTimeOutMessageShortestPath(routerMsg* ttmsg){
   timeOutMsg *toutMsg = check_and_cast<timeOutMsg *>(ttmsg->getEncapsulatedPacket());
   int destination = toutMsg->getReceiver();

   if ((ttmsg->getHopCount())==0){ //is at the sender
      if (successfulDoNotSendTimeOut.count(toutMsg->getTransactionId())>0){ //already received ack for it, do not send out
         successfulDoNotSendTimeOut.erase(toutMsg->getTransactionId());
         ttmsg->decapsulate();
         delete toutMsg;
         delete ttmsg;
      }

      else{
         int nextNode = (ttmsg->getRoute())[ttmsg->getHopCount()+1];
         CanceledTrans ct = make_tuple(toutMsg->getTransactionId(),simTime(),-1, nextNode, destination);
         canceledTransactions.insert(ct);
         forwardTimeOutMessage(ttmsg);
      }
   }
   else{ //is at the destination
      CanceledTrans ct = make_tuple(toutMsg->getTransactionId(),simTime(),(ttmsg->getRoute())[ttmsg->getHopCount()-1],-1, destination);

      canceledTransactions.insert(ct);

      ttmsg->decapsulate();
      delete toutMsg;
      delete ttmsg;
   }//end else ((ttmsg->getHopCount())==0)
}

void hostNode::handleAckMessageTimeOut(routerMsg* ttmsg){
   ackMsg *aMsg = check_and_cast<ackMsg *>(ttmsg->getEncapsulatedPacket());
   //check if transactionId is in canceledTransaction set
   int transactionId = aMsg->getTransactionId();

   //Radhika: what if there are multiple HTLC's per transaction? 
   auto iter = find_if(canceledTransactions.begin(),
         canceledTransactions.end(),
         [&transactionId](const tuple<int, simtime_t, int, int, int>& p)
         { return get<0>(p) == transactionId; });

   if (iter!=canceledTransactions.end()){
      canceledTransactions.erase(iter);
   }

   //at last index of route
   //add transactionId to set of successful transactions that we don't need to send time out messages
   if (!_waterfillingEnabled){
      successfulDoNotSendTimeOut.insert(aMsg->getTransactionId());

   }
   /*else{ //_waterfillingEnabled
      //Radhika TODO: following should happen irrespective of timeouts?
      tuple<int,int> key = make_tuple(aMsg->getTransactionId(), aMsg->getPathIndex());

      transPathToAckState[key].amtReceived = transPathToAckState[key].amtReceived + aMsg->getAmount();

         if (transPathToAckState[key].amtReceived == transPathToAckState[key].amtSent){
         transPathToAckState.erase(key);
         }
    
   }*/
}


void hostNode::handleAckMessageShortestPath(routerMsg* ttmsg){
   int destNode = ttmsg->getRoute()[0]; //destination of origin TransUnit job
   ackMsg *aMsg = check_and_cast<ackMsg *>(ttmsg->getEncapsulatedPacket());

   //cout << "ackMessage received " << endl;
   if (aMsg->getIsSuccess()==false){
       //cout << "ack failure" << endl;
      statNumFailed[destNode] = statNumFailed[destNode]+1;
      statRateFailed[destNode] = statRateFailed[destNode]+1;
   }
   else{
       //cout << "ack success" << endl;
      statNumCompleted[destNode] = statNumCompleted[destNode]+1;
      statRateCompleted[destNode] = statRateCompleted[destNode]+1;
   }
}

void hostNode::handleAckMessagePriceScheme(routerMsg* ttmsg){
   ackMsg *aMsg = check_and_cast<ackMsg *>(ttmsg->getEncapsulatedPacket());
   int pathIndex = aMsg->getPathIndex();
   int destNode = ttmsg->getRoute()[0]; //destination of origin TransUnit job
   if (aMsg->getIsSuccess()==false){
      statNumFailed[destNode] = statNumFailed[destNode]+1;
      statRateFailed[destNode] = statRateFailed[destNode]+1;
   }
   else{
      statNumCompleted[destNode] = statNumCompleted[destNode]+1;
      statRateCompleted[destNode] = statRateCompleted[destNode]+1;
      nodeToShortestPathsMap[destNode][pathIndex].statRateCompleted += 1;
   }

   nodeToShortestPathsMap[destNode][pathIndex].sumOfTransUnitsInFlight -= aMsg->getAmount();
   destNodeToNumTransPending[destNode] = destNodeToNumTransPending[destNode] - 1;

}

void hostNode::handleAckMessageWaterfilling(routerMsg* ttmsg){
   ackMsg *aMsg = check_and_cast<ackMsg *>(ttmsg->getEncapsulatedPacket());
   //check all amt received before incrementing num completed
   if (transToAmtLeftToComplete.count(aMsg->getTransactionId()) == 0){
      cout << "error, transaction " << aMsg->getTransactionId() <<" htlc index:" << aMsg->getHtlcIndex() << 
         " acknowledged at time " << simTime() << " wasn't written to transToAmtLeftToComplete" << endl;
   }
   else{
      (transToAmtLeftToComplete[aMsg->getTransactionId()]).amtReceived =
         transToAmtLeftToComplete[aMsg->getTransactionId()].amtReceived + aMsg->getAmount();

      //increment rateNumCompletedPerDestPerPath
      nodeToShortestPathsMap[aMsg->getReceiver()][aMsg->getPathIndex()].statRateCompleted =
         nodeToShortestPathsMap[aMsg->getReceiver()][aMsg->getPathIndex()].statRateCompleted + 1;

      if (transToAmtLeftToComplete[aMsg->getTransactionId()].amtReceived == transToAmtLeftToComplete[aMsg->getTransactionId()].amtSent){
         statNumCompleted[aMsg->getReceiver()] = statNumCompleted[aMsg->getReceiver()] + 1;
         statRateCompleted[aMsg->getReceiver()] = statRateCompleted[aMsg->getReceiver()] + 1;
         //erase transaction from map
         transToAmtLeftToComplete.erase(aMsg->getTransactionId());
      }

      //increment transaction amount ack on a path. 
      tuple<int,int> key = make_tuple(aMsg->getTransactionId(), aMsg->getPathIndex());
      transPathToAckState[key].amtReceived = transPathToAckState[key].amtReceived + aMsg->getAmount();

      // decrement amtinflight on a path
     int destNode = aMsg->getReceiver();
     nodeToShortestPathsMap[destNode][aMsg->getPathIndex()].sumOfTransUnitsInFlight -= aMsg->getAmount();
    destNodeToNumTransPending[destNode] = destNodeToNumTransPending[destNode] - 1;
			
   
    //Radhika: why is this commented out?
      /*
         if (transPathToAckState[key].amtReceived == transPathToAckState[key].amtSent){
         transPathToAckState.erase(key);
         }
       */
   }
}


/* handleAckMessage
 */
void hostNode::handleAckMessage(routerMsg* ttmsg){

    // this method can only be called at the sender of a transaction
    // so there's no need to check incoming trans units unlike routers 
    // assert that this is the sender @Vibhaa

   //generate updateMsg
   // this is previous node on the ack path, so next node on the forward path
   int prevNode = ttmsg->getRoute()[ttmsg->getHopCount()-1];

   //note: prevNode is nextNode in original route (is only prev in ack reversed route)
   //remove transaction from outgoing_trans_unit
   map<Id, double> *outgoingTransUnits = &(nodeToPaymentChannel[prevNode].outgoingTransUnits);
   ackMsg *aMsg = check_and_cast<ackMsg *>(ttmsg->getEncapsulatedPacket());

   (*outgoingTransUnits).erase(make_tuple(aMsg->getTransactionId(), aMsg->getHtlcIndex()));
   if (aMsg->getIsSuccess() == false){
      //increment payment back to outgoing channel
      // unless I'm the node where it failed
      if (aMsg->getFailedHopNum() != myIndex())
        nodeToPaymentChannel[prevNode].balance = nodeToPaymentChannel[prevNode].balance + aMsg->getAmount();

      //removing transaction from incoming_trans_units is not necessary because no node after this one
      if (ttmsg->getHopCount() < ttmsg->getRoute().size()-1){
          int nextNode = ttmsg->getRoute()[ttmsg->getHopCount()-1];
      map<Id, double> *incomingTransUnits = &(nodeToPaymentChannel[nextNode].incomingTransUnits);

      (*incomingTransUnits).erase(make_tuple(aMsg->getTransactionId(), aMsg->getHtlcIndex()));
      }


   }
   else{ //isSuccess == true
      routerMsg* uMsg =  generateUpdateMessage(aMsg->getTransactionId(), prevNode, aMsg->getAmount(), aMsg->getHtlcIndex() );
      sendUpdateMessage(uMsg);

   }


   //increment signal numProcessed
   nodeToPaymentChannel[prevNode].statNumProcessed = nodeToPaymentChannel[prevNode].statNumProcessed + 1;
   //assume successful transaction


   //broadcast completion time signal
   simtime_t timeTakenInMilli = 1000*(simTime() - aMsg->getTimeSent());
   if (_signalsEnabled) emit(completionTimeSignal, timeTakenInMilli);

   //delete ack message
   ttmsg->decapsulate();
   delete aMsg;
   delete ttmsg;
}

/*
 * generateAckMessage - called only when a transactionMsg reaches end of its path, keeps routerMsg casing of msg
 *   and reuses it to send ackMsg in reversed order of route
 */
routerMsg *hostNode::generateAckMessage(routerMsg* ttmsg, bool isSuccess ){ //default is true
   int transactionId;
   double timeSent;
   double amount;
   int sender = (ttmsg->getRoute())[0];
   int receiver = (ttmsg->getRoute())[(ttmsg->getRoute()).size() -1];
   bool hasTimeOut;



    int nextNode = ttmsg->getRoute()[ttmsg->getHopCount()+1];



   transactionMsg *transMsg = check_and_cast<transactionMsg *>(ttmsg->getEncapsulatedPacket());
   transactionId = transMsg->getTransactionId();
   timeSent = transMsg->getTimeSent();
   amount = transMsg->getAmount();
   hasTimeOut = transMsg->getHasTimeOut();
   char msgname[MSGSIZE];




   sprintf(msgname, "receiver-%d-to-sender-%d ackMsg", receiver, sender);
   routerMsg *msg = new routerMsg(msgname);
   ackMsg *aMsg = new ackMsg(msgname);
   aMsg->setTransactionId(transactionId);
   aMsg->setIsSuccess(isSuccess);
   aMsg->setTimeSent(timeSent);
   aMsg->setAmount(amount);
   aMsg->setReceiver(transMsg->getReceiver());
   aMsg->setHasTimeOut(hasTimeOut);
   aMsg->setHtlcIndex(transMsg->getHtlcIndex());
   aMsg->setPathIndex(transMsg->getPathIndex());

    //no need to set secret;
   vector<int> route = ttmsg->getRoute();
   //route.resize(ttmsg->getHopCount() + 1); //don't resize, might mess up destination
   reverse(route.begin(), route.end());

   msg->setRoute(route);
   msg->setHopCount((route.size()-1) - ttmsg->getHopCount());
   //need to reverse path from current hop number in case of partial failure
   msg->setMessageType(ACK_MSG); //now an ack message
   // remove encapsulated packet
   ttmsg->decapsulate();
   delete transMsg;
   delete ttmsg;
   msg->encapsulate(aMsg);
   return msg;
}

/*
 * forwardAckMessage - called inside handleAckMsg, sends ackMsg to next destination, and

 *      adds transId to sent_ack_trans_units list
 */
void hostNode::forwardAckMessage(routerMsg *msg){
   // Increment hop count.
   msg->setHopCount(msg->getHopCount()+1);
   //use hopCount to find next destination
   int nextDest = msg->getRoute()[msg->getHopCount()];
   if (_loggingEnabled) cout << "forwardAckMsg send: "<< simTime() << endl;
   send(msg, nodeToPaymentChannel[nextDest].gate);
}

/*
 * handleUpdateMessage - called when receive update message, increment back funds, see if we can
 *      process more jobs with new funds, delete update message
 */
void hostNode::handleUpdateMessage(routerMsg* msg){
   vector<tuple<int, double , routerMsg *, Id>> *q;
   int prevNode = msg->getRoute()[msg->getHopCount()-1];

   updateMsg *uMsg = check_and_cast<updateMsg *>(msg->getEncapsulatedPacket());
   //increment the in flight funds back
   double newBalance = nodeToPaymentChannel[prevNode].balance + uMsg->getAmount();
   nodeToPaymentChannel[prevNode].balance =  newBalance;       
   nodeToPaymentChannel[prevNode].balanceEWMA = 
          (1 -_ewmaFactor) * nodeToPaymentChannel[prevNode].balanceEWMA + (_ewmaFactor) * newBalance; 

   //remove transaction from incoming_trans_units
   map<Id, double> *incomingTransUnits = &(nodeToPaymentChannel[prevNode].incomingTransUnits);

   (*incomingTransUnits).erase(make_tuple(uMsg->getTransactionId(), uMsg->getHtlcIndex()));

   nodeToPaymentChannel[prevNode].statNumProcessed = nodeToPaymentChannel[prevNode].statNumProcessed + 1;

   msg->decapsulate();
   delete uMsg;
   delete msg; //delete update message

   //see if we can send more jobs out
   q = &(nodeToPaymentChannel[prevNode].queuedTransUnits);
   processTransUnits(prevNode, *q);
}

routerMsg *hostNode::generateClearStateMessage(){
   char msgname[MSGSIZE];
   sprintf(msgname, "node-%d clearStateMsg", myIndex());
   routerMsg *rMsg = new routerMsg(msgname);
   rMsg->setMessageType(CLEAR_STATE_MSG);
   return rMsg;
}

/*
 *  generateUpdateMessage - creates new message to send as updateMessage
 *      - note: all update messages have route length of exactly 2
 *
 */
routerMsg *hostNode::generateUpdateMessage(int transId, int receiver, double amount, int htlcIndex){
   char msgname[MSGSIZE];

   sprintf(msgname, "tic-%d-to-%d updateMsg", myIndex(), receiver);

   routerMsg *rMsg = new routerMsg(msgname);

   vector<int> route={myIndex(),receiver};
   rMsg->setRoute(route);
   rMsg->setHopCount(0);
   rMsg->setMessageType(UPDATE_MSG);

   updateMsg *uMsg = new updateMsg(msgname);

   uMsg->setAmount(amount);
   uMsg->setTransactionId(transId);
   uMsg->setHtlcIndex(htlcIndex);
   rMsg->encapsulate(uMsg);
   return rMsg;
}


/*
 * forwardUpdateMessage - sends updateMessage to appropriate destination
 */
void hostNode::sendUpdateMessage(routerMsg *msg){
   // Increment hop count.
   msg->setHopCount(msg->getHopCount()+1);
   //use hopCount to find next destination
   int nextDest = msg->getRoute()[msg->getHopCount()];

   if (_loggingEnabled) {
      cout << "forwardUpdateMsg send:"<< simTime() << endl;
      cout << "msgName: "<< msg->getName() << endl;
      cout << "route: " << endl;
      printVector(msg->getRoute());
   }
   send(msg, nodeToPaymentChannel[nextDest].gate);
}

routerMsg* hostNode::generateWaterfillingTransactionMessage(double amt, vector<int> path, int pathIndex, transactionMsg* transMsg){
   char msgname[MSGSIZE];
   sprintf(msgname, "tic-%d-to-%d water-transMsg", myIndex(), transMsg->getReceiver());
   transactionMsg *msg = new transactionMsg(msgname);
   msg->setAmount(amt);
   msg->setTimeSent(transMsg->getTimeSent());
   msg->setSender(transMsg->getSender());
   msg->setReceiver(transMsg->getReceiver());
   msg->setPriorityClass(transMsg->getPriorityClass());
   int transactionId = transMsg->getTransactionId();
   int htlcIndex = 0;
   msg->setTransactionId(transactionId);
   if (transactionIdToNumHtlc.count(transactionId) == 0){
      transactionIdToNumHtlc[transactionId] = 1;
   }
   else{
      htlcIndex =  transactionIdToNumHtlc[transactionId];
      transactionIdToNumHtlc[transactionId] = transactionIdToNumHtlc[transactionId] + 1;
   }

   msg->setHtlcIndex(htlcIndex);
   msg->setHasTimeOut(transMsg->getHasTimeOut());
   msg->setPathIndex(pathIndex);
   msg->setTimeOut(transMsg->getTimeOut());
   msg->setTransactionId(transMsg->getTransactionId());

   sprintf(msgname, "tic-%d-to-%d water-routerTransMsg", myIndex(), transMsg->getReceiver());
   routerMsg *rMsg = new routerMsg(msgname);
   rMsg->setRoute(path);

   rMsg->setHopCount(0);
   rMsg->setMessageType(TRANSACTION_MSG);
   rMsg->encapsulate(msg);
   return rMsg;

}

routerMsg* hostNode::generateWaterfillingTimeOutMessage( vector<int> path, int transactionId, int receiver){
   char msgname[MSGSIZE];
   sprintf(msgname, "tic-%d-to-%d water-timeOutMsg", myIndex(), receiver);
   timeOutMsg *msg = new timeOutMsg(msgname);

   msg->setReceiver(receiver);
   msg->setTransactionId(transactionId);

   sprintf(msgname, "tic-%d-to-%d water-router-timeOutMsg", myIndex(), receiver);
   routerMsg *rMsg = new routerMsg(msgname);
   rMsg->setRoute(path);

   rMsg->setHopCount(0);
   rMsg->setMessageType(TIME_OUT_MSG);
   rMsg->encapsulate(msg);
   return rMsg;
}


// computes the updated path probabilities based on the current state of 
// bottleneck link balances and returns the next path index to send the transaction 
// on in accordance to the latest rate

int hostNode::updatePathProbabilities(vector<double> bottleneckBalances, int destNode) {

    double averageBottleneck = accumulate(bottleneckBalances.begin(), 
            bottleneckBalances.end(), 0.0)/bottleneckBalances.size(); 
                
    double timeSinceLastTxn = simTime().dbl() - destNodeToLastMeasurementTime[destNode];
    destNodeToLastMeasurementTime[destNode] = simTime().dbl();

    // compute new porbabailities based on adjustment factor and expression
    vector<double> probabilities;
    int i = 0;
    for (auto b : bottleneckBalances) {
        probabilities.push_back(nodeToShortestPathsMap[destNode][i].probability + 
            (1 - exp(-1 * timeSinceLastTxn/_Tau))*(b - averageBottleneck)/_Normalizer);
        probabilities[i] = max(0.0, probabilities[i]);
        i++;
    }
    double sumProbabilities = accumulate(probabilities.begin(), probabilities.end(), 0.0); 
    
    // normalize them to 1 and update the stored probabilities
    for (i = 0; i < probabilities.size(); i++) {
        probabilities[i] /= sumProbabilities;
        nodeToShortestPathsMap[destNode][i].probability = probabilities[i];
    }
    return sampleFromDistribution(probabilities);

}


// samples a random number (index) of the passed in vector
// based on the actual probabilities passed in

int hostNode::sampleFromDistribution(vector<double> probabilities) {

    vector<double> cumProbabilities { 0 };

    double sumProbabilities = accumulate(probabilities.begin(), probabilities.end(), 0.0); 
    assert(sumProbabilities < 1.0);
    
    // compute cumulative probabilities
    for (int i = 0; i < probabilities.size(); i++) {
        cumProbabilities.push_back(probabilities[i] + cumProbabilities[i]);
    }

    // generate the next index to send on based on these probabilities
    double value  = (rand() / double(RAND_MAX));
    int index;
    for (int i = 1; i < cumProbabilities.size(); i++) {
        if (value < cumProbabilities[i])
            return i - 1;
    }

    // should never be reached
    //assert(false);

    return 0;
}



void hostNode::splitTransactionForWaterfilling(routerMsg * ttmsg){
   transactionMsg *transMsg = check_and_cast<transactionMsg *>(ttmsg->getEncapsulatedPacket());
   int destNode = transMsg->getReceiver();
   double remainingAmt = transMsg->getAmount();
   bool randomChoice = false;

   map<int, double> pathMap = {}; //key is pathIdx, double is amt
   vector<double> bottleneckList;
   
   priority_queue<pair<double,int>> pq;
   if (_loggingEnabled) cout << "bottleneck for node " <<  getIndex();
   
   // fill up priority queue
   for (auto iter: nodeToShortestPathsMap[destNode] ){
      int key = iter.first;
      double bottleneck = (iter.second).bottleneck;
      double inflight = (iter.second).sumOfTransUnitsInFlight;
      bottleneckList.push_back(bottleneck);
      if (_loggingEnabled) cout << bottleneck << " (" << key  << "," << iter.second.lastUpdated<<"), ";
      
      pq.push(make_pair(bottleneck - inflight, key)); //automatically sorts with biggest first index-element
   }
   if (_loggingEnabled) cout << endl;


   double highestBal;
   double secHighestBal;
   double diffToSend;
   double amtToSend;
   int highestBalIdx;
   int numPaths = nodeToShortestPathsMap[destNode].size();

    if (randomChoice) {
       vector<double> probabilities (numPaths, 1.0/numPaths);
       int pathIndex = sampleFromDistribution(probabilities);
       pathMap[pathIndex] = pathMap[pathIndex] + remainingAmt;
       remainingAmt = 0;
    } 
    else if (_smoothWaterfillingEnabled) {
        highestBal = get<0>(pq.top());

        if (highestBal >= 0) {
            // Vibhaa : >= 0 here gives slightly better results
            int pathIndex = updatePathProbabilities(bottleneckList, destNode);
            pathMap[pathIndex] = pathMap[pathIndex] + remainingAmt;
            remainingAmt = 0;
        }
    }
    else {
          // normal waterfilling
          while(pq.size()>0 && remainingAmt > SMALLEST_INDIVISIBLE_UNIT){
            highestBal = get<0>(pq.top());
            highestBalIdx = get<1>(pq.top());
            pq.pop();
            
            if (pq.size()==0){
             secHighestBal=0;
            }
            else{
             secHighestBal = get<0>(pq.top());
            }
            diffToSend = highestBal - secHighestBal;

            amtToSend = min(remainingAmt/(pathMap.size()+1),diffToSend);

            for (auto p: pathMap){
                pathMap[p.first] = p.second + amtToSend;
                remainingAmt = remainingAmt - amtToSend;
            }
            pathMap[highestBalIdx] = amtToSend;
            remainingAmt = remainingAmt - amtToSend;
         }
   
           // send all of the remaining amount beyond the indivisible unit on one path
           // the highest bal path as long as it has non zero balance
           if (remainingAmt > 0 && pq.size()>0 ) {
               highestBal = get<0>(pq.top());
               highestBalIdx = get<1>(pq.top());
               
               if (highestBal > 0) {
                    pathMap[highestBalIdx] = pathMap[highestBalIdx] + remainingAmt;
                    remainingAmt = 0;
                }
            } 
            else {
               cout << "PATHS NOT FOUND to " << destNode << "WHEN IT SHOULD HAVE BEEN";
                throw std::exception();
            }
       }
 

    //Radhika: where is this check that if balance 0, don't send happening?
    // works as long as txn is sent on one path only
   if (remainingAmt == 0) {
       statRateAttempted[destNode] = statRateAttempted[destNode] + 1;
   }
  transMsg->setAmount(remainingAmt);
   
   for (auto p: pathMap){
      if (p.second > 0){
         tuple<int,int> key = make_tuple(transMsg->getTransactionId(),p.first); 
         //key is (transactionId, pathIndex)

         //update the data structure keeping track of how much sent and received on each path
         if (transPathToAckState.count(key) == 0){
            AckState temp = {};
            temp.amtSent = p.second;
            temp.amtReceived = 0;
            transPathToAckState[key] = temp;
         }
         else{
            transPathToAckState[key].amtSent =  transPathToAckState[key].amtSent + p.second;
         }

         vector<int> path = nodeToShortestPathsMap[destNode][p.first].path;
         double amt = p.second;


         routerMsg* waterMsg = generateWaterfillingTransactionMessage(amt, path, p.first, transMsg);

         if (_signalsEnabled) emit(pathPerTransPerDestSignals[destNode], p.first);
         // increment numAttempted per path
         nodeToShortestPathsMap[destNode][p.first].statRateAttempted =
            nodeToShortestPathsMap[destNode][p.first].statRateAttempted + 1;
         handleTransactionMessage(waterMsg);
         
         // incrementInFlight balance
         nodeToShortestPathsMap[destNode][p.first].sumOfTransUnitsInFlight += p.second;

      }
   }
}

//If transaction already timed-out, delete it without processing it. 
bool hostNode::handleTransactionMessageTimeOut(routerMsg* ttmsg){
   transactionMsg *transMsg = check_and_cast<transactionMsg *>(ttmsg->getEncapsulatedPacket());

   //check if transactionId is in canceledTransaction set
   int transactionId = transMsg->getTransactionId();

   auto iter = find_if(canceledTransactions.begin(),
         canceledTransactions.end(),
         [&transactionId](const tuple<int, simtime_t, int, int, int>& p)
         { return get<0>(p) == transactionId; });

   //Radhika: is the first condition needed? Isn't last condition enough?
   // Vibhaa got rid of the last condition
   //
   // this will never get hit i think
   if ( iter!=canceledTransactions.end() ){
       if (getIndex() == transMsg->getSender()) {
          statNumTimedOutAtSender[transMsg->getReceiver()] = 
              statNumTimedOutAtSender[transMsg->getReceiver()] + 1;


       }

      //delete yourself
      ttmsg->decapsulate();
      delete transMsg;
      delete ttmsg;
      return true;
   }
   else{
      return false;
   }
}

void hostNode::handleTransactionMessagePriceScheme(routerMsg* ttmsg){
   transactionMsg *transMsg = check_and_cast<transactionMsg *>(ttmsg->getEncapsulatedPacket());
   int hopcount = ttmsg->getHopCount();

   int destination = transMsg->getReceiver();
   //is a self-message/at hop count = 0
   int destNode = transMsg->getReceiver();
   int nextNode = ttmsg->getRoute()[hopcount+1];



   // first time seeing this transaction so add to d_ij computation
   // count the txn for accounting also
   if (simTime() == transMsg->getTimeSent()){
       destNodeToNumTransPending[destNode] = destNodeToNumTransPending[destNode] + 1;
       nodeToDestInfo[destNode].transSinceLastInterval += 1;
       statNumArrived[destNode] = statNumArrived[destNode]+1;
       statRateArrived[destNode] = statRateArrived[destNode]+1;
   }

   if (nodeToShortestPathsMap.count(destNode) == 0 && getIndex() == transMsg->getSender()){
      vector<vector<int>> kShortestRoutes = getKShortestRoutes(transMsg->getSender(),destNode, _kValue);
      initializePriceProbes(kShortestRoutes, destNode);
      //TODO: change to a queue implementation
      scheduleAt(simTime() + _maxTravelTime, ttmsg);
      return;
   }

   int transactionId = transMsg->getTransactionId();
   if (transMsg->getReceiver() == myIndex()) {
      int prevNode = ttmsg->getRoute()[ttmsg->getHopCount() - 1];

      map<Id, double> *incomingTransUnits = &(nodeToPaymentChannel[prevNode].incomingTransUnits);
      (*incomingTransUnits)[make_tuple(transMsg->getTransactionId(), transMsg->getHtlcIndex())] = transMsg->getAmount();
      int transactionId = transMsg->getTransactionId();
      routerMsg* newMsg =  generateAckMessage(ttmsg);

      if (_timeoutEnabled){
         //forward ack message - no need to wait;
         //check if transactionId is in canceledTransaction set
         auto iter = find_if(canceledTransactions.begin(),
               canceledTransactions.end(),
               [&transactionId](const tuple<int, simtime_t, int, int, int>& p)
               { return get<0>(p) == transactionId; });

         if (iter!=canceledTransactions.end()){
            canceledTransactions.erase(iter);
         }
      }
      forwardAckMessage(newMsg);
      return;
   }
   else if (transMsg->getSender() == myIndex()) {
    // Vibhaa : Fix this to use self-message
      //is a self-message/at hop count = 0
      int destNode = transMsg->getReceiver();

      DestInfo* destInfo = &(nodeToDestInfo[destNode]);

      //check if there are transactions queued up
      if ((destInfo->transWaitingToBeSent).size() == 0){ //none queued up
         //for each of the k paths for that destination
         for (auto p: nodeToShortestPathsMap[destNode]){
            int pathIndex = p.first;

            if (p.second.rateToSendTrans > 0 && simTime() > p.second.timeToNextSend){

               //set transaction path and update inflight on path
               ttmsg->setRoute(p.second.path);

               int nextNode = p.second.path[1];
                //transaction being sent out, need to increment nValue
               nodeToPaymentChannel[nextNode].nValue = nodeToPaymentChannel[nextNode].nValue + 1;

                // increment number of units sent to a particular destination on a particular path
                nodeToShortestPathsMap[destNode][pathIndex].nValue += 1;
               
                //generate time out message here, when path is decided
                if (_timeoutEnabled) {
                 routerMsg *toutMsg = generateTimeOutMessage(ttmsg);
                 scheduleAt(simTime() + transMsg->getTimeOut(), toutMsg );
                }
                
            // @Vibhaa: should this be calling handleTransactionMessage because there might be queues to this payment channel
            // doesn't happen typically at the end host but still
               forwardTransactionMessage(ttmsg);

               // update number attempted to this destination and on this path
              nodeToShortestPathsMap[destNode][p.first].statRateAttempted =
                nodeToShortestPathsMap[destNode][p.first].statRateAttempted + 1;
              statRateAttempted[destNode] += 1;
               
               //update "amount of transaction in flight"
               nodeToShortestPathsMap[destNode][p.first].sumOfTransUnitsInFlight =
                  nodeToShortestPathsMap[destNode][p.first].sumOfTransUnitsInFlight + transMsg->getAmount();

               //update the "time when the next transaction can be sent"
               simtime_t newTimeToNextSend =  simTime() + (transMsg->getAmount()/(p.second.rateToSendTrans));
               nodeToShortestPathsMap[destNode][p.first].timeToNextSend = newTimeToNextSend;

               return;
            }
         }

         //transaction cannot be sent on any of the paths, queue transaction
         destInfo->transWaitingToBeSent.push_back(ttmsg);
         for (auto p: nodeToShortestPathsMap[destNode]){
            PathInfo pInfo = p.second;
            if (p.second.isSendTimerSet == false){
               simtime_t timeToNextSend = pInfo.timeToNextSend;
               if (simTime()>timeToNextSend){
                  //pInfo.timeToNextSend = simTime() + 0.5;
                  timeToNextSend = simTime() + 0.001; //TODO: fix this constant
               }
               scheduleAt(timeToNextSend, pInfo.triggerTransSendMsg);
               nodeToShortestPathsMap[destNode][p.first].isSendTimerSet = true;
            }
         }

      }
      else{ //has a queue
         destInfo->transWaitingToBeSent.push_back(ttmsg);
      }
   }

}



void hostNode::handleTransactionMessageWaterfilling(routerMsg* ttmsg){
   transactionMsg *transMsg = check_and_cast<transactionMsg *>(ttmsg->getEncapsulatedPacket());

   int hopcount = ttmsg->getHopCount();
   
   //is a self-message/at hop count = 0
   int destNode = transMsg->getReceiver();
   int nextNode = ttmsg->getRoute()[hopcount+1];

   assert(destNode != myIndex()); 
   //no route is specified -
   //means we need to break up into chunks and assign route

   //If transaction seen for first time, update stats.
   if (simTime() == transMsg->getTimeSent()){ //TODO: flag for transactionMessage (isFirstTime seen)
      statNumArrived[destNode] = statNumArrived[destNode]+1;
      statRateArrived[destNode] = statRateArrived[destNode]+1;
      destNodeToNumTransPending[destNode] = destNodeToNumTransPending[destNode] + 1;
      AckState * s = new AckState();
      s->amtReceived = 0;
      s->amtSent = transMsg->getAmount();
      transToAmtLeftToComplete[transMsg->getTransactionId()] = *s;
   }

   //If destination seen for first time, compute K shortest paths and initialize probes.
   if (nodeToShortestPathsMap.count(destNode) == 0 ){
      vector<vector<int>> kShortestRoutes = getKShortestRoutes(transMsg->getSender(),destNode, _kValue);
      if (_loggingEnabled) {
         cout << "source: " << transMsg->getSender() << ", dest: " << destNode << endl;

         for (auto v: kShortestRoutes){
            cout << "route: ";
            printVector(v);
         }

         cout << "after K Shortest Routes" << endl;
      }
      initializeProbes(kShortestRoutes, destNode);

      /* schedule more probes
      for (auto i = 0; i < numProbes; i++) {
          timeBetweenProbes = _maxTravelTime / _numOutstandingProbes;
          scheduleAt(simTime + timeBetweenProbes, triggerNewProbesMsg);
      }*/
      scheduleAt(simTime() + 1, ttmsg);
      return;
   }
   else{
      bool recent = probesRecent(nodeToShortestPathsMap[destNode]);

      //if all probes from destination are recent enough, send transaction on one or more paths.
      if (recent){ // we made sure all the probes are back
         // destNodeToNumTransPending[destNode] = destNodeToNumTransPending[destNode]-1;
         if ((!_timeoutEnabled) || (simTime() < (transMsg->getTimeSent() + transMsg->getTimeOut()))) { 
             //TODO: remove?
            splitTransactionForWaterfilling(ttmsg);
            if (transMsg->getAmount()>0){
               // destNodeToNumTransPending[destNode] = destNodeToNumTransPending[destNode]+1;
               scheduleAt(simTime() + 1, ttmsg);
            }
            else{
               ttmsg->decapsulate();
               delete transMsg;
               delete ttmsg;
            }
         }
         //don't send transaction if it has timed out.
         else{
            statNumTimedOut[destNode] += 1;
            statNumTimedOutAtSender[destNode] += 1; 
            ttmsg->decapsulate();
            delete transMsg;
            delete ttmsg;
         }
         return;
      }

      else{ //if not recent
         if (destNodeToNumTransPending[destNode] > 0){
             restartProbes(destNode);
         }
         scheduleAt(simTime() + 1, ttmsg);
         return;
      }
   }
}


void hostNode::initializePathInfoLandmarkRouting(vector<vector<int>> kShortestPaths, int  destNode){ //initialize PathInfo struct: including signals
    //print all the vectors
    /*
    cout << "route start: " << endl;
    for (auto r: kShortestPaths){
        for (auto i : r) cout << i << ", ";
        cout << endl;
    }
    */

    //maybe less than k routes
      destNodeToLastMeasurementTime[destNode] = 0.0;
      nodeToShortestPathsMap[destNode] = {};
     for (int pathIdx = 0; pathIdx < kShortestPaths.size(); pathIdx++){
        //map<int, map<int, PathInfo>> nodeToShortestPathsMap;
        //Radhika TODO: PathInfo can be a struct and need not be a class.

        PathInfo temp = {};

        nodeToShortestPathsMap[destNode][pathIdx] = temp;
        nodeToShortestPathsMap[destNode][pathIdx].path = kShortestPaths[pathIdx];
        nodeToShortestPathsMap[destNode][pathIdx].probability = 1.0 / kShortestPaths.size();
        if (pathIdx == 0) {
          nodeToShortestPathsMap[destNode][pathIdx].probability = 0.8;
        }
        else {
          nodeToShortestPathsMap[destNode][pathIdx].probability = 0.2;
        }

        /* NOTE: indexing each path from source -> dest doesn't make sense anymore
         (should be by landmark, and a landmark is skipped if no route exists)
        //initialize signals
        char signalName[64];
        simsignal_t signal;
        cProperty *statisticTemplate;

        // bottleneckPerDest signal
        if (destNode<_numHostNodes){
           sprintf(signalName, "bottleneckPerDestPerPath_%d(host %d)", pathIdx, destNode);
        }
        else{
           sprintf(signalName, "bottleneckPerDestPerPath_%d(router %d [%d] )", pathIdx, destNode - _numHostNodes, destNode);
        }
        signal = registerSignal(signalName);
        statisticTemplate = getProperties()->get("statisticTemplate", "bottleneckPerDestPerPathTemplate");
        getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
        nodeToShortestPathsMap[destNode][pathIdx].bottleneckPerDestPerPathSignal = signal;

        if (destNode<_numHostNodes){
           sprintf(signalName, "probeBackPerDestPerPath_%d(host %d)", pathIdx, destNode);
        }
        else{
           sprintf(signalName, "probeBackPerDestPerPath_%d(router %d [%d] )", pathIdx, destNode - _numHostNodes, destNode);
        }
        signal = registerSignal(signalName);
        statisticTemplate = getProperties()->get("statisticTemplate", "probeBackPerDestPerPathTemplate");
        getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
        nodeToShortestPathsMap[destNode][pathIdx].probeBackPerDestPerPathSignal = signal;

        if (destNode<_numHostNodes){
           sprintf(signalName, "rateCompletedPerDestPerPath_%d(host %d)", pathIdx, destNode);
        }
        else{
           sprintf(signalName, "rateCompletedPerDestPerPath_%d(router %d [%d] )", pathIdx, destNode - _numHostNodes, destNode);
        }

        signal = registerSignal(signalName);
        statisticTemplate = getProperties()->get("statisticTemplate", "rateCompletedPerDestPerPathTemplate");
        getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
        nodeToShortestPathsMap[destNode][pathIdx].rateCompletedPerDestPerPathSignal = signal;

        // probabilityPerDest signal
        if (destNode<_numHostNodes){
           sprintf(signalName, "probabilityPerDestPerPath_%d(host %d)", pathIdx, destNode);
        }
        else{
           sprintf(signalName, "probabilityPerDestPerPath_%d(router %d [%d] )", pathIdx, destNode - _numHostNodes, destNode);
        }
        signal = registerSignal(signalName);
        statisticTemplate = getProperties()->get("statisticTemplate", "probabilityPerDestPerPathTemplate");
        getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
        nodeToShortestPathsMap[destNode][pathIdx].probabilityPerDestPerPathSignal = signal;


        if (destNode<_numHostNodes){
           sprintf(signalName, "rateAttemptedPerDestPerPath_%d(host %d)", pathIdx, destNode);
        }
        else{
           sprintf(signalName, "rateAttemptedPerDestPerPath_%d(router %d [%d] )", pathIdx, destNode - _numHostNodes, destNode);
        }

        signal = registerSignal(signalName);
        statisticTemplate = getProperties()->get("statisticTemplate", "rateAttemptedPerDestPerPathTemplate");
        getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
        nodeToShortestPathsMap[destNode][pathIdx].rateAttemptedPerDestPerPathSignal = signal;

        routerMsg * msg = generateProbeMessage(destNode, pathIdx, kShortestPaths[pathIdx]);
        nodeToShortestPathsMap[destNode][pathIdx].isProbeOutstanding = true;
        forwardProbeMessage(msg);
        */
     }
    return;
}


void hostNode::initializeLandmarkRoutingProbes(routerMsg * msg, int transactionId, int destNode){

    ProbeInfo probeInfoTemp =  {};
    probeInfoTemp.messageToSend = msg; //message to send out once all probes return
    probeInfoTemp.probeReturnTimes = {}; //probeReturnTimes[0] is return time of first probe

    for (auto pTemp: nodeToShortestPathsMap[destNode]){
        PathInfo pInfo = pTemp.second;
        vector<int> path = pInfo.path;
        routerMsg * rMsg = generateProbeMessage(destNode , pTemp.first, path);
        //set the transactionId in the generated message
        probeMsg *pMsg = check_and_cast<probeMsg *>(rMsg->getEncapsulatedPacket());
        pMsg->setTransactionId(transactionId);
        forwardProbeMessage(rMsg);

        probeInfoTemp.probeReturnTimes.push_back(-1);
        probeInfoTemp.probeBottlenecks.push_back(-1);


    }
    probeInfoTemp.numProbesWaiting = nodeToShortestPathsMap[destNode].size();
    transactionIdToProbeInfoMap[transactionId] = probeInfoTemp;
    return;
}

void hostNode::handleTransactionMessageLandmarkRouting(routerMsg* ttmsg){
   transactionMsg *transMsg = check_and_cast<transactionMsg *>(ttmsg->getEncapsulatedPacket());

   int hopcount = ttmsg->getHopCount();
   vector<tuple<int, double , routerMsg *, Id>> *q;

   int destNode = transMsg->getReceiver();
   int destination = destNode;

   if (ttmsg->getRoute().size() == 0){ //route not yet set, first time we're seeing it

       // in waterfilling message can be received multiple times, so only update when simTime == transTime
      statNumArrived[destination] = statNumArrived[destination] + 1;
      statRateArrived[destination] = statRateArrived[destination] + 1;
      statRateAttempted[destination] = statRateAttempted[destination] + 1;

      //If destination seen for first time, compute K shortest paths and initialize probes.
        if (nodeToShortestPathsMap.count(destNode) == 0 ){

            //cout << "calculate new routes" << endl;
           vector<vector<int>> kShortestRoutes = getKShortestRoutesLandmarkRouting(transMsg->getSender(),destNode, _kValue);
           initializePathInfoLandmarkRouting(kShortestRoutes, destNode); //initialize PathInfo struct: including signals
        }

           initializeLandmarkRoutingProbes(ttmsg, transMsg->getTransactionId(), destNode ); //parameters: routerMsg* ttmsg, int destNode

   }
   else if(ttmsg->getRoute().size() > 0 && ttmsg->getHopCount() ==  ttmsg->getRoute().size()-1){ // are at last index of route
      int prevNode = ttmsg->getRoute()[ttmsg->getHopCount()-1];
      map<Id, double> *incomingTransUnits = &(nodeToPaymentChannel[prevNode].incomingTransUnits);
      (*incomingTransUnits)[make_tuple(transMsg->getTransactionId(), transMsg->getHtlcIndex())] = transMsg->getAmount();
      int transactionId = transMsg->getTransactionId();
      routerMsg* newMsg =  generateAckMessage(ttmsg);

      forwardAckMessage(newMsg);
      return;
   }
   else if(ttmsg->getRoute().size() > 0 && ttmsg->getHopCount() == 0){

      //is a self-message/at hop count = 0
      int destNode = transMsg->getReceiver();
      int nextNode = ttmsg->getRoute()[hopcount+1];
      q = &(nodeToPaymentChannel[nextNode].queuedTransUnits);
      tuple<int,int > key = make_tuple(transMsg->getTransactionId(), transMsg->getHtlcIndex());

      if (_hasQueueCapacity && _queueCapacity <= (*q).size()){ //failed transaction, queue at capacity
          if (forwardTransactionMessage(ttmsg) == false){ //attempt to send - important if queue size is 0

              routerMsg * failedAckMsg = generateAckMessage(ttmsg, false);
                      handleAckMessage(failedAckMsg);
          }
          else{

          }
      }
      else{

         (*q).push_back(make_tuple(transMsg->getPriorityClass(), transMsg->getAmount(),
                  ttmsg, key));
         push_heap((*q).begin(), (*q).end(), sortPriorityThenAmtFunction);
         processTransUnits(nextNode, *q);
      }
   }
   else{
       cout << "should never be here in handleTransactionMessageLandmarkRouting" << endl;
   }
}




/* handleTransactionMessage - checks if message has arrived
 *      1. has arrived - turn transactionMsg into ackMsg, forward ackMsg
 *      2. has not arrived - add to appropriate job queue q, process q as
 *          much as we have funds for
 */
void hostNode::handleTransactionMessage(routerMsg* ttmsg){
   transactionMsg *transMsg = check_and_cast<transactionMsg *>(ttmsg->getEncapsulatedPacket());

   int hopcount = ttmsg->getHopCount();
   vector<tuple<int, double , routerMsg *, Id>> *q;

   int destination = transMsg->getReceiver();
   if (!_waterfillingEnabled && !_priceSchemeEnabled){
       // in waterfilling and price scheme message can be received multiple times, so only update when simTime == transTime
      statRateArrived[destination] = statRateArrived[destination] + 1;
      statRateAttempted[destination] = statRateAttempted[destination] + 1;
   }
   
   //check if transactionId is in canceledTransaction set
   int transactionId = transMsg->getTransactionId();
   if(ttmsg->getHopCount() ==  ttmsg->getRoute().size()-1){ // are at last index of route
      int prevNode = ttmsg->getRoute()[ttmsg->getHopCount()-1];
      map<Id, double> *incomingTransUnits = &(nodeToPaymentChannel[prevNode].incomingTransUnits);
      (*incomingTransUnits)[make_tuple(transMsg->getTransactionId(), transMsg->getHtlcIndex())] = transMsg->getAmount();
      int transactionId = transMsg->getTransactionId();
      routerMsg* newMsg =  generateAckMessage(ttmsg);

      if (_timeoutEnabled){
         //forward ack message - no need to wait;
         //check if transactionId is in canceledTransaction set
         //Radhika: what if we have multiple HTLC's per transaction id? Why is this check needed?
         auto iter = find_if(canceledTransactions.begin(),
               canceledTransactions.end(),
               [&transactionId](const tuple<int, simtime_t, int, int, int>& p)
               { return get<0>(p) == transactionId; });

         if (iter!=canceledTransactions.end()){
            canceledTransactions.erase(iter);
         }
      }

      forwardAckMessage(newMsg);
      return;
   }
   else{

      //is a self-message/at hop count = 0
      int destNode = transMsg->getReceiver();
      int nextNode = ttmsg->getRoute()[hopcount+1];

	 		//cout << "Transaction arrived: " << simTime() << " " << myIndex() << " " << destNode	<< endl;

      q = &(nodeToPaymentChannel[nextNode].queuedTransUnits);
      tuple<int,int > key = make_tuple(transMsg->getTransactionId(), transMsg->getHtlcIndex());

      if (_hasQueueCapacity && _queueCapacity == 0) {
          if (forwardTransactionMessage(ttmsg) == false) {
              // if there isn't balance, because cancelled txn case will never be hit
              // TODO: make this and forward txn message cleaner
              // maybe just clean out queue when a timeout arrives as opposed to after clear state
             routerMsg * failedAckMsg = generateAckMessage(ttmsg, false);
             handleAckMessage(failedAckMsg);
          }
      }
      else if (_hasQueueCapacity && (*q).size() >= _queueCapacity){ 
          //failed transaction, queue at capacity and others are queued so don't send this transaction even
         routerMsg * failedAckMsg = generateAckMessage(ttmsg, false);
         handleAckMessage(failedAckMsg);
      }
      else{
          // add to queue and process in order of queue
         (*q).push_back(make_tuple(transMsg->getPriorityClass(), transMsg->getAmount(),
                  ttmsg, key));
         push_heap((*q).begin(), (*q).end(), sortPriorityThenAmtFunction);
         processTransUnits(nextNode, *q);
      }
   }
}

routerMsg *hostNode::generateTimeOutMessage(routerMsg* msg)
{
   transactionMsg *transMsg = check_and_cast<transactionMsg *>(msg->getEncapsulatedPacket());
   char msgname[MSGSIZE];

   sprintf(msgname, "tic-%d-to-%d timeOutMsg", transMsg->getSender(), transMsg->getReceiver());
   timeOutMsg *toutMsg = new timeOutMsg(msgname);
   toutMsg->setTransactionId(transMsg->getTransactionId());
   toutMsg->setReceiver(transMsg->getReceiver());

   sprintf(msgname, "tic-%d-to-%d routerTimeOutMsg(%f)", transMsg->getSender(), transMsg->getReceiver(), transMsg->getTimeSent());
   routerMsg *rMsg = new routerMsg(msgname);
   rMsg->setRoute(msg->getRoute());
   rMsg->setHopCount(0);
   rMsg->setMessageType(TIME_OUT_MSG);
   rMsg->encapsulate(toutMsg);
   return rMsg;
}


/* generateTransactionMessage - takes in a TransUnit object and returns corresponding routerMsg message
 *      with encapsulated transactionMsg inside.
 *      note: calls get_route function to get route from sender to receiver
 */
routerMsg *hostNode::generateTransactionMessage(TransUnit unit)
{
   char msgname[MSGSIZE];
   sprintf(msgname, "tic-%d-to-%d transactionMsg", unit.sender, unit.receiver);
   transactionMsg *msg = new transactionMsg(msgname);
   msg->setAmount(unit.amount);
   msg->setTimeSent(unit.timeSent);
   msg->setSender(unit.sender);
   msg->setReceiver(unit.receiver);
   msg->setPriorityClass(unit.priorityClass);
   msg->setTransactionId(msg->getId());
   msg->setHtlcIndex(0);
   msg->setHasTimeOut(unit.hasTimeOut);
   msg->setTimeOut(unit.timeOut);
   sprintf(msgname, "tic-%d-to-%d router-transaction-Msg", unit.sender, unit.receiver);
   routerMsg *rMsg = new routerMsg(msgname);
   if (destNodeToPath.count(unit.receiver) == 0){ //compute route and add to memoization table
      vector<int> route = getRoute(unit.sender,unit.receiver);
      destNodeToPath[unit.receiver] = route;
      rMsg->setRoute(route);
   }
   else{
      rMsg->setRoute(destNodeToPath[unit.receiver]);
   }
   rMsg->setHopCount(0);
   rMsg->setMessageType(TRANSACTION_MSG);
   rMsg->encapsulate(msg);
   return rMsg;
}

routerMsg *hostNode::generateTriggerTransactionSendMessage(vector<int> path, int pathIndex, int destNode)
{
   char msgname[MSGSIZE];
   sprintf(msgname, "tic-%d-to-%d transactionSendMsg", myIndex(), destNode);
   transactionSendMsg *tsMsg = new transactionSendMsg(msgname);
   tsMsg->setPathIndex(pathIndex);
   tsMsg->setTransactionPath(path);
   tsMsg->setReceiver(destNode);

   routerMsg *rMsg = new routerMsg(msgname);
   rMsg->setMessageType(TRIGGER_TRANSACTION_SEND_MSG);
   rMsg->encapsulate(tsMsg);
   return rMsg;
}


/*
 * processTransUnits - given an adjacent node, and TransUnit queue of things to send to that node, sends
 *  TransUnits until channel funds are too low by calling forwardMessage on message representing TransUnit
 */
void hostNode:: processTransUnits(int dest, vector<tuple<int, double , routerMsg *, Id>>& q){
   bool successful = true;
   while((int)q.size()>0 && successful){
      successful = forwardTransactionMessage(get<2>(q.back()));
      if (successful){
         q.pop_back();
      }
   }
}

/*
 *  forwardTransactionMessage - given a message representing a TransUnit, increments hopCount, finds next destination,
 *      adjusts (decrements) channel balance, sends message to next node on route
 */
bool hostNode::forwardTransactionMessage(routerMsg *msg)
{
   transactionMsg *transMsg = check_and_cast<transactionMsg *>(msg->getEncapsulatedPacket());
   int nextDest = msg->getRoute()[msg->getHopCount()+1];
   int transactionId = transMsg->getTransactionId();

   if (nodeToPaymentChannel[nextDest].balance <= 0 || transMsg->getAmount() >nodeToPaymentChannel[nextDest].balance){
       cout << "transMsg->getAmount():" <<  transMsg->getAmount() << endl;
       cout << "nodeToPaymentChannel[nextDest].balance:" << nodeToPaymentChannel[nextDest].balance << endl;
      return false;
   }

   else{

      // check if txn has been cancelled and if so return without doing anything to move on to the next transaction
      // TODO: this case won't be hit for price scheme because you check before calling this function even
      int transactionId = transMsg->getTransactionId();
      auto iter = find_if(canceledTransactions.begin(),
           canceledTransactions.end(),
           [&transactionId](const tuple<int, simtime_t, int, int, int>& p)
           { return get<0>(p) == transactionId; });

      // can potentially erase info?
      if (iter != canceledTransactions.end()) {
         return true;
      }

      // Increment hop count.
      msg->setHopCount(msg->getHopCount()+1);
      //use hopCount to find next destination

      //add amount to outgoing map
      map<Id, double> *outgoingTransUnits = &(nodeToPaymentChannel[nextDest].outgoingTransUnits);
      (*outgoingTransUnits)[make_tuple(transMsg->getTransactionId(),transMsg->getHtlcIndex())] = transMsg->getAmount();

      //numSentPerChannel incremented every time (key,value) pair added to outgoing_trans_units map
      nodeToPaymentChannel[nextDest].statNumSent =  nodeToPaymentChannel[nextDest].statNumSent+1;
      int amt = transMsg->getAmount();

      double newBalance = nodeToPaymentChannel[nextDest].balance - amt;
      nodeToPaymentChannel[nextDest].balance = newBalance;
      nodeToPaymentChannel[nextDest].balanceEWMA = 
          (1 -_ewmaFactor) * nodeToPaymentChannel[nextDest].balanceEWMA + (_ewmaFactor) * newBalance;

      //int transId = transMsg->getTransactionId();
      if (_loggingEnabled) cout << "forwardTransactionMsg send: " << simTime() << endl;
      send(msg, nodeToPaymentChannel[nextDest].gate);
      return true;
   } //end else (transMsg->getAmount() >nodeToPaymentChannel[dest].balance)
}

