#include "hostNode.h"
#include <queue>
#include "hostInitialize.h"

//global parameters
map<int, vector<TransUnit>> _transUnitList; //list of all transUnits
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
bool _timeoutEnabled;
bool _loggingEnabled;
bool _signalsEnabled;
double _ewmaFactor;

//global parameters for price scheme
bool _priceSchemeEnabled;
double _eta; //for price computation
double _kappa; //for price computation
double _tUpdate; //for triggering price updates at routers
double _tQuery; //for triggering price query probes at hosts
double _alpha;

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
            temp!= (nodeToPaymentChannel[key].queuedTransUnits).end(); temp++){
         routerMsg * rMsg = get<2>(*temp);
         auto tMsg = rMsg->getEncapsulatedPacket();
         rMsg->decapsulate();
         delete tMsg;
         delete rMsg;
      }
   }
}


routerMsg * hostNode::generatePriceQueryMessage(vector<int> path, int pathIdx){
   //cout << "generatePriceQuery path:" ;
   //printVector(path);
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
      (*pathBalances).push_back(nodeToPaymentChannel[nextDest].balance);
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

      PathInfo * p = &(nodeToShortestPathsMap[destNode][pathIdx]);
      assert(p->path == pMsg->getPath());
      p->lastUpdated = simTime();
      vector<double> pathBalances = pMsg->getPathBalances();
      int bottleneck = minVectorElemDouble(pathBalances);
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
      routerMsg * msg = generateProbeMessage(destNode, p.first, p.second.path);
      forwardProbeMessage(msg);
   }

}


void hostNode::initializeProbes(vector<vector<int>> kShortestPaths, int destNode){ 
    //maybe less than k routes

   for (int pathIdx = 0; pathIdx < kShortestPaths.size(); pathIdx++){
      //map<int, map<int, PathInfo>> nodeToShortestPathsMap;
      //Radhika TODO: PathInfo can be a struct and need not be a class. 
      PathInfo temp = {};
      nodeToShortestPathsMap[destNode][pathIdx] = temp;
      nodeToShortestPathsMap[destNode][pathIdx].path = kShortestPaths[pathIdx];

      //initialize signals
      char signalName[64];
      simsignal_t signal;
      cProperty *statisticTemplate;

      // numInQueuePerChannel signal
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
      forwardProbeMessage(msg);
   }
}



