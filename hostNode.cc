#include "hostNode.h"
#include <queue>
#include "hostInitialize.h"

//global parameters
vector<TransUnit> _transUnitList; //list of all transUnits
int _numNodes;
int _numRouterNodes;
int _numHostNodes;
double _maxTravelTime;
//number of nodes in network
map<int, vector<pair<int,int>>> _channels; //adjacency list format of graph edges of network
map<tuple<int,int>,double> _balances;
//map of balances for each edge; key = <int,int> is <source, destination>
double _statRate;
double _clearRate;
//bool withFailures;
bool _useWaterfilling;
int _kValue;
double _simulationLength; //TODO: need to set this in hostInitialize

#define MSGSIZE 100

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

routerMsg * hostNode::generateProbeMessage(int destNode, int pathIdx, vector<int> path){

   char msgname[MSGSIZE];
   sprintf(msgname, "tic-%d-to-%d probeMsg [idx %d]", myIndex(), destNode, pathIdx);
   //int pathIndex;
   //   int sender;
   //  int receiver;
   // bool isReversed = false;
   // IntVector pathBalances;
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


void hostNode:: printNodeToPaymentChannel(){

        printf("print of channels\n" );
        for (auto i : nodeToPaymentChannel){
           printf("(key: %d )",i.first);
           /*
           for (auto k: i.second){
              printf("(%d, %d) ",get<0>(k), get<1>(k));
           }

           printf("] \n");
           */
        }
        cout<<endl;

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
   //cout << "myIndex:" << myIndex() << endl;
   //printNodeToPaymentChannel();
   send(msg, nodeToPaymentChannel[nextDest].gate);
   //cout << "after sending " << endl;
}

void printVector(vector<int> v){
    for (auto temp : v){
        cout << temp << ", ";
    }
    cout << endl;
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

      //TODO
      int pathIdx = pMsg->getPathIndex();

      int destNode = pMsg->getReceiver();


      PathInfo * p = &(nodeToShortestPathsMap[destNode][pathIdx]);
      assert(p->path == pMsg->getPath());

      p->lastUpdated = simTime();
      vector<double> pathBalances = pMsg->getPathBalances();
      int bottleneck = minVectorElemDouble(pathBalances);

      p->bottleneck = bottleneck ;

      p->pathBalances = pathBalances;
      //cout << "path Index: " << pathIdx << endl;



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
/*
      for (auto p: nodeToShortestPathsMap[destNode]){
          cout << "[index]: "<< p.first << endl;
          cout << "path: ";
          printVector(p.second.path);
          cout << "pathBalances: ";
          printVectorDouble(p.second.pathBalances);
          cout << "bottleneck:" << p.second.bottleneck << endl;
      }
*/


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


void hostNode::initializeProbes(vector<vector<int>> kShortestPaths, int destNode){ //maybe less than k routes

   for (int pathIdx = 0; pathIdx < kShortestPaths.size(); pathIdx++){
      //map<int, map<int, PathInfo>> nodeToShortestPathsMap;

      PathInfo temp = {};
      nodeToShortestPathsMap[destNode][pathIdx] = temp;
      nodeToShortestPathsMap[destNode][pathIdx].path = kShortestPaths[pathIdx];
      //initialize signal


         char signalName[64];
         simsignal_t signal;
         cProperty *statisticTemplate;

         //numInQueuePerChannel signal

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

      /*

       cout << "path number " << pathIdx << endl;
      for (int i=0; i<kShortestPaths[pathIdx].size(); i++ ){
          cout << kShortestPaths[pathIdx][i] << ", ";
      }
      cout << endl;
      cout << "before generate probe message" << endl;
       */

      routerMsg * msg = generateProbeMessage(destNode, pathIdx, kShortestPaths[pathIdx]);

      //cout << "after generate probe message" << endl;
      //printChannels();
      //cout << "number Host nodes " << _numHostNodes << endl;

      forwardProbeMessage(msg);
      //cout << "after forward probe message" << endl;

   }

}



/* initialize() -
 *  if node index is 0:
 *      - initialize global parameters: instantiate all TransUnits, and add to global list "trans_unit_list"
 *      - create "channels" map - adjacency list representation of network
 *      - create "balances" map - map representing initial balances; each key is directed edge (source, dest)
 *          and value is balance
 *  all nodes:
 *      - find my TransUnits from "trans_unit_list" and store them into my private list "my_trans_units"
 *      - create "node_to_gate" map - map to identify channels, each key is node index adjacent to this node
 *          value is gate connecting to that adjacent node
 *      - create "node_to_balances" map - map to identify remaining outgoing balance for gates/channels connected
 *          to this node (key is adjacent node index; value is remaining outgoing balance)
 *      - create "node_to_queued_trans_units" map - map to get TransUnit queue for outgoing TransUnits,
 *          one queue corresponding to each adjacent node/each connected channel
 *      - send all TransUnits in my_trans_units as a message (using generateTransactionMessage), sent to myself (node index),
 *          scheduled to arrive at timeSent parameter field in TransUnit
 */
void hostNode::initialize()
{
   string topologyFile_ = par("topologyFile");
   string workloadFile_ = par("workloadFile");

   completionTimeSignal = registerSignal("completionTime");

   successfulDoNotSendTimeOut = {};

   if (getIndex() == 0){  //main initialization for global parameters
      _simulationLength = 30;
      _maxTravelTime = 0.0;
      //set statRate
      _statRate = 0.2;
      _clearRate = 0.5;


      _useWaterfilling = true;

      if (_useWaterfilling){
         _kValue = 3;
      }


      setNumNodes(topologyFile_); //TODO: condense into generate_trans_unit_list
      // add all the TransUnits into global list
      generateTransUnitList(workloadFile_, _transUnitList);

      //create "channels" map - from topology file
      //create "balances" map - from topology file
      generateChannelsBalancesMap(topologyFile_, _channels, _balances );
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
         nodeToPaymentChannel[key] = temp; //TODO: change to myIndex
      }
   } while (i < gateSize);

   //initialize everything for adjacent nodes/nodes with payment channel to me
   for(auto iter = nodeToPaymentChannel.begin(); iter != nodeToPaymentChannel.end(); ++iter)
   {
      int key =  iter->first; //node

      //fill in balance field of nodeToPaymentChannel
      nodeToPaymentChannel[key].balance = _balances[make_tuple(myIndex(),key)];

      //initialize queuedTransUnits
      vector<tuple<int, double , routerMsg *, Id>> temp;
      make_heap(temp.begin(), temp.end(), sortPriorityThenAmtFunction);
      nodeToPaymentChannel[key].queuedTransUnits = temp;

      //register signals
      char signalName[64];
      simsignal_t signal;
      cProperty *statisticTemplate;

      //numInQueuePerChannel signal
      if (key<_numHostNodes){
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
      if (key<_numHostNodes){
      sprintf(signalName, "numProcessedPerChannel(host %d)", key);
      }
      else{
        sprintf(signalName, "numProcessedPerChannel(router %d [%d])", key-_numHostNodes, key);
      }
      signal = registerSignal(signalName);
      statisticTemplate = getProperties()->get("statisticTemplate", "numProcessedPerChannelTemplate");
      getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
      nodeToPaymentChannel[key].numProcessedPerChannelSignal = signal;

      //statNumProcessed int
      nodeToPaymentChannel[key].statNumProcessed = 0;

      //balancePerChannel signal

      if (key<_numHostNodes){
            sprintf(signalName, "balancePerChannel(host %d)", key);
            }
            else{
              sprintf(signalName, "balancePerChannel(router %d [%d])", key-_numHostNodes, key);
            }

      signal = registerSignal(signalName);
      statisticTemplate = getProperties()->get("statisticTemplate", "balancePerChannelTemplate");
      getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
      nodeToPaymentChannel[key].balancePerChannelSignal = signal;

      //numSentPerChannel signal
      if (key<_numHostNodes){
            sprintf(signalName, "numSentPerChannel(host %d)", key);
            }
            else{
              sprintf(signalName, "numSentPerChannel(router %d [%d])", key-_numHostNodes, key);
            }
      signal = registerSignal(signalName);
      statisticTemplate = getProperties()->get("statisticTemplate", "numSentPerChannelTemplate");
      getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
      nodeToPaymentChannel[key].numSentPerChannelSignal = signal;

      //statNumSent int
      nodeToPaymentChannel[key].statNumSent = 0;
   }


   //initialize signals with all other nodes in graph
   for (int i = 0; i < _numHostNodes; ++i) {
      char signalName[64];
      simsignal_t signal;
      cProperty *statisticTemplate;

      //numCompletedPerDest signal

      sprintf(signalName, "numCompletedPerDest_Total(host node %d)", i);
      signal = registerSignal(signalName);
      statisticTemplate = getProperties()->get("statisticTemplate", "numCompletedPerDestTemplate");
      getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
      numCompletedPerDestSignals.push_back(signal);

      statNumCompleted.push_back(0);

      //numAttemptedPerDest signal
      sprintf(signalName, "numAttemptedPerDest_Total(host node %d)", i);
      signal = registerSignal(signalName);
      statisticTemplate = getProperties()->get("statisticTemplate", "numAttemptedPerDestTemplate");
      getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
      numAttemptedPerDestSignals.push_back(signal);

      statNumAttempted.push_back(0);


      //numTimedOutPerDest signal
         sprintf(signalName, "numTimedOutPerDest_Total(host node %d)", i);
         signal = registerSignal(signalName);
         statisticTemplate = getProperties()->get("statisticTemplate", "numTimedOutPerDestTemplate");
         getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
         numTimedOutPerDestSignals.push_back(signal);
         statNumTimedOut.push_back(0);



   }



   //iterate through the global trans_unit_list and find my TransUnits
   for (int i=0; i<(int)_transUnitList.size(); i++){
      if (_transUnitList[i].sender == myIndex()){
         myTransUnits.push_back(_transUnitList[i]);
      }
   }


   //implementing timeSent parameter, send all msgs at beginning
   for (int i=0 ; i<(int)myTransUnits.size(); i++){

      TransUnit j = myTransUnits[i];
      double timeSent = j.timeSent;

      routerMsg *msg = generateTransactionMessage(j);

      if (_useWaterfilling){
         vector<int> blankPath = {};
         msg->setRoute(blankPath);
      }

      scheduleAt(timeSent, msg);

      if (j.hasTimeOut){ //TODO : timeOut implementation is not going to work with waterfilling

         routerMsg *toutMsg = generateTimeOutMessage(msg);
         scheduleAt(timeSent+j.timeOut, toutMsg );

      }

   }



   //get stat message
   routerMsg *statMsg = generateStatMessage();
   scheduleAt(simTime()+0, statMsg);

   routerMsg *clearStateMsg = generateClearStateMessage();
   scheduleAt(simTime()+_clearRate, clearStateMsg);


}

void hostNode::handleMessage(cMessage *msg)
{


   //cout << "Host Time:" << simTime() << endl;
   //cout << "msg: " << msg->getName() << endl;



   routerMsg *ttmsg = check_and_cast<routerMsg *>(msg);

   if (simTime() > _simulationLength){
      if (ttmsg->getMessageType() == TRANSACTION_MSG){
         auto tempMsg = ttmsg->getEncapsulatedPacket();
         ttmsg->decapsulate();
         delete tempMsg;
         delete ttmsg;
         return;
      }
   }


   if (ttmsg->getMessageType()==ACK_MSG){ //preprocessor constants defined in routerMsg_m.h


      //cout << "[HOST "<< myIndex() <<": RECEIVED ACK MSG] " << msg->getName() << endl;
      //print_message(ttmsg);
      //print_private_values();
      handleAckMessage(ttmsg);
      //cout << "[AFTER HANDLING:]" <<endl;
      // print_private_values();
   }
   else if(ttmsg->getMessageType()==TRANSACTION_MSG){
      //cout<< "[HOST "<< myIndex() <<": RECEIVED TRANSACTION MSG]  "<< msg->getName() <<endl;
      // print_message(ttmsg);
      // print_private_values();
      handleTransactionMessage(ttmsg);
      //cout<< "[AFTER HANDLING:] "<<endl;
      // print_private_values();
   }
   else if(ttmsg->getMessageType()==UPDATE_MSG){
      //cout<< "[HOST "<< myIndex() <<": RECEIVED UPDATE MSG] "<< msg->getName() << endl;
      // print_message(ttmsg);
      // print_private_values();
      handleUpdateMessage(ttmsg);
      //cout<< "[AFTER HANDLING:]  "<<endl;
      //print_private_values();

   }
   else if (ttmsg->getMessageType() ==STAT_MSG){
      //cout<< "[HOST "<< myIndex() <<": RECEIVED STAT MSG] "<< msg->getName() << endl;
      handleStatMessage(ttmsg);
      //cout<< "[AFTER HANDLING:]  "<<endl;
   }
   else if (ttmsg->getMessageType() == TIME_OUT_MSG){
      //cout<< "[HOST "<< myIndex() <<": RECEIVED TIME_OUT_MSG] "<<msg->getName() << endl;
      handleTimeOutMessage(ttmsg);
      //cout<< "[AFTER HANDLING:]  "<<endl;
   }
   else if (ttmsg->getMessageType() == PROBE_MSG){
      //cout<< "[HOST "<< myIndex() <<": RECEIVED PROBE_MSG] "<< msg->getName() << endl;
      handleProbeMessage(ttmsg);
      //cout<< "[AFTER HANDLING:]  "<<endl;
   }
   else if (ttmsg->getMessageType() == CLEAR_STATE_MSG){
       //cout<< "[HOST "<< myIndex() <<": RECEIVED CLEAR_STATE_MSG] " << msg->getName() << endl;
      handleClearStateMessage(ttmsg);
      //cout<< "[AFTER HANDLING:]  "<<endl;
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


      if (simTime() > (msgArrivalTime + _maxTravelTime)){ //we can process it
         if (prevNode != -1){

            map<tuple<int,int>, double> *incomingTransUnits = &(nodeToPaymentChannel[prevNode].incomingTransUnits);
            auto iterIncoming = find_if((*incomingTransUnits).begin(),
                  (*incomingTransUnits).end(),
                  [&transactionId](const pair<tuple<int, int >, double> & p)
                  { return get<0>(p.first) == transactionId; });
            while (iterIncoming != (*incomingTransUnits).end()){
               if (iterIncoming != (*incomingTransUnits).end()){
                  iterIncoming = (*incomingTransUnits).erase(iterIncoming);
                  tuple<int, int> tempId = make_tuple(transactionId, get<1>(iterIncoming->first));
                  // start queue searching
                  vector<tuple<int, double, routerMsg*, Id>>* queuedTransUnits = &(nodeToPaymentChannel[nextNode].queuedTransUnits);
                  sort_heap((*queuedTransUnits).begin(), (*queuedTransUnits).end());

                  auto iterQueue = find_if((*queuedTransUnits).begin(),
                        (*queuedTransUnits).end(),
                        [&tempId](const tuple<int, double, routerMsg*, Id>& p)
                        { return (get<3>(p) == tempId); });
                  if (iterQueue != (*queuedTransUnits).end()){
                     iterQueue= (*queuedTransUnits).erase(iterQueue);
                     numCleared++;

                  }

                  make_heap((*queuedTransUnits).begin(), (*queuedTransUnits).end(), sortPriorityThenAmtFunction);
                  //end queue searching
               }

               auto iterIncoming = find_if((*incomingTransUnits).begin(),
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
               if (iterOutgoing != (*outgoingTransUnits).end()){
                  double amount = iterOutgoing -> second;
                  nodeToPaymentChannel[nextNode].balance = nodeToPaymentChannel[nextNode].balance + amount;
                  iterOutgoing = (*outgoingTransUnits).erase(iterOutgoing);
               }
               auto iterOutgoing = find_if((*outgoingTransUnits).begin(),
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



void hostNode::handleStatMessage(routerMsg* ttmsg){
   if (simTime() > _simulationLength){
      delete ttmsg;
      deleteMessagesInQueues();
   }
   else{
      scheduleAt(simTime()+_statRate, ttmsg);

   }

   for ( auto it = nodeToPaymentChannel.begin(); it!= nodeToPaymentChannel.end(); it++){ //iterate through all adjacent nodes

      int node = it->first; //key
      emit(nodeToPaymentChannel[node].numInQueuePerChannelSignal, (nodeToPaymentChannel[node].queuedTransUnits).size());

      emit(nodeToPaymentChannel[node].balancePerChannelSignal, nodeToPaymentChannel[node].balance);

      emit(nodeToPaymentChannel[node].numProcessedPerChannelSignal, nodeToPaymentChannel[node].statNumProcessed);
      nodeToPaymentChannel[node].statNumProcessed = 0;

      emit(nodeToPaymentChannel[node].numSentPerChannelSignal, nodeToPaymentChannel[node].statNumSent);
      nodeToPaymentChannel[node].statNumSent = 0;
   }


   for (auto it = 0; it<_numHostNodes; it++){ //iterate through all nodes in graph

       for (auto p: nodeToShortestPathsMap[it]){
           emit(p.second.bottleneckPerDestPerPathSignal, p.second.bottleneck);

       }
      emit(numAttemptedPerDestSignals[it], statNumAttempted[it]);

      emit(numCompletedPerDestSignals[it], statNumCompleted[it]);

      emit(numTimedOutPerDestSignals[it], statNumTimedOut[it]);
   }

}


void hostNode::forwardTimeOutMessage(routerMsg* msg){
   // Increment hop count.
   msg->setHopCount(msg->getHopCount()+1);
   //use hopCount to find next destination
   int nextDest = msg->getRoute()[msg->getHopCount()];
   //ackMsg *aMsg = check_and_cast<ackMsg *>(msg->getEncapsulatedPacket());

   send(msg, nodeToPaymentChannel[nextDest].gate);

}

/*
 *  handleTimeOutMessage -
 */

void hostNode::handleTimeOutMessage(routerMsg* ttmsg){

   timeOutMsg *toutMsg = check_and_cast<timeOutMsg *>(ttmsg->getEncapsulatedPacket());
   //cout << "hopCount: " << ttmsg->getHopCount() << endl;
   if ((ttmsg->getHopCount())==0){ //is at the sender

      if (_useWaterfilling){
         int transactionId = toutMsg->getTransactionId();
         int destination = toutMsg->getReceiver();
         // map<int, map<int, PathInfo>> nodeToShortestPathsMap = {};
         for (auto p : (nodeToShortestPathsMap[destination])){
            int pathIndex = p.first;
            tuple<int,int> key = make_tuple(transactionId, pathIndex);

            // cout << "sent time out msg" << endl;
                         routerMsg* waterTimeOutMsg = generateWaterfillingTimeOutMessage(
                               nodeToShortestPathsMap[destination][p.first].path, transactionId, destination);
                         //TODO: forwardTimeOutMsg
                         forwardTimeOutMessage(waterTimeOutMsg);

            /*if (transPathToAckState.count(key)==0){
               //do not generate time out msg for path
                statNumTimedOut[destination] = statNumTimedOut[destination]  + 1;
            }
            else
            */
             /*
            if(transPathToAckState[key].amtSent == transPathToAckState[key].amtReceived){
               //do not generate time out msg for path
                statNumTimedOut[destination] = statNumTimedOut[destination]  + 1;
            }
            else{
               // cout << "sent time out msg" << endl;
               routerMsg* waterTimeOutMsg = generateWaterfillingTimeOutMessage(
                     nodeToShortestPathsMap[destination][p.first].path, transactionId, destination);
               //TODO: forwardTimeOutMsg
               forwardTimeOutMessage(waterTimeOutMsg);
            }
            */
         }
         delete ttmsg;

      }
      else{

         if (successfulDoNotSendTimeOut.count(toutMsg->getTransactionId())>0){ //already received ack for it, do not send out
            successfulDoNotSendTimeOut.erase(toutMsg->getTransactionId());
            int destination = toutMsg->getReceiver();
            statNumTimedOut[destination] = statNumTimedOut[destination]  + 1;
            ttmsg->decapsulate();
            delete toutMsg;
            delete ttmsg;
         }
         else{

            int nextNode = (ttmsg->getRoute())[ttmsg->getHopCount()+1];

            CanceledTrans ct = make_tuple(toutMsg->getTransactionId(),simTime(),-1, nextNode);

            canceledTransactions.insert(ct);
            forwardTimeOutMessage(ttmsg);
         }
      }
   }
   else{ //is at the destination
      //cout << "at timeOutMsg destination" << endl;
      CanceledTrans ct = make_tuple(toutMsg->getTransactionId(),simTime(),(ttmsg->getRoute())[ttmsg->getHopCount()-1],-1);
      canceledTransactions.insert(ct);

      ttmsg->decapsulate();
      delete toutMsg;
      delete ttmsg;
   }//end else ((ttmsg->getHopCount())==0)
}


/* handleAckMessage
 */
void hostNode::handleAckMessage(routerMsg* ttmsg){ //TODO: fix handleAckMessage
   //generate updateMsg
   int prevNode = ttmsg->getRoute()[ttmsg->getHopCount()-1];

   //remove transaction from outgoing_trans_unit
   map<Id, double> *outgoingTransUnits = &(nodeToPaymentChannel[prevNode].outgoingTransUnits);
   ackMsg *aMsg = check_and_cast<ackMsg *>(ttmsg->getEncapsulatedPacket());

   (*outgoingTransUnits).erase(make_tuple(aMsg->getTransactionId(), aMsg->getHtlcIndex()));

   //increment signal numProcessed
   nodeToPaymentChannel[prevNode].statNumProcessed = nodeToPaymentChannel[prevNode].statNumProcessed+1;

   //assume successful transaction

   routerMsg* uMsg =  generateUpdateMessage(aMsg->getTransactionId(), prevNode, aMsg->getAmount() );
   sendUpdateMessage(uMsg);


   //check if transactionId is in canceledTransaction set
   int transactionId = aMsg->getTransactionId();

   //TODO: not sure if this works properly
   auto iter = find_if(canceledTransactions.begin(),
         canceledTransactions.end(),
         [&transactionId](const tuple<int, simtime_t, int, int>& p)
         { return get<0>(p) == transactionId; });

   if (iter!=canceledTransactions.end()){
      canceledTransactions.erase(iter);
   }



   //at last index of route
   //add transactionId to set of successful transactions that we don't need to send time out messages
   if (aMsg->getHasTimeOut()  && !_useWaterfilling){
      successfulDoNotSendTimeOut.insert(aMsg->getTransactionId());

   }
   else if(aMsg->getHasTimeOut()  && _useWaterfilling){
      tuple<int,int> key = make_tuple(aMsg->getTransactionId(), aMsg->getPathIndex());

      transPathToAckState[key].amtReceived = transPathToAckState[key].amtReceived + aMsg->getAmount();
      /*
      if (transPathToAckState[key].amtReceived == transPathToAckState[key].amtSent){
         transPathToAckState.erase(key);
      }
      */

   }

   int destNode = ttmsg->getRoute()[0]; //destination of origin TransUnit job

   statNumCompleted[destNode] = statNumCompleted[destNode]+1;

   //broadcast completion time signal
   //ackMsg *aMsg = check_and_cast<ackMsg *>(ttmsg->getEncapsulatedPacket());
   simtime_t timeTakenInMilli = 1000*(simTime() - aMsg->getTimeSent());

   emit(completionTimeSignal, timeTakenInMilli);

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
   aMsg->setHasTimeOut(hasTimeOut);
   aMsg->setHtlcIndex(transMsg->getHtlcIndex());
   if (_useWaterfilling){
      aMsg->setPathIndex(transMsg->getPathIndex());
   }
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
   //ackMsg *aMsg = check_and_cast<ackMsg *>(msg->getEncapsulatedPacket());

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
   nodeToPaymentChannel[prevNode].balance =  nodeToPaymentChannel[prevNode].balance + uMsg->getAmount();

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
routerMsg *hostNode::generateUpdateMessage(int transId, int receiver, double amount){
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
   msg->setHasTimeOut(transMsg->getHasTimeOut()); //TODO: all waterfilling transactions do not have time out rn , transMsg->hasTimeOut);
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


void hostNode::splitTransactionForWaterfilling(routerMsg * ttmsg){
   transactionMsg *transMsg = check_and_cast<transactionMsg *>(ttmsg->getEncapsulatedPacket());
   int destNode = transMsg->getReceiver();
   double remainingAmt = transMsg->getAmount();

   map<int, double> pathMap = {}; //key is pathIdx, double is amt
   priority_queue<pair<double,int>> pq;
   for (auto iter: nodeToShortestPathsMap[destNode] ){
      int key = iter.first;
      double bottleneck = (iter.second).bottleneck;
      pq.push(make_pair(bottleneck, key)); //automatically sorts with biggest first index-element
      //cout << "pathLength: " << (iter.second).path.size() << endl;
   }

   double highestBal;
   double secHighestBal;
   double diffToSend;
   double amtToSend;
   int highestBalIdx;
   //cout << "beginning size: " << pq.size();
   //cout <<  "total amount: " << transMsg->getAmount();
   while(pq.size()>0 && remainingAmt > 0){
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
      amtToSend = min(remainingAmt/(pq.size()+1),diffToSend);

      for (auto p: pathMap){
         pathMap[p.first] = p.second + amtToSend;
         remainingAmt = remainingAmt - amtToSend;
         cout << "(" << p.first << "," << p.second << ")";


      }
      pathMap[highestBalIdx] = amtToSend;
      remainingAmt = remainingAmt - amtToSend;

   }


   //cout << "total amount: "<< transMsg->getAmount();


   // print how much was sent on each path
   /*
   for (auto p: pathMap){
       cout << "index: "<< p.first << "; bottleneck: " << nodeToShortestPathsMap[destNode][p.first].bottleneck << "; ";
       cout << "amtToSend: "<< p.second  << endl;
   }
    */
   transMsg->setAmount(remainingAmt);

   for (auto p: pathMap){
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
      routerMsg* waterMsg = generateWaterfillingTransactionMessage(amt, path, p.first, transMsg); //TODO:
      handleTransactionMessage(waterMsg);
   }
   //cout << "number of probes after water gen: " <<  nodeToShortestPathsMap[destNode].size();


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

   //check if transactionId is in canceledTransaction set
   int transactionId = transMsg->getTransactionId();


   auto iter = find_if(canceledTransactions.begin(),
         canceledTransactions.end(),
         [&transactionId](const tuple<int, simtime_t, int, int>& p)
         { return get<0>(p) == transactionId; });

   if ( iter!=canceledTransactions.end() ){
      //TODO: check to see if transactionId is in canceledTransactions set

      //delete yourself
      ttmsg->decapsulate();
      delete transMsg;
      delete ttmsg;
   }
   else{


      if(ttmsg->getHopCount() ==  ttmsg->getRoute().size()-1){ // are at last index of route
         int prevNode = ttmsg->getRoute()[ttmsg->getHopCount()-1];
         map<Id, double> *incomingTransUnits = &(nodeToPaymentChannel[prevNode].incomingTransUnits);
         (*incomingTransUnits)[make_tuple(transMsg->getTransactionId(), transMsg->getHtlcIndex())] = transMsg->getAmount();
         int transactionId = transMsg->getTransactionId();
         routerMsg* newMsg =  generateAckMessage(ttmsg);
         //forward ack message - no need to wait;

         //check if transactionId is in canceledTransaction set



         auto iter = find_if(canceledTransactions.begin(),
               canceledTransactions.end(),
               [&transactionId](const tuple<int, simtime_t, int, int>& p)
               { return get<0>(p) == transactionId; });

         if (iter!=canceledTransactions.end()){
            canceledTransactions.erase(iter);
         }

         forwardAckMessage(newMsg);
         return;

      }
      else{
         //is a self-message/at hop count = 0
         int destNode = transMsg->getReceiver();
         statNumAttempted[destNode] = statNumAttempted[destNode]+1;

         if(_useWaterfilling && (ttmsg->getRoute().size() == 0)){ //no route is specified -
            //means we need to break up into chunks and assign route




                 if (simTime() == transMsg->getTimeSent()){
                     destNodeToNumTransPending[destNode] = destNodeToNumTransPending[destNode] + 1;
                 }



                  if (nodeToShortestPathsMap.count(destNode) == 0 ){
                     vector<vector<int>> kShortestRoutes = getKShortestRoutes(transMsg->getSender(),destNode, _kValue);


                    cout << "after K Shortest Routes" << endl;
                     initializeProbes(kShortestRoutes, destNode);
                     cout << "after initialize probes" << endl;
                     scheduleAt(simTime() + 1, ttmsg);
                     return;
                  }
                  else{





            bool recent = probesRecent(nodeToShortestPathsMap[destNode]);

            if (recent){ // we made sure all the probes are back
               destNodeToNumTransPending[destNode] = destNodeToNumTransPending[destNode]-1;
               if (simTime() < (transMsg->getTimeSent()+ transMsg->getTimeOut())){
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
               else{
                   ttmsg->decapsulate();
                   delete transMsg;
                 delete ttmsg;

               }

               return;
            }
            else{ //if not recent
                if (destNodeToNumTransPending[destNode] == 1){
                   restartProbes(destNode);
                }
               scheduleAt(simTime() + 1, ttmsg);
               return;
            }
                  }
                  return;
         }
         int nextNode = ttmsg->getRoute()[hopcount+1];
         q = &(nodeToPaymentChannel[nextNode].queuedTransUnits);
         tuple<int,int > key = make_tuple(transMsg->getTransactionId(), transMsg->getHtlcIndex());
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
   if (transMsg->getAmount() >nodeToPaymentChannel[nextDest].balance){
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
      nodeToPaymentChannel[nextDest].balance = nodeToPaymentChannel[nextDest].balance - amt;

      //int transId = transMsg->getTransactionId();
      send(msg, nodeToPaymentChannel[nextDest].gate);
      return true;
   } //end else (transMsg->getAmount() >nodeToPaymentChannel[dest].balance)
}

