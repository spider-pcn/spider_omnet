

#include "initialize.h"
#include "routerNode.h"

Define_Module(routerNode);

/* string_node_to_balance - helper function, turns the node_to_balance map into printable string
 *
 */
string routerNode::string_node_to_balance(){
   string result = "";
   for(map<int,double>::iterator iter = node_to_balance.begin(); iter != node_to_balance.end(); ++iter)
   {
      int key =  iter->first;
      int value = iter->second;
      result = result + "("+to_string(key)+":"+to_string(value)+") ";
   }
   return result;
}


void routerNode:: print_message(routerMsg *msg){
    EV <<  "<message - transId:" << msg->getTransactionId() << ", sender: " << msg->getRoute()[0] << ", receiver: " << msg->getRoute()[msg->getRoute().size()-1];
    EV << ", amount: " << msg->getAmount() << "> \n";

}

void routerNode::print_private_values(){
    EV << getIndex()<< " - node_to_queued_trans_units:";
    typedef map<int, deque<tuple<int, double , routerMsg *>>>::const_iterator queueMapIter;
    for (queueMapIter iter = node_to_queued_trans_units.begin(); iter != node_to_queued_trans_units.end(); iter++)
       {
           EV << "{"<< iter->first << ": " ;
           typedef  deque<tuple<int, double , routerMsg *>>::const_iterator queueIter;
           for (queueIter iter_inner = iter->second.begin(); iter_inner != iter->second.end(); iter_inner++){
               EV << "(" << get<0>(*iter_inner);
               EV << "," << get<1>(*iter_inner) << ")   ";
           }
           EV << "}   ";
       }
       EV << endl;

    //print map - channel balance
    EV << getIndex() <<  " - node_to_balance: ";
    typedef map<int, double>::const_iterator MapIterator2;
    for (MapIterator2 iter_inner = node_to_balance.begin(); iter_inner != node_to_balance.end(); iter_inner++){
        EV << "(" << iter_inner->first << "," << iter_inner->second << ")   ";}

    EV << endl;

    EV << getIndex() << "- incoming_trans_units: ";

    typedef map<int, map<int, double>>::const_iterator MapIterator;
    for (MapIterator iter = incoming_trans_units.begin(); iter != incoming_trans_units.end(); iter++)
    {
        EV << "{"<< iter->first << ": " ;
        typedef map<int, double>::const_iterator MapIterator;
        for (MapIterator iter_inner = iter->second.begin(); iter_inner != iter->second.end(); iter_inner++)
            EV << "(" << iter_inner->first << "," << iter_inner->second << ")   ";
        EV << "}   ";
    }
    EV << endl;

    EV << getIndex() << " - outgoing_trans_units: ";
    typedef map<int, map<int, double>>::const_iterator MapIterator;
    for (MapIterator iter = outgoing_trans_units.begin(); iter != outgoing_trans_units.end(); iter++)
    {
        EV << "{"<< iter->first << ": " ;
        typedef map<int, double>::const_iterator MapIterator;
        for (MapIterator iter_inner = iter->second.begin(); iter_inner != iter->second.end(); iter_inner++)
            EV << "(" << iter_inner->first << "," << iter_inner->second << ")   ";
        EV << "}   ";

    }
    EV <<  endl;
}

/* initialize() -
 *  if node index is 0:
 *      - initialize global parameters: instantiate all transUnits, and add to global list "trans_unit_list"
 *      - create "channels" map - adjacency list representation of network
 *      - create "balances" map - map representing initial balances; each key is directed edge (source, dest)
 *          and value is balance
 *  all nodes:
 *      - find my transUnits from "trans_unit_list" and store them into my private list "my_trans_units"
 *      - create "node_to_gate" map - map to identify channels, each key is node index adjacent to this node
 *          value is gate connecting to that adjacent node
 *      - create "node_to_balances" map - map to identify remaining outgoing balance for gates/channels connected
 *          to this node (key is adjacent node index; value is remaining outgoing balance)
 *      - create "node_to_queued_trans_units" map - map to get transUnit queue for outgoing transUnits,
 *          one queue corresponding to each adjacent node/each connected channel
 *      - send all transUnits in my_trans_units as a message (using generateTransactionMessage), sent to myself (node index),
 *          scheduled to arrive at timeSent parameter field in transUnit
 */


