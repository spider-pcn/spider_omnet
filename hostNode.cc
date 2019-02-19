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
bool _priceSchemeEnabled;
bool _landmarkRoutingEnabled;
vector<tuple<int,int>> _landmarksWithConnectivityList = {};



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



/* print channel information */
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

/* print any vector */
void printVectorDouble(vector<double> v){
    for (auto temp : v){
        cout << temp << ", ";
    }
    cout << endl;
}


/* samples a random number (index) of the passed in vector
 *  based on the actual probabilities passed in
 */
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
    return 0;
}


/* generate next Transaction to be processed at this node 
 * this is an optimization to prevent all txns from being loaded initially
 */
void hostNode::generateNextTransaction() {
      if (_transUnitList[myIndex()].empty())
          return;
      TransUnit j = _transUnitList[myIndex()].top();
      double timeSent = j.timeSent;
      
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




/* register a signal per destination for this path of the particular type passed in
 * and return the signal created
 */
simsignal_t hostNode::registerSignalPerDestPath(string signalStart, int pathIdx, int destNode) {
    string signalPrefix = signalStart + "PerDestPerPath";
    char signalName[64];
    if (destNode < _numHostNodes){
        sprintf(signalName, signalPrefix + "_%d(host %d)", pathIdx, destNode);
    } else{
        sprintf(signalName, signalPrefix + "_%d(router %d [%d] )", 
             pathIdx, destNode - _numHostNodes, destNode);
    }
    simsignal_t signal = registerSignal(signalName);
    cProperty *statisticTemplate = getProperties()->get("statisticTemplate", 
            signalPrefix + "Template");
    getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
    return signal;
}


/* register a signal per channel of the particular type passed in
 * and return the signal created
 */
simsignal_t hostNode::registerSignalPerChannel(string signalStart, int id) {
    string signalPrefix = signalStart + "PerChannel";
    char signalName[64];
    if (id < _numHostNodes){
        sprintf(signalName, signalPrefix + "(host %d)", id);
    } else{
        sprintf(signalName, signalPrefix + "(router %d [%d] )", 
             pathIdx, id - _numHostNodes, id);
    }
    simsignal_t signal = registerSignal(signalName);
    cProperty *statisticTemplate = getProperties()->get("statisticTemplate", 
            signalPrefix + "Template");
    getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
    return signal;
}


/* register a signal per dest of the particular type passed in
 * and return the signal created
 */
simsignal_t hostNode::registerSignalPerChannel(string signalStart, int destNode, string suffix) {
    string signalPrefix = signalStart + "PerDest" + suffix;
    char signalName[64];
    simsignal_t signal = registerSignal(signalName);
    cProperty *statisticTemplate = getProperties()->get("statisticTemplate", 
            signalPrefix + "Template");
    getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
    return signal;
}


/*
 * handleUpdateMessage - called when receive update message, increment back funds, see if we can
 *      process more jobs with new funds, delete update message
 */
void hostNode::handleUpdateMessage(routerMsg* msg) {
    vector<tuple<int, double , routerMsg *, Id>> *q;
    int prevNode = msg->getRoute()[msg->getHopCount()-1];
    updateMsg *uMsg = check_and_cast<updateMsg *>(msg->getEncapsulatedPacket());
    PaymentChannel *prevChannel = &(nodeToPaymentChannel[prevNode]);
   
    //increment the in flight funds back
    double newBalance = prevChannel->balance + uMsg->getAmount();
    prevChannel->balance =  newBalance;       
    prevChannel->balanceEWMA = (1 -_ewmaFactor) * prevChannel->balanceEWMA 
        + (_ewmaFactor) * newBalance; 

    //remove transaction from incoming_trans_units
    map<Id, double> *incomingTransUnits = &(prevChannel->incomingTransUnits);
    (*incomingTransUnits).erase(make_tuple(uMsg->getTransactionId(), uMsg->getHtlcIndex()));
    prevChannel->statNumProcessed = prevChannel->statNumProcessed + 1;

    msg->decapsulate();
    delete uMsg;
    delete msg; //delete update message

    //see if we can send more jobs out
    q = &(prevChannel->queuedTransUnits);
    processTransUnits(prevNode, *q);
} 



/* responsible for forwarding all messages but transactions which need special care
 * in particular, looks up the next node's interface and sends out the message
 */
void hostNode::forwardMessage(routerMsg* msg){
   // Increment hop count.
   msg->setHopCount(msg->getHopCount()+1);
   //use hopCount to find next destination
   int nextDest = msg->getRoute()[msg->getHopCount()];
   if (_loggingEnabled) cout << "forwarding " << msg->getMessageType() << " at " 
       << simTime() << endl;
   send(msg, nodeToPaymentChannel[nextDest].gate);
}



/* initialize() all of the global parameters and basic
 * per channel information as well as default signals for all
 * payment channels and destinations
 */
void hostNode::initialize() {
    string topologyFile_ = par("topologyFile");
    string workloadFile_ = par("workloadFile");

    completionTimeSignal = registerSignal("completionTime");
    successfulDoNotSendTimeOut = {};

    // initialize global parameters once
    if (getIndex() == 0){  
        _simulationLength = par("simulationLength");
        _statRate = par("statRate");
        _clearRate = par("timeoutClearRate");
        _waterfillingEnabled = par("waterfillingEnabled");
        _smoothWaterfillingEnabled = par("smoothWaterfillingEnabled");
        _timeoutEnabled = par("timeoutEnabled");
        _signalsEnabled = par("signalsEnabled");
        _loggingEnabled = par("loggingEnabled");
        _priceSchemeEnabled = par("priceSchemeEnabled");

        _hasQueueCapacity = false;
        _queueCapacity = 0;
        _reschedulingEnabled = true;

        _landmarkRoutingEnabled = false;
        if (_landmarkRoutingEnabled){
            _hasQueueCapacity = true;
            _queueCapacity = 0;
            _timeoutEnabled = false;
        }

        // price scheme parameters         
        _nesterov = false;
        _secondOrderOptimization = true;

        _eta = par("eta");
        _kappa = par("kappa");
        _tUpdate = par("updateQueryTime");
        _tQuery = par("updateQueryTime");
        _alpha = par("alpha");
        _gamma = 1; // ewma factor to compute per path rates
        _zeta = par("zeta"); // ewma for d_ij every source dest demand
        _minPriceRate = par("minRate");
        _rho = _rhoLambda = _rhoMu = par("rhoValue");

        _epsilon = pow(10, -6);
        cout << "epsilon" << _epsilon << endl;
        
        // smooth waterfilling parameters
        _Tau = par("tau");
        _Normalizer = par("normalizer"); // TODO: C from discussion with Mohammad)
        _ewmaFactor = 1; // EWMA factor for balance information on probes

        if (_waterfillingEnabled || _priceSchemeEnabled || _landmarkRoutingEnabled){
           _kValue = par("numPathChoices");
        }

        _maxTravelTime = 0.0;
        _delta = 0.01; // to avoid divide by zero 
        setNumNodes(topologyFile_);
        // add all the TransUnits into global list
        generateTransUnitList(workloadFile_);

        //create "channels" map - from topology file
        generateChannelsBalancesMap(topologyFile_);
    } 


    // Assign gates to all the payment channels
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
        int key =iter->first; //node

        //fill in balance field of nodeToPaymentChannel
        nodeToPaymentChannel[key].balance = _balances[make_tuple(myIndex(),key)];
        nodeToPaymentChannel[key].balanceEWMA = nodeToPaymentChannel[key].balance;

        // intialize capacity
        double balanceOpp =  _balances[make_tuple(key, myIndex())];
        nodeToPaymentChannel[key].totalCapacity = nodeToPaymentChannel[key].balance + balanceOpp;

        //initialize queuedTransUnits
        vector<tuple<int, double , routerMsg *, Id>> temp;
        make_heap(temp.begin(), temp.end(), sortPriorityThenAmtFunction);
        nodeToPaymentChannel[key].queuedTransUnits = temp;

        //register PerChannel signals
        simsignal_t signal;
        signal = registerSignalPerChannel("numInQueue", key);
        nodeToPaymentChannel[key].numInQueuePerChannelSignal = signal;

        signal = registerSignalPerChannel("numProcessed", key);
        nodeToPaymentChannel[key].numProcessedPerChannelSignal = signal;
        nodeToPaymentChannel[key].statNumProcessed = 0;

        signal = registerSignalPerChannel("balance", key);
        nodeToPaymentChannel[key].balancePerChannelSignal = signal;

        signal = registerSignalPerChannel("numSent", key);
        nodeToPaymentChannel[key].numSentPerChannelSignal = signal;
        nodeToPaymentChannel[key].statNumSent = 0;
    }

    //initialize signals with all other nodes in graph
    for (int i = 0; i < _numHostNodes; ++i) {
        simsignal_t signal;
        signal = registerSignalPerDest("rateCompleted", i, "_Total");
        rateCompletedPerDestSignals.push_back(signal);
        statRateCompleted.push_back(0);

        signal = registerSignalPerDest("numCompleted", i, "_Total");
        numCompletedPerDestSignals.push_back(signal);
        statNumCompleted.push_back(0);

        signal = registerSignalPerDest("rateAttempted", i, "_Total");
        rateAttemptedPerDestSignals.push_back(signal);
        statRateAttempted.push_back(0);

        signal = registerSignalPerDest("rateArrived", i, "_Total");
        rateArrivedPerDestSignals.push_back(signal);
        statRateArrived.push_back(0);

        signal = registerSignalPerDest("numArrived", i, "_Total");
        numArrivedPerDestSignals.push_back(signal);
        statNumArrived.push_back(0);

        signal = registerSignalPerDest("numFailed", i, "_Total");
        numFailedPerDestSignals.push_back(signal);
        statNumFailed.push_back(0);

        signal = registerSignalPerDest("numTimedOut", i, "_Total");
        numTimedOutPerDestSignals.push_back(signal);
        statNumTimedOut.push_back(0);

        signal = registerSignalPerDest("numPending", i, "_Total");
        numPendingPerDestSignals.push_back(signal);
        
        signal = registerSignalPerDest("numWaiting", i, "_Total");
        numWaitingPerDestSignals.push_back(signal);

        signal = registerSignalPerDest("numTimedOutAtSender", i, "_Total");
        numTimedOutAtSenderSignals.push_back(signal);
        statNumTimedOutAtSender.push_back(0);

        signal = registerSignalPerDest("fracSuccessful", i, "_Total");
        fracSuccessfulPerDestSignals.push_back(signal);

        signal = registerSignalPerDest("rateFailed", i, "");
        rateFailedPerDestSignals.push_back(signal);
        statRateFailed.push_back(0);

        signal = registerSignalPerDest("pathPerTrans", i, "");
        pathPerTransPerDestSignals.push_back(signal);

        signal = registerSignalPerDest("demandEstimate", i, "");
        demandEstimatePerDestSignals.push_back(signal);
    }
    
    // generate first transaction
    generateNextTransaction();

    //generate stat message
    routerMsg *statMsg = generateStatMessage();
    scheduleAt(simTime() + 0, statMsg);

    if (_timeoutEnabled){
       routerMsg *clearStateMsg = generateClearStateMessage();
       scheduleAt(simTime()+ _clearRate, clearStateMsg);
    }
}