void hostNode::initializePriceProbes(vector<vector<int>> kShortestPaths, int destNode){ //maybe less than k routes

   //cout << "initPriceProbes1" << endl;
   for (int pathIdx = 0; pathIdx < kShortestPaths.size(); pathIdx++){
      //map<int, map<int, PathInfo>> nodeToShortestPathsMap;
      PathInfo temp = {};
      nodeToShortestPathsMap[destNode][pathIdx] = temp;
      nodeToShortestPathsMap[destNode][pathIdx].path = kShortestPaths[pathIdx];
      cout << "signal1" << endl;
      routerMsg * triggerTransSendMsg = generateTriggerTransactionSendMessage(kShortestPaths[pathIdx], pathIdx, destNode);
      nodeToShortestPathsMap[destNode][pathIdx].triggerTransSendMsg = triggerTransSendMsg;
      //cout << "initPriceProbes2" << endl;
      //initialize signals
      cout << "signal2" << endl;
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
      cout << "signal3" << endl;
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
      cout << "signal5" << endl;
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

      cout << "signal5.1" << endl;
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
      cout << "signal5.2" << endl;
      signal = registerSignal(signalName);
      statisticTemplate = getProperties()->get("statisticTemplate", "sumOfTransUnitsInFlightPerPathTemplate");
      getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
      nodeToShortestPathsMap[destNode][pathIdx].sumOfTransUnitsInFlightSignal = signal;
      cout << "signal5.3" << endl;
      //priceLastUpdatedPerPath signal
      if (destNode<_numHostNodes){
         sprintf(signalName, "priceLastUpdatedPerPath_%d(host %d)", pathIdx, destNode);
      }
      else{
         sprintf(signalName, "priceLastUpdatedPerPath_%d(router %d [%d] )", pathIdx, destNode - _numHostNodes, destNode);
      }
      cout << "signal5.8" << endl;
      signal = registerSignal(signalName);
      statisticTemplate = getProperties()->get("statisticTemplate", "priceLastUpdatedPerPathTemplate");
      getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
      nodeToShortestPathsMap[destNode][pathIdx].priceLastUpdatedSignal = signal;

      //isSendTimerSetPerPath signal
      if (destNode<_numHostNodes){
         sprintf(signalName, "isSendTimerSetPerPath_%d(host %d)", pathIdx, destNode);
      }
      else{
         sprintf(signalName, "isSendTimerSetPerPath_%d(router %d [%d] )", pathIdx, destNode - _numHostNodes, destNode);
      }
      cout << "signal5.9" << endl;
      signal = registerSignal(signalName);
      statisticTemplate = getProperties()->get("statisticTemplate", "isSendTimerSetPerPathTemplate");
      getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
      nodeToShortestPathsMap[destNode][pathIdx].isSendTimerSetSignal = signal;
      cout << "signal6" << endl;

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
      //set statRate
      _statRate = par("statRate");
      _clearRate = par("timeoutClearRate");
      _waterfillingEnabled = par("waterfillingEnabled");
      _timeoutEnabled = par("timeoutEnabled");
      _signalsEnabled = par("signalsEnabled");
      _loggingEnabled = par("loggingEnabled");
      _eta = 0.5;
      _kappa = 0.5;
      _tUpdate = 0.5;
      _priceSchemeEnabled = par("priceSchemeEnabled");

      _ewmaFactor = 0.8; // EWMA factor for balance information on probes

      if (_waterfillingEnabled || _priceSchemeEnabled){
         _kValue = par("numPathChoices");
      }

      _maxTravelTime = 0.0; 

      setNumNodes(topologyFile_);
      // add all the TransUnits into global list
      generateTransUnitList(workloadFile_);

      //create "channels" map - from topology file
      //create "balances" map - from topology file
      generateChannelsBalancesMap(topologyFile_);
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

         //nValueSignal

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

         //balancePerChannel signal

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

         //balancePerChannel signal

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


         //balancePerChannel signal

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

         //balancePerChannel signal

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

      //pathPerTransPerDest signal
      sprintf(signalName, "pathPerTransPerDest(host node %d)", i);
      signal = registerSignal(signalName);
      statisticTemplate = getProperties()->get("statisticTemplate", "pathPerTransPerDestTemplate");
      getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
      pathPerTransPerDestSignals.push_back(signal);


      //numtimedoutperdest signal
      sprintf(signalName, "numTimedOutPerDest_total(host node %d)", i);
      signal = registerSignal(signalName);
      statisticTemplate = getProperties()->get("statisticTemplate", "numTimedOutPerDestTemplate");
      getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
      numTimedOutPerDestSignals.push_back(signal);
      statNumTimedOut.push_back(0);

      //numPendingPerDest signal
      sprintf(signalName, "numPendingPerDest_total(host node %d)", i);
      signal = registerSignal(signalName);
      statisticTemplate = getProperties()->get("statisticTemplate", "numPendingPerDestTemplate");
      getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
      numPendingPerDestSignals.push_back(signal);

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

   //implementing timeSent parameter, send all msgs at beginning
   for (int i=0; i<(int)(_transUnitList[myIndex()].size()); i++) {

      TransUnit j = _transUnitList[myIndex()][i];
      double timeSent = j.timeSent;
      routerMsg *msg = generateTransactionMessage(j); //TODO: flag to whether to calculate path

      if (_waterfillingEnabled || _priceSchemeEnabled){
         vector<int> blankPath = {};
         //Radhika TODO: maybe no need to compute path to begin with?
         msg->setRoute(blankPath);
      }

      scheduleAt(timeSent, msg);

      if (_timeoutEnabled && !_priceSchemeEnabled && j.hasTimeOut){
         routerMsg *toutMsg = generateTimeOutMessage(msg);
         scheduleAt(timeSent + j.timeOut, toutMsg );
      }
   }

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
      //print  map<int, int> transactionIdToNumHtlc
      int splitTrans = 0;
      for (auto p: transactionIdToNumHtlc){
         if (p.second>1) splitTrans++;
      }
      if (_loggingEnabled) {
         cout << endl;
         cout << "number greater than 1: " << splitTrans << endl;
      }

      cout << "maxTravelTime:" << _maxTravelTime << endl;
      endSimulation();

      if (ttmsg->getMessageType() == TRANSACTION_MSG){
         auto tempMsg = ttmsg->getEncapsulatedPacket();
         ttmsg->decapsulate();
         delete tempMsg;
         delete ttmsg;
         return;
      }
   }

   /*if (printNodeToPaymentChannel() == false){
     endSimulation();
     }
    */

   if (ttmsg->getMessageType()==ACK_MSG){ //preprocessor constants defined in routerMsg_m.h
      if (_loggingEnabled) cout << "[HOST "<< myIndex() <<": RECEIVED ACK MSG] " << msg->getName() << endl;
      //print_message(ttmsg);
      //print_private_values();
      if (_timeoutEnabled){
         if(_loggingEnabled) cout << "handleAckMessageTimeOut" << endl;
         handleAckMessageTimeOut(ttmsg);
      }
      if (_waterfillingEnabled){
         handleAckMessageWaterfilling(ttmsg);
      }
      else if (_priceSchemeEnabled){ //includes _priceSchemeEnabled case

         handleAckMessagePriceScheme(ttmsg);
      }
      else{
         if(_loggingEnabled) cout << "handleAckMessageShortestPath" << endl;
         handleAckMessageShortestPath(ttmsg);
      }
      handleAckMessage(ttmsg);
      if (_loggingEnabled) cout << "[AFTER HANDLING:]" <<endl;
      // print_private_values();
   }
   else if(ttmsg->getMessageType()==TRANSACTION_MSG){
      if (_loggingEnabled) cout<< "[HOST "<< myIndex() <<": RECEIVED TRANSACTION MSG]  "<< msg->getName() <<endl;
      // print_message(ttmsg);
      // print_private_values();
      cout << "handleTrans1" << endl;
      if (_timeoutEnabled && handleTransactionMessageTimeOut(ttmsg)){
         //note: handleTransactionMessageTimeOut(ttmsg) returns true if message was deleted
         cout << "handleTrans2" << endl;
         return;
      }
      cout << "handleTrans3" << endl;
      if (_waterfillingEnabled && (ttmsg->getRoute().size() == 0)){
         cout << "handleTrans4" << endl;
         handleTransactionMessageWaterfilling(ttmsg);
      }
      else if (_priceSchemeEnabled){
         cout << "handleTrans5" << endl;
         handleTransactionMessagePriceScheme(ttmsg);
         cout << "handleTrans6" << endl;
      }
      else{
         handleTransactionMessage(ttmsg);
      }

      if (_loggingEnabled) cout<< "[AFTER HANDLING:] "<<endl;
      // print_private_values();
   }
   else if(ttmsg->getMessageType()==UPDATE_MSG){
      if (_loggingEnabled) cout<< "[HOST "<< myIndex() <<": RECEIVED UPDATE MSG] "<< msg->getName() << endl;
      // print_message(ttmsg);
      // print_private_values();
      handleUpdateMessage(ttmsg);
      if (_loggingEnabled) cout<< "[AFTER HANDLING:]  "<<endl;
      //print_private_values();

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
      handleProbeMessage(ttmsg);
      if (_loggingEnabled) cout<< "[AFTER HANDLING:]  "<<endl;
   }
   else if (ttmsg->getMessageType() == CLEAR_STATE_MSG){
      if (_loggingEnabled) cout<< "[HOST "<< myIndex() <<": RECEIVED CLEAR_STATE_MSG] " << msg->getName() << endl;
      if (_priceSchemeEnabled){
         handleClearStateMessagePriceScheme(ttmsg); //clears the transactions queued in nodeToDestInfo
         //TODO: need to write function
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
   /*
      if (printNodeToPaymentChannel() == false){
      endSimulation();
      }
    */
}


void hostNode::handleClearStateMessagePriceScheme(routerMsg *msg){

   return;
}

void hostNode::handleTriggerTransactionSendMessage(routerMsg* ttmsg){
   transactionSendMsg *tsMsg = check_and_cast<transactionSendMsg *>(ttmsg->getEncapsulatedPacket());

   vector<int> path = tsMsg->getTransactionPath();
   //cout << "send transaction on path:" << endl;
   printVector(path);
   int pathIndex = tsMsg->getPathIndex();
   int destNode = tsMsg->getReceiver();

   if (nodeToDestInfo[destNode].transWaitingToBeSent.size() > 0){ //has trans to send
      //remove the transaction $tu$ at the head of the queue


      routerMsg * msgToSend = nodeToDestInfo[destNode].transWaitingToBeSent.front();
      nodeToDestInfo[destNode].transWaitingToBeSent.pop_front();
      transactionMsg *transMsg = check_and_cast<transactionMsg *>(msgToSend->getEncapsulatedPacket());
      //Send the transaction $tu$ along the corresponding path.
      while(simTime() > transMsg->getTimeSent() + transMsg->getTimeOut() ){
         msgToSend->decapsulate();
         delete msgToSend;
         delete transMsg;

         msgToSend = nodeToDestInfo[destNode].transWaitingToBeSent.front();
         nodeToDestInfo[destNode].transWaitingToBeSent.pop_front();
         transMsg = check_and_cast<transactionMsg *>(msgToSend->getEncapsulatedPacket());

      }
      transMsg->setPathIndex(pathIndex);
      msgToSend->setRoute(path);
      msgToSend->setHopCount(0);
      //generate time out message here, when path is decided
      routerMsg *toutMsg = generateTimeOutMessage(msgToSend);
      forwardTransactionMessage(msgToSend);



      //cout << "flag1" << endl;
      //TODO: clear state needs to clear destNodeToTransWaiting queues, so this will never be scheduled to the past?
      scheduleAt(transMsg->getTimeSent() + transMsg->getTimeOut(), toutMsg );

      //cout << "flag2" << endl;

      //Update the  “time when next transaction can be sent” to (“current time” +
      //(“amount in the transaction $tu$ that is currently being sent” / “rate”)).
      double rateToSendTrans = nodeToShortestPathsMap[destNode][pathIndex].rateToSendTrans;
      if (rateToSendTrans == 0){
         //nodeToShortestPathsMap[destNode][pathIndex].rateToSendTrans = 0.5;
         rateToSendTrans = 0.5; //TODO: fix constant
      }
      nodeToShortestPathsMap[destNode][pathIndex].timeToNextSend =
         simTime() + transMsg->getAmount()/rateToSendTrans;

      //If there are more transactions queued up, set a timer to send the “trigger
      //sending transaction” self-message at the “time when next transaction can
      //be sent” with this path and destination identifiers.
      if (nodeToDestInfo[destNode].transWaitingToBeSent.size() > 0){
         scheduleAt(nodeToShortestPathsMap[destNode][pathIndex].timeToNextSend, ttmsg);
      }
   }
   else{ //no trans to send
      nodeToShortestPathsMap[destNode][pathIndex].isSendTimerSet = false;
   }
}



void hostNode::handlePriceQueryMessage(routerMsg* ttmsg){

   priceQueryMsg *pqMsg = check_and_cast<priceQueryMsg *>(ttmsg->getEncapsulatedPacket());
   //cout << "handlePriceQueryMsg route: ";
   //printVector(ttmsg->getRoute());
   //cout << "myIndex: " << myIndex() << endl;

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
      double zValue = pqMsg->getZValue();
      int destNode = ttmsg->getRoute()[0];
      int routeIndex = pqMsg->getPathIndex();
      double oldRate = nodeToShortestPathsMap[destNode][routeIndex].rateToSendTrans;
      nodeToShortestPathsMap[destNode][routeIndex].rateToSendTrans =
         maxDouble(oldRate + _alpha*(1-zValue), 0);

      nodeToShortestPathsMap[destNode][routeIndex].priceLastUpdated = simTime();
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

   nodeToPaymentChannel[sender].lambda = maxDouble(oldLambda + _eta*(xLocal + xRemote - (cValue/_maxTravelTime)),0);
   nodeToPaymentChannel[sender].muLocal = maxDouble(oldMuLocal + _kappa*(xLocal - xRemote) , 0);
   nodeToPaymentChannel[sender].muRemote = maxDouble(oldMuRemote + _kappa*(xRemote - xLocal) , 0);

}

void hostNode::handleTriggerPriceQueryMessage(routerMsg* ttmsg){
   if (simTime() > _simulationLength){
      delete ttmsg;
   }
   else{
      scheduleAt(simTime()+_tUpdate, ttmsg);
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
            //cout << "destNode: " << it->first << endl;
            //cout << "vector to generate new generate price query message: ";
            //printVector(pInfo.path);

            routerMsg * msg = generatePriceQueryMessage(pInfo.path, routeIndex);// TODO: adjust parameters
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

      if (simTime() > (msgArrivalTime + _maxTravelTime)){ //we can process it

         if (nextNode != -1){
            vector<tuple<int, double, routerMsg*, Id>>* queuedTransUnits = &(nodeToPaymentChannel[nextNode].queuedTransUnits);
            //sort_heap((*queuedTransUnits).begin(), (*queuedTransUnits).end());

            auto iterQueue = find_if((*queuedTransUnits).begin(),
                  (*queuedTransUnits).end(),
                  [&transactionId](const tuple<int, double, routerMsg*, Id>& p)
                  { return (get<0>(get<3>(p)) == transactionId); });
            while (iterQueue != (*queuedTransUnits).end()){
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


            // start queue searching


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
                  emit(pInfo->priceLastUpdatedSignal, pInfo->priceLastUpdated);
                  emit(pInfo->isSendTimerSetSignal, pInfo->isSendTimerSet);

               }
            }
         }//end if (nodeToShortestPathsMap.count(it) > 0)
      }//end for
   } // end if (_loggingEnabled)
}




void hostNode::handleStatMessage(routerMsg* ttmsg){
   if (simTime() > _simulationLength){
      delete ttmsg;
      deleteMessagesInQueues();
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
               if (_signalsEnabled) emit(p.second.bottleneckPerDestPerPathSignal, p.second.bottleneck);

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
            emit(numCompletedPerDestSignals[it], statNumCompleted[it]);

            emit(numTimedOutPerDestSignals[it], statNumTimedOut[it]);
            emit(numTimedOutAtSenderSignals[it], statNumTimedOutAtSender[it]);
            emit(numPendingPerDestSignals[it], destNodeToNumTransPending[it]);

            int frac = ((100*statNumCompleted[it])/(maxTwoNum(statNumArrived[it],1)));
            emit(fracSuccessfulPerDestSignals[it],frac);

         }

      }//end if
   }//end for
}