void routerNode::initialize()
{
    cout << "here, before initialization" << endl;
    //statNumProcessed = 0;

    //register signals
    //numInQueueSignal = registerSignal("numInQueue");
    //numProcessedSignal = registerSignal("numProcessed");
    completionTimeSignal = registerSignal("completionTime");






   if (getIndex() == 0){  //main initialization for global parameters
      // instantiate all the transUnits that need to be sent

      // add all the transUnits into global list

      set_num_nodes();
      //numNodes= 4;
      generate_trans_unit_list(trans_unit_list);

      //create "channels" map - from topology file
      //create "balances" map - from topology file

      generate_channels_balances_map(channels, balances );  //also sets numNodes
      //dijkstra(0,0);

      //dijkstra(1,0);

   }


   for (int i = 0; i < numNodes; ++i) {
        char signalName[64];
        sprintf(signalName, "numInQueuePerChannel(node %d)", i);
        simsignal_t signal = registerSignal(signalName);
        cProperty *statisticTemplate = getProperties()->get("statisticTemplate", "numInQueuePerChannelTemplate");
        getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
        numInQueuePerChannelSignals.push_back(signal);
    }


   for (int i = 0; i < numNodes; ++i) {
         char signalName[64];
         sprintf(signalName, "numProcessedPerChannel(node %d)", i);
         simsignal_t signal = registerSignal(signalName);
         cProperty *statisticTemplate = getProperties()->get("statisticTemplate", "numProcessedPerChannelTemplate");
         getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
         numProcessedPerChannelSignals.push_back(signal);
     }

   //initialize statNumProcessed vector
   for (int i=0 ; i<numNodes; i++){
       statNumProcessed.push_back(0);
   }

   for (int i = 0; i < numNodes; ++i) {
            char signalName[64];
            sprintf(signalName, "numCompletedPerDest_Total(dest node %d)", i);
            simsignal_t signal = registerSignal(signalName);
            cProperty *statisticTemplate = getProperties()->get("statisticTemplate", "numCompletedPerDestTemplate");
            getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
            numCompletedPerDestSignals.push_back(signal);
        }
   //initialize statNumCompleted vector
   for (int i=0 ; i<numNodes; i++){
       statNumCompleted.push_back(0);
   }


   for (int i = 0; i < numNodes; ++i) {
               char signalName[64];
               sprintf(signalName, "numAttemptedPerDest_Total(dest node %d)", i);
               simsignal_t signal = registerSignal(signalName);
               cProperty *statisticTemplate = getProperties()->get("statisticTemplate", "numAttemptedPerDestTemplate");
               getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
               numAttemptedPerDestSignals.push_back(signal);
           }
      //initialize statNumAttempted vector
      for (int i=0 ; i<numNodes; i++){
          statNumAttempted.push_back(0);
      }


      for (int i = 0; i < numNodes; ++i) {
                    char signalName[64];
                    sprintf(signalName, "balancePerChannel(node %d)", i);
                    simsignal_t signal = registerSignal(signalName);
                    cProperty *statisticTemplate = getProperties()->get("statisticTemplate", "balancePerChannelTemplate");
                    getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
                    balancePerChannelSignals.push_back(signal);
                }
      //initialize statNumSent vector
      for (int i=0 ; i<numNodes; i++){
             statNumSent.push_back(0);
         }


         for (int i = 0; i < numNodes; ++i) {
                     char signalName[64];
                     sprintf(signalName, "numSentPerChannel(node %d)", i);
                     simsignal_t signal = registerSignal(signalName);
                     cProperty *statisticTemplate = getProperties()->get("statisticTemplate", "numSentPerChannelTemplate");
                     getEnvir()->addResultRecorders(this, signal, signalName,  statisticTemplate);
                     numSentPerChannelSignals.push_back(signal);
                 }


   cout << getIndex() << endl;
   cout << "here, after initialization \n";

   //iterate through the global trans_unit_list and find my transUnits
   for (int i=0; i<(int)trans_unit_list.size(); i++){
      if (trans_unit_list[i].sender == getIndex()){
         my_trans_units.push_back(trans_unit_list[i]);
      }
   }

   //create "node_to_gate" map
   const char * gateName = "out";
   cGate *destGate = NULL;

   int i = 0;
   int gateSize = gate(gateName, 0)->size();

   do {
      destGate = gate(gateName, i++);
      cGate *nextGate = destGate->getNextGate();
      if (nextGate ) {
         node_to_gate[nextGate->getOwnerModule()->getIndex()] = destGate;
      }
   } while (i < gateSize);

   //create "node_to_balance" map
   for(map<int,cGate *>::iterator iter = node_to_gate.begin(); iter != node_to_gate.end(); ++iter)
   {
      int key =  iter->first;
      node_to_balance[key] = balances[make_tuple(getIndex(),key)];
   }

   WATCH_MAP(node_to_balance);

   //create "node_to_queued_trans_units" map
   for(map<int,cGate *>::iterator iter = node_to_gate.begin(); iter != node_to_gate.end(); ++iter)
   {
      int key =  iter->first;
      deque<tuple<int, double , routerMsg *>> temp;
      node_to_queued_trans_units[key] = temp;
   }
   //arrivalSignal = registerSignal("arrival");
   //implementing timeSent parameter, send all msgs at beginning
   for (int i=0 ; i<(int)my_trans_units.size(); i++){
      transUnit j = my_trans_units[i];
      double timeSent = j.timeSent;
      routerMsg *msg = generateTransactionMessage(j);
      scheduleAt(timeSent, msg);
   }
   //set statRate
   statRate = 0.5;
   //get stat message
   routerMsg *statMsg = generateStatMessage();
   scheduleAt(0, statMsg);



   cout << getIndex() << endl;
    cout << "here, after initialization \n";
}