/* function that is called at the end of the simulation that
 * deletes any remaining messages and records scalars
 */
void hostNode::finish() {
    deleteMessagesInQueues();
    if (myIndex() == 0) {
        // can be done on a per node basis also if need be
        // all in seconds
        recordScalar("max travel time", _maxTravelTime);
        recordScalar("delta", _delta);
        recordScalar("average delay", _avgDelay/1000.0);
        recordScalar("epsilon", _epsilon);
    }
}


/*
 *  given an adjacent node, and TransUnit queue of things to send to that node, sends
 *  TransUnits until channel funds are too low
 *  calls forwardTransactionMessage on every individual TransUnit
 */
void hostNode:: processTransUnits(int dest, vector<tuple<int, double , routerMsg *, Id>>& q) {
    bool successful = true;
    while((int)q.size() > 0 && successful) {
        successful = forwardTransactionMessage(get<2>(q.back()));
        if (successful){
            q.pop_back();
        }
    }
}


/* removes all of the cancelled messages from the queues to any
 * of the adjacent payment channels
 */
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
            PathInfo second = nodeToShortestPathsMap[destNode][pathIndex];

            if (second.rateToSendTrans > 0 && simTime() > second.timeToNextSend){
               //set transaction path and update inflight on path
               ttmsg->setRoute(second.path);

               int nextNode = second.path[1];
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
              nodeToShortestPathsMap[destNode][pathIndex].statRateAttempted =
                nodeToShortestPathsMap[destNode][pathIndex].statRateAttempted + 1;
              statRateAttempted[destNode] += 1;
               
               //update "amount of transaction in flight" and other state regarding last transaction to this destination
               nodeToShortestPathsMap[destNode][pathIndex].sumOfTransUnitsInFlight =
                  nodeToShortestPathsMap[destNode][pathIndex].sumOfTransUnitsInFlight + transMsg->getAmount();
               nodeToShortestPathsMap[destNode][pathIndex].lastTransSize = transMsg->getAmount();
               nodeToShortestPathsMap[destNode][pathIndex].lastSendTime = simTime();
               nodeToShortestPathsMap[destNode][pathIndex].amtAllowedToSend = 
                   max(nodeToShortestPathsMap[destNode][pathIndex].amtAllowedToSend - transMsg->getAmount(), 
                           0.0);


               //update the "time when the next transaction can be sent
               double rateToUse = max(second.rateToSendTrans, _epsilon);
               simtime_t newTimeToNextSend =  simTime() + max(transMsg->getAmount()/rateToUse, _epsilon);
               nodeToShortestPathsMap[destNode][pathIndex].timeToNextSend = newTimeToNextSend;
                
               if (_signalsEnabled) emit(pathPerTransPerDestSignals[destNode], pathIndex);

               return;
            }
         }

         //transaction cannot be sent on any of the paths, queue transaction
         // can't you just send out transaction here because you should have actually sent the txn but you havent
         destInfo->transWaitingToBeSent.push_back(ttmsg);
         for (auto p: nodeToShortestPathsMap[destNode]){
            PathInfo pInfo = p.second;
            if (p.second.isSendTimerSet == false){
               simtime_t timeToNextSend = pInfo.timeToNextSend;
               if (simTime()>timeToNextSend){
                  //pInfo.timeToNextSend = simTime() + 0.5;
                  timeToNextSend = simTime() + _epsilon; //TODO: fix this constant
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