void hostNode::forwardTimeOutMessage(routerMsg* msg){
   // Increment hop count.
   //cout << "forwardTimeOutMessage" << endl;
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
            CanceledTrans ct = make_tuple(toutMsg->getTransactionId(),simTime(),-1, nextNode);
            canceledTransactions.insert(ct);

            statNumTimedOut[destination] = statNumTimedOut[destination]  + 1;
            forwardTimeOutMessage(waterTimeOutMsg);


         }
         //TODO: HandleClearStateMessage to remove this key
      }


      delete ttmsg;
   }
   else{

      CanceledTrans ct = make_tuple(toutMsg->getTransactionId(),simTime(),(ttmsg->getRoute())[ttmsg->getHopCount()-1],-1);
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

   //cout << "timeout1" << endl;
   timeOutMsg *toutMsg = check_and_cast<timeOutMsg *>(ttmsg->getEncapsulatedPacket());
   //cout << "timeout1.2" << endl;
   int destination = toutMsg->getReceiver();

   //cout << "timeout1.3" << endl;

   if ((ttmsg->getHopCount())==0){ //is at the sender
      //cout << "timeout1.4" << endl;

      if (successfulDoNotSendTimeOut.count(toutMsg->getTransactionId())>0){ //already received ack for it, do not send out
         //cout << "timeout1.5" << endl;
         successfulDoNotSendTimeOut.erase(toutMsg->getTransactionId());


         ttmsg->decapsulate();
         delete toutMsg;
         delete ttmsg;
      }
      else{
         //cout << "timeout1.6" << endl;
         //cout << "route: ";
         printVector(ttmsg->getRoute());
         //cout << "hopCount: " << ttmsg->getHopCount() << endl;

         int nextNode = (ttmsg->getRoute())[ttmsg->getHopCount()+1];
         //cout << "timeout1.61" << endl;
         CanceledTrans ct = make_tuple(toutMsg->getTransactionId(),simTime(),-1, nextNode);
         canceledTransactions.insert(ct);
         statNumTimedOut[destination] = statNumTimedOut[destination]  + 1;
         //cout << "timeout1.62" << endl;
         forwardTimeOutMessage(ttmsg);
      }
   }
   else{ //is at the destination
      //cout << "timeout1.7" << endl;
      CanceledTrans ct = make_tuple(toutMsg->getTransactionId(),simTime(),(ttmsg->getRoute())[ttmsg->getHopCount()-1],-1);

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
         [&transactionId](const tuple<int, simtime_t, int, int>& p)
         { return get<0>(p) == transactionId; });

   if (iter!=canceledTransactions.end()){
      canceledTransactions.erase(iter);
   }

   //at last index of route
   //add transactionId to set of successful transactions that we don't need to send time out messages
   if (!_waterfillingEnabled){
      successfulDoNotSendTimeOut.insert(aMsg->getTransactionId());

   }
   else{ //_waterfillingEnabled
      //Radhika TODO: following should happen irrespective of timeouts?
      tuple<int,int> key = make_tuple(aMsg->getTransactionId(), aMsg->getPathIndex());

      transPathToAckState[key].amtReceived = transPathToAckState[key].amtReceived + aMsg->getAmount();
      /*
         if (transPathToAckState[key].amtReceived == transPathToAckState[key].amtSent){
         transPathToAckState.erase(key);
         }
       */
   }
}