void routerNode::handleMessage(cMessage *msg)
{
   routerMsg *ttmsg = check_and_cast<routerMsg *>(msg);
   if (ttmsg->getMessageType()==1){
      // EV<< "[NODE "<< getIndex() <<": RECEIVED ACK MSG] \n";
       //print_message(ttmsg);
       //print_private_values();
       handleAckMessage(ttmsg);
      // EV<< "[AFTER HANDLING:]\n";
      // print_private_values();
   }
   else if(ttmsg->getMessageType()==0){
      // EV<< "[NODE "<< getIndex() <<": RECEIVED TRANSACTION MSG]  \n";
      // print_message(ttmsg);
      // print_private_values();
       handleTransactionMessage(ttmsg);
      // EV<< "[AFTER HANDLING:] \n";
      // print_private_values();
   }
   else if(ttmsg->getMessageType()==2){
     //  EV<< "[NODE "<< getIndex() <<": RECEIVED UPDATE MSG] \n";
      // print_message(ttmsg);
      // print_private_values();
       handleUpdateMessage(ttmsg);
      // EV<< "[AFTER HANDLING:]  \n";
       //print_private_values();

   }
   else if (ttmsg->getMessageType() ==3){
       handleStatMessage(ttmsg);
   }
}


routerMsg *routerNode::generateStatMessage(){
    char msgname[30];
      sprintf(msgname, "node-%d statMsg", getIndex());

      routerMsg *rMsg = new routerMsg(msgname);

      rMsg->setMessageType(3); //3 means statMsg
      return rMsg;
}



void routerNode::handleStatMessage(routerMsg* ttmsg){
    if (simTime() >30){
        delete ttmsg;

    }
    else{
    scheduleAt(simTime()+statRate, ttmsg);
    }

    //

    int statNumInQueue = 0;
    //  map<int, deque<tuple<int, double , routerMsg *>>> node_to_queued_trans_units;
    for ( map<int, deque<tuple<int, double , routerMsg *>>>::iterator it = node_to_queued_trans_units.begin();
           it!= node_to_queued_trans_units.end(); it++){
        //EV << "node "<< getIndex() << ": queued "<<(it->second).size();
        statNumInQueue = statNumInQueue + (it->second).size();
        emit(numInQueuePerChannelSignals[it->first], (it->second).size());

    }

   // map<int, double> node_to_balance;

    //emit balancePerChannelSignals
    for ( map<int, double>::iterator it = node_to_balance.begin();
             it!= node_to_balance.end(); it++){
          //EV << "node "<< getIndex() << ": queued "<<(it->second).size();
          //statNumInQueue = statNumInQueue + (it->second).size();
          emit(balancePerChannelSignals[it->first], it->second);

      }



    //emit num processed signals
    printf("numInQueue %i \n", statNumInQueue);

    for (int i=0; i<numNodes; i++){
        emit(numProcessedPerChannelSignals[i], statNumProcessed[i]);
        statNumProcessed[i] = 0;
    }


    //emit num sent per channel signals
    for (int i=0; i<numNodes; i++){
          emit(numSentPerChannelSignals[i], statNumSent[i]);
          statNumSent[i] = 0;
    }

    //EV << "node "<< getIndex() << ": numProcessed "<< statNumProcessed;
    //int arrSignal[2] = {statNumInQueue};
    //int arrSignal[4] = {10,20,30,40};



    //emit(numInQueueSignal, statNumInQueue);
    //emit(numProcessedSignal, statNumProcessed);


    //statNumProcessed = 0;

    //send out statistics here
}


/* handleAckMessage - passes on ack message if not at end of root else deletes it
 *      - generates and sends corresponding update message
 *      - erases transId on received ack msg from "want_ack_trans_units"
 *      - adds transId to "sent_ack_trans_units", list of transUnits waiting for
 *          balance update message with their transId (in forwardAckMessage())
 */
void routerNode::handleAckMessage(routerMsg* ttmsg){
    //generate updateMsg
    int sender = ttmsg->getRoute()[ttmsg->getHopCount()-1];

    //remove transaction from outgoing_trans_unit
    outgoing_trans_units[sender].erase(ttmsg->getTransactionId());

    //increment signal numProcessed
    statNumProcessed[sender] = statNumProcessed[sender]+1;

    // virtual routerMsg *generateUpdateMessage(int transId, int receiver, double amount);
     routerMsg* updateMsg =  generateUpdateMessage(ttmsg->getTransactionId(),sender, ttmsg->getAmount() );
     forwardUpdateMessage(updateMsg);
     // remove transId from want_ack_trans_units
     want_ack_trans_units.erase(ttmsg->getTransactionId());
    if(ttmsg->getHopCount() <  ttmsg->getRoute().size()-1){ // are not at last index of route
        forwardAckMessage(ttmsg);
    }
    else{


        int destNode = ttmsg->getRoute()[0]; //destination of origin transUnit job

        statNumCompleted[destNode] = statNumCompleted[destNode]+1;

        //broadcast completion time signal
        ackMsg *aMsg = check_and_cast<ackMsg *>(ttmsg->getEncapsulatedPacket());
        simtime_t timeTakenInMilli = 1000*(simTime() - aMsg->getTimeSent());

        emit(completionTimeSignal, timeTakenInMilli);


        emit(numCompletedPerDestSignals[destNode], statNumCompleted[destNode]);


        //delete ack message

        ttmsg->decapsulate();
        delete aMsg;
        delete ttmsg;
    }
}

/*
 * generateAckMessage - called only when a transactionMsg reaches end of its path, keeps routerMsg casing of msg
 *   and reuses it to send ackMsg in reversed order of route
 */
routerMsg *routerNode::generateAckMessage(routerMsg* ttmsg){
    transactionMsg *transMsg = check_and_cast<transactionMsg *>(ttmsg->getEncapsulatedPacket());
    char msgname[30];
     sprintf(msgname, "receiver-%d-to-sender-%d ackMsg", transMsg->getReceiver(), transMsg->getSender());
     routerMsg *msg = new routerMsg(msgname);
     ackMsg *aMsg = new ackMsg(msgname);
     aMsg->setTransactionId(transMsg->getTransactionId());
     aMsg->setIsSuccess(true);
     aMsg->setTimeSent(transMsg->getTimeSent());
     //no need to set secret;
     vector<int> route=ttmsg->getRoute();
     reverse(route.begin(), route.end());
     msg->setRoute(route);
     msg->setHopCount(0);
     msg->setAmount(ttmsg->getAmount());
     msg->setMessageType(1); //now an ack message
     ttmsg->decapsulate(); // remove encapsulated packet
     delete transMsg;
     delete ttmsg;
     msg->encapsulate(aMsg);
    return msg;
}