void hostNode::handleAckMessageShortestPath(routerMsg* ttmsg){
   int destNode = ttmsg->getRoute()[0]; //destination of origin TransUnit job
   statNumCompleted[destNode] = statNumCompleted[destNode]+1;
   statRateCompleted[destNode] = statRateCompleted[destNode]+1;
}

void hostNode::handleAckMessagePriceScheme(routerMsg* ttmsg){
   ackMsg *aMsg = check_and_cast<ackMsg *>(ttmsg->getEncapsulatedPacket());
   int pathIndex = aMsg->getPathIndex();
   int destNode = ttmsg->getRoute()[0]; //destination of origin TransUnit job
   statNumCompleted[destNode] = statNumCompleted[destNode]+1;
   statRateCompleted[destNode] = statRateCompleted[destNode]+1;

   nodeToShortestPathsMap[destNode][pathIndex].sumOfTransUnitsInFlight -= aMsg->getAmount();

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
   //generate updateMsg
   int prevNode = ttmsg->getRoute()[ttmsg->getHopCount()-1];

   //remove transaction from outgoing_trans_unit
   map<Id, double> *outgoingTransUnits = &(nodeToPaymentChannel[prevNode].outgoingTransUnits);
   ackMsg *aMsg = check_and_cast<ackMsg *>(ttmsg->getEncapsulatedPacket());

   (*outgoingTransUnits).erase(make_tuple(aMsg->getTransactionId(), aMsg->getHtlcIndex()));

   //increment signal numProcessed
   nodeToPaymentChannel[prevNode].statNumProcessed = nodeToPaymentChannel[prevNode].statNumProcessed + 1;
   //assume successful transaction

   routerMsg* uMsg =  generateUpdateMessage(aMsg->getTransactionId(), prevNode, aMsg->getAmount(), aMsg->getHtlcIndex() );
   sendUpdateMessage(uMsg);

   //broadcast completion time signal
   //ackMsg *aMsg = check_and_cast<ackMsg *>(ttmsg->getEncapsulatedPacket());
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
routerMsg *hostNode::generateAckMessage(routerMsg* ttmsg){ //default is false
   bool isSuccess;
   int transactionId;
   double timeSent;
   double amount;
   int sender = (ttmsg->getRoute())[0];
   int receiver = (ttmsg->getRoute())[(ttmsg->getRoute()).size() -1];
   bool hasTimeOut;

   transactionMsg *transMsg = check_and_cast<transactionMsg *>(ttmsg->getEncapsulatedPacket());
   transactionId = transMsg->getTransactionId();
   timeSent = transMsg->getTimeSent();
   amount = transMsg->getAmount();
   isSuccess = true;
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
   vector<int> route=ttmsg->getRoute();
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
   //printVector(path);

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


void hostNode::splitTransactionForWaterfilling(routerMsg * ttmsg){
   transactionMsg *transMsg = check_and_cast<transactionMsg *>(ttmsg->getEncapsulatedPacket());
   int destNode = transMsg->getReceiver();
   double remainingAmt = transMsg->getAmount();
   bool ewma = true;

   map<int, double> pathMap = {}; //key is pathIdx, double is amt
   vector<double> bottleneckList;
   
   priority_queue<pair<double,int>> pq;
   if (_loggingEnabled) cout << "bottleneck: ";
   for (auto iter: nodeToShortestPathsMap[destNode] ){
      int key = iter.first;
      double bottleneck = (iter.second).bottleneck;
      bottleneckList.push_back(bottleneck);
      if (_loggingEnabled) cout << bottleneck << " (" << iter.second.lastUpdated<<"), ";
      pq.push(make_pair(bottleneck, key)); //automatically sorts with biggest first index-element
   }


   if (_loggingEnabled) cout << endl;
   double highestBal;
   double secHighestBal;
   double diffToSend;
   double amtToSend;
   int highestBalIdx;

   //Radhika TODO: need to walk over this code with Vibhaa if we encounter this case.
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
       vector<double> cumProbabilties;
       
       if (highestBal >= 0) {
            if (ewma && highestBal > 0) {
                std::default_random_engine generator;
                std::discrete_distribution<int> d(std::begin(bottleneckList), std::end(bottleneckList));
                int index = d(generator);

                pathMap[index] = pathMap[index] + remainingAmt;
                remainingAmt = 0;

            }
            else {
                pathMap[highestBalIdx] = pathMap[highestBalIdx] + remainingAmt;
                remainingAmt = 0;
            }
       }
   }
   else {
      cout << "PATHS NOT FOUND to " << destNode << "WHEN IT SHOULD HAVE BEEN";
      throw std::exception();
   }

   // print how much was sent on each path
   /*
      for (auto p: pathMap){
      cout << "index: "<< p.first << "; bottleneck: " << nodeToShortestPathsMap[destNode][p.first].bottleneck << "; ";
      cout << "amtToSend: "<< p.second  << endl;
      }
      cout << endl;
    */

    //Radhika: where is this check that if balance 0, don't send happening?
    // works as long as txn is sent on one path only
   if (remainingAmt == 0) {
       statRateAttempted[destNode] = statRateAttempted[destNode] + 1;
   }
   transMsg->setAmount(remainingAmt);
   
   for (auto p: pathMap){
      if (p.second > 0){
         tuple<int,int> key = make_tuple(transMsg->getTransactionId(),p.first); //key is (transactionId, pathIndex)

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
         
         // decrement balance after sending this out
        nodeToShortestPathsMap[destNode][p.first].bottleneck -= amt;
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
         [&transactionId](const tuple<int, simtime_t, int, int>& p)
         { return get<0>(p) == transactionId; });
	 //Radhika: is the first condition needed? Isn't last condition enough??
   if ( iter!=canceledTransactions.end() || (transMsg->getHasTimeOut() && (simTime() > transMsg->getTimeSent() + transMsg->getTimeOut())) ){
      
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


   statNumArrived[destNode] = statNumArrived[destNode]+1;
   statRateArrived[destNode] = statRateArrived[destNode]+1;

   destNodeToNumTransPending[destNode] = destNodeToNumTransPending[destNode] + 1;


   if (nodeToShortestPathsMap.count(destNode) == 0 ){
      //cout << "start getKShortestRoutes" << endl;
      vector<vector<int>> kShortestRoutes = getKShortestRoutes(transMsg->getSender(),destNode, _kValue);
      //cout << "num paths from kShortestRoutes: " << kShortestRoutes.size() << endl;
      for (auto p: kShortestRoutes){
         printVector(p);
      }
      cout << "destNode: " << destNode << endl;
      cout << "myIndex: " << myIndex() << endl;
      initializePriceProbes(kShortestRoutes, destNode);
      cout << "num paths after initializePriceProbes: " << nodeToShortestPathsMap[destNode].size() << endl;
      //TODO: change to a queue implementation
      scheduleAt(simTime() + _maxTravelTime, ttmsg);
      return;
   }


   int transactionId = transMsg->getTransactionId();
   //cout << "ttmsg->getHopCount(): " << ttmsg->getHopCount() << endl;
   //cout << "ttmsg->getRoute(): ";
   //printVector(ttmsg->getRoute());

   if(ttmsg->getRoute().size() > 0){ // are at last index of route
      cout << "at last index of route " << endl;
      int prevNode = ttmsg->getRoute()[ttmsg->getHopCount()-1];
      //transaction arrival, need to increment nValue
      nodeToPaymentChannel[prevNode].nValue = nodeToPaymentChannel[prevNode].nValue + 1;

      map<Id, double> *incomingTransUnits = &(nodeToPaymentChannel[prevNode].incomingTransUnits);
      (*incomingTransUnits)[make_tuple(transMsg->getTransactionId(), transMsg->getHtlcIndex())] = transMsg->getAmount();
      int transactionId = transMsg->getTransactionId();
      cout << "generatedAckMessage" << endl;
      routerMsg* newMsg =  generateAckMessage(ttmsg);

      if (_timeoutEnabled){
         //forward ack message - no need to wait;
         //check if transactionId is in canceledTransaction set
         auto iter = find_if(canceledTransactions.begin(),
               canceledTransactions.end(),
               [&transactionId](const tuple<int, simtime_t, int, int>& p)
               { return get<0>(p) == transactionId; });

         if (iter!=canceledTransactions.end()){
            canceledTransactions.erase(iter);
         }

      }
      cout << "ackMessage route:";
      printVector(newMsg->getRoute());
      cout << "myIndex(): " << myIndex() << endl;
      forwardAckMessage(newMsg);
      return;
   }
   else{
      //is a self-message/at hop count = 0
      int destNode = transMsg->getReceiver();

      //add transaction to queue per destination
      DestInfo* destInfo = &(nodeToDestInfo[destNode]);

      //check if there are transactions queued up
      if ((destInfo->transWaitingToBeSent).size() == 0){ //none queued up
         //for each of the k paths for that destination
         for (auto p: nodeToShortestPathsMap[destNode]){
            int pathIndex = p.first;
            if (p.second.rateToSendTrans > 0 && simTime() > p.second.timeToNextSend){
               //send transaction on that path

               //set transaction path
               ttmsg->setRoute(p.second.path);
               forwardTransactionMessage(ttmsg);

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
         //cout << "transaction queued " << endl;
         //cout << "destination node: " << destNode << endl;
         //cout << "num paths:" << nodeToShortestPathsMap[destNode].size() << endl;
         for (auto p: nodeToShortestPathsMap[destNode]){
            PathInfo pInfo = p.second;
            if (p.second.isSendTimerSet == false){
               cout << "triggerTransSendMsg set for: " << pInfo.timeToNextSend << endl;
               simtime_t timeToNextSend = pInfo.timeToNextSend;
               if (simTime()>timeToNextSend){
                  //pInfo.timeToNextSend = simTime() + 0.5;
                  timeToNextSend = simTime() + 0.5; //TODO: fix this constant
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
         destNodeToNumTransPending[destNode] = destNodeToNumTransPending[destNode]-1;
         if ((!_timeoutEnabled) || (simTime() < (transMsg->getTimeSent() + transMsg->getTimeOut()))) { //TODO: remove?
            splitTransactionForWaterfilling(ttmsg);
            if (transMsg->getAmount()>0){
               destNodeToNumTransPending[destNode] = destNodeToNumTransPending[destNode]+1;
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
            ttmsg->decapsulate();
            delete transMsg;
            delete ttmsg;
         }
         return;
      }
      else{ //if not recent, reschedule transaction to arrive after 1sec. 
         if (destNodeToNumTransPending[destNode] == 1){
            restartProbes(destNode);
         }
         scheduleAt(simTime() + 1, ttmsg);
         return;
      }
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

   if (!_waterfillingEnabled){
       // in waterfilling message can be received multiple times, so only update when simTime == transTime
      statNumArrived[destination] = statNumArrived[destination] + 1;
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
               [&transactionId](const tuple<int, simtime_t, int, int>& p)
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
      q = &(nodeToPaymentChannel[nextNode].queuedTransUnits);
      tuple<int,int > key = make_tuple(transMsg->getTransactionId(), transMsg->getHtlcIndex());
      (*q).push_back(make_tuple(transMsg->getPriorityClass(), transMsg->getAmount(),
               ttmsg, key));
      push_heap((*q).begin(), (*q).end(), sortPriorityThenAmtFunction);
      processTransUnits(nextNode, *q);
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
   sprintf(msgname, "tic-%d-to-%d routerMsg", unit.sender, unit.receiver);
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
   /*
      auto iter = find_if(canceledTransactions.begin(),
      canceledTransactions.end(),
      [&transactionId](const tuple<int, simtime_t, int, int>& p)
      { return get<0>(p) == transactionId; });

      if ( iter!=canceledTransactions.end() ){
   //TODO: check to see if transactionId is in canceledTransactions set

   return false;
   }
    */
   if (nodeToPaymentChannel[nextDest].balance <= 0 || transMsg->getAmount() >nodeToPaymentChannel[nextDest].balance){
      return false;
   }
   else{
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