/*
 * forwardAckMessage - called inside handleAckMsg, sends ackMsg to next destination, and
 *      adds transId to sent_ack_trans_units list
 */
void routerNode::forwardAckMessage(routerMsg *msg){
    // Increment hop count.
       msg->setHopCount(msg->getHopCount()+1);
       //use hopCount to find next destination
       int nextDest = msg->getRoute()[msg->getHopCount()];

       int amt = msg->getAmount();


       int transId = msg->getTransactionId();
       int amount = msg->getAmount();
       sent_ack_trans_units[transId] = amount;

       send(msg, node_to_gate[nextDest]);

}

/*
 * handleUpdateMessage - called when receive update message, increment back funds, see if we can
 *      process more jobs with new funds, delete update message
 */
void routerNode::handleUpdateMessage(routerMsg* msg){
    deque<tuple<int, double , routerMsg *>> *q;
    int sender = msg->getRoute()[msg->getHopCount()-1];
    //remove value from sent_ack_trans_units  --- note: sent_ack means want_update
    sent_ack_trans_units.erase(msg->getTransactionId());
    //increment the in flight funds back

    print_private_values();

    node_to_balance[sender] = node_to_balance[sender] + msg->getAmount();
    EV <<"ADDED FUNDS BACK, sender: " <<sender << ", amount: "<< msg->getAmount() << endl;

    print_private_values();
    //remove transaction from incoming_trans_units

    incoming_trans_units[sender].erase(msg->getTransactionId());
    //increment numProcessed signal
    statNumProcessed[sender] = statNumProcessed[sender]+1;
    //statNumProcessed++;


    delete msg; //delete update message

    //see if we can send more jobs out
    q = &node_to_queued_trans_units[sender];

   processTransUnits(sender, *q);

}


/*
 *  generateUpdateMessage - creates new message to send as updateMessage
 *      - note: all update messages have route length of exactly 2
 *
 */
// virtual routerMsg *generateUpdateMessage(int transId, int receiver, double amount);
routerMsg *routerNode::generateUpdateMessage(int transId, int receiver, double amount){
    char msgname[30];
      sprintf(msgname, "tic-%d-to-%d updateMsg", getIndex(), receiver);

      routerMsg *rMsg = new routerMsg(msgname);

      vector<int> route={getIndex(),receiver};
      rMsg->setRoute(route);
      rMsg->setHopCount(0);
      rMsg->setMessageType(2); //2 means nothing encapsulated inside
     //EV << "generateUpdateMessage with AMOUNT: "<< amount <<endl;
      rMsg->setAmount(amount);
      rMsg->setTransactionId(transId);
      return rMsg;
}

/*
 * forwardUpdateMessage - sends updateMessage to appropriate destination
 */
void routerNode::forwardUpdateMessage(routerMsg *msg){
    // Increment hop count.
          msg->setHopCount(msg->getHopCount()+1);
          //use hopCount to find next destination
          int nextDest = msg->getRoute()[msg->getHopCount()];
         send(msg, node_to_gate[nextDest]);
}


/* handleTransactionMessage - checks if message has arrived
 *      1. has arrived - turn transactionMsg into ackMsg, forward ackMsg
 *      2. has not arrived - add to appropriate job queue q, process q as
 *          much as we have funds for
 */
void routerNode::handleTransactionMessage(routerMsg* ttmsg){
    int hopcount = ttmsg->getHopCount();
       deque<tuple<int, double , routerMsg *>> *q;


       //add to incoming_trans_units
       if ((ttmsg->getHopCount())>0){ //not a self-message
               int sender = ttmsg->getRoute()[ttmsg->getHopCount()-1];

                     if ( incoming_trans_units.find(sender) == incoming_trans_units.end() ) {
                         map<int,double> tempMap = {};
                         tempMap[ttmsg->getTransactionId()]= ttmsg->getAmount();
                         incoming_trans_units[sender] = tempMap;
                       // not found
                     } else {

                         incoming_trans_units[sender][ttmsg->getTransactionId()] = ttmsg->getAmount();
                       // found
                     }
       }
       else{ //is a self-message
           int destNode = ttmsg->getRoute()[ttmsg->getRoute().size()-1];
           statNumAttempted[destNode] = statNumAttempted[destNode]+1;
           emit(numAttemptedPerDestSignals[destNode], statNumAttempted[destNode]);
       }

       if(ttmsg->getHopCount() ==  ttmsg->getRoute().size()-1){ // are at last index of route

          routerMsg* newMsg =  generateAckMessage(ttmsg);
        //forward ack message - no need to wait;
          forwardAckMessage(newMsg);
       }
       else{
          //displays string of balances remaining on connected channels
          bubble(string_node_to_balance().c_str());
          int nextStop = ttmsg->getRoute()[hopcount+1];
          q = &node_to_queued_trans_units[nextStop];
          q->push_back(make_tuple(ttmsg->getPriorityClass(), ttmsg->getAmount(),
                   ttmsg));
          // re-sort queued transUnits for next stop based on lowest priority, then lowest amount
          sort(q->begin(), q->end(), sortFunction);
          //EV << "added to job queue:" <<endl;
          print_private_values();
          processTransUnits(nextStop, *q);
          bubble(string_node_to_balance().c_str());
       }
}


/* generateTransactionMessage - takes in a transUnit object and returns corresponding routerMsg message
 *      with encapsulated transactionMsg inside.
 *      note: calls get_route function to get route from sender to receiver
 */
routerMsg *routerNode::generateTransactionMessage(transUnit transUnit)
{
   char msgname[30];
   sprintf(msgname, "tic-%d-to-%d transactionMsg", transUnit.sender, transUnit.receiver);
   transactionMsg *msg = new transactionMsg(msgname);
   msg->setAmount(transUnit.amount);
   msg->setTimeSent(transUnit.timeSent);
   msg->setSender(transUnit.sender);
   msg->setReceiver(transUnit.receiver);
   msg->setPriorityClass(transUnit.priorityClass);
   msg->setTransactionId(msg->getId());

   sprintf(msgname, "tic-%d-to-%d routerMsg", transUnit.sender, transUnit.receiver);
   routerMsg *rMsg = new routerMsg(msgname);
   rMsg->setRoute(get_route(transUnit.sender,transUnit.receiver));
   rMsg->setHopCount(0);
   rMsg->setMessageType(0);
   rMsg->setAmount(transUnit.amount);
   rMsg->setPriorityClass(transUnit.priorityClass);
   rMsg->setTransactionId(msg->getId());

   rMsg->encapsulate(msg);
   return rMsg;
}


/*
 * processTransUnits - given an adjacent node, and transUnit queue of things to send to that node, sends
 *  transUnits until channel funds are too low by calling forwardMessage on message representing transUnit
 */
void routerNode:: processTransUnits(int dest, deque<tuple<int, double , routerMsg *>>& q){
   while((int)q.size()>0 && get<1>(q[0])<=node_to_balance[dest]){

      forwardTransactionMessage(get<2>(q[0]));
        print_private_values();
      q.pop_front();
     // EV << "processed something" <<endl;
      print_private_values();

   }
}


/*
 *  forwardTransactionMessage - given a message representing a transUnit, increments hopCount, finds next destination,
 *      adjusts (decrements) channel balance, sends message to next node on route
 */
void routerNode::forwardTransactionMessage(routerMsg *msg)
{
   // Increment hop count.
   msg->setHopCount(msg->getHopCount()+1);
   //use hopCount to find next destination
   int nextDest = msg->getRoute()[msg->getHopCount()];
   //add amount to outgoing map

   if ( outgoing_trans_units.find(nextDest) == outgoing_trans_units.end() ) {
             map<int,double> tempMap = {};
             tempMap[msg->getTransactionId()]= msg->getAmount();
             outgoing_trans_units[nextDest] = tempMap;
           // not found
     } else {
             outgoing_trans_units[nextDest][msg->getTransactionId()] = msg->getAmount();
           // found
    }

   //numSentPerChannel incremented every time (key,value) pair added to outgoing_trans_units map
   statNumSent[nextDest] = statNumSent[nextDest]+1;

   int amt = msg->getAmount();
   node_to_balance[nextDest] = node_to_balance[nextDest] - amt;
   int transId = msg->getTransactionId();
   want_ack_trans_units[transId] = amt;

   send(msg, node_to_gate[nextDest]);

}
