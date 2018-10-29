

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

   if (getIndex() == 0){  //main initialization for global parameters
      // instantiate all the transUnits that need to be sent

      // add all the transUnits into global list

      numNodes= 4;
      generate_trans_unit_list(trans_unit_list);

      //create "channels" map - from topology file
      //create "balances" map - from topology file

      generate_channels_balances_map(channels, balances );

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
   cout << getIndex() << endl;
    cout << "here, after initialization \n";
}

void routerNode::handleMessage(cMessage *msg)
{
   routerMsg *ttmsg = check_and_cast<routerMsg *>(msg);
   if (ttmsg->getMessageType()==1){
       handleAckMessage(ttmsg);
   }
   else if(ttmsg->getMessageType()==0){
       handleTransactionMessage(ttmsg);

   }
   else if(ttmsg->getMessageType()==2){
       handleUpdateMessage(ttmsg);

   }
}


/* handleAckMessage - passes on ack message if not at end of root else deletes it
 *      - generates and sends corresponding update message
 *      - erases transId on received ack msg from "want_ack_trans_units"
 *      - adds transId to "sent_ack_trans_units", list of transUnits waiting for
 *          balance update message with their transId (in forwardAckMessage())
 */
void routerNode::handleAckMessage(routerMsg* ttmsg){
    EV << "in handleAckMessage \n";
    //generate updateMsg
    int sender = ttmsg->getRoute()[ttmsg->getHopCount()-1];
    EV <<"need to send update message to "<< sender << "\n";
    // virtual routerMsg *generateUpdateMessage(int transId, int receiver, double amount);
     routerMsg* updateMsg =  generateUpdateMessage(ttmsg->getTransactionId(),sender, ttmsg->getAmount() );
     forwardUpdateMessage(updateMsg);
     // remove transId from want_ack_trans_units
     want_ack_trans_units.erase(ttmsg->getTransactionId());
    if(ttmsg->getHopCount() <  ttmsg->getRoute().size()-1){ // are not at last index of route
        forwardAckMessage(ttmsg);
    }
    else{
        //delete ack message
        ackMsg *aMsg = check_and_cast<ackMsg *>(ttmsg->getEncapsulatedPacket());
        ttmsg->decapsulate();
        delete aMsg;
        delete ttmsg;
    }
}

/*
 * generateAckMessage - called only when a transactionMsg reaches end of its path, keeps routerMsg casing of msg
 *   and reuses it to send ackMsg in reversed order of route
 */
routerMsg *routerNode::generateAckMessage(routerMsg* msg){
    transactionMsg *transMsg = check_and_cast<transactionMsg *>(msg->getEncapsulatedPacket());
    char msgname[30];
     sprintf(msgname, "receiver-%d-to-sender-%d ackMsg", transMsg->getReceiver(), transMsg->getSender());

     ackMsg *aMsg = new ackMsg(msgname);

     aMsg->setTransactionId(transMsg->getTransactionId());
     aMsg->setIsSuccess(true);
     //no need to set secret;
     vector<int> route=msg->getRoute();
     reverse(route.begin(), route.end());
     msg->setRoute(route);
     msg->setHopCount(0);
     msg->setMessageType(1); //now an ack message
     msg->decapsulate(); // remove encapsulated packet
     delete transMsg;
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
       EV << "Forwarding message " << msg << " on gate[" << nextDest << "]\n";
       int amt = msg->getAmount();
       //node_to_balance[nextDest] = node_to_balance[nextDest] - amt;

       int transId = msg->getTransactionId();
       int amount = msg->getAmount();
       sent_ack_trans_units[transId] = amount;
       EV << "before forwarding ack, nextDest is "<<nextDest<<" \n";
       send(msg, node_to_gate[nextDest]);
       EV << "after forwarding ack \n";
}

/*
 * handleUpdateMessage - called when receive update message, increment back funds, see if we can
 *      process more jobs with new funds, delete update message
 */
void routerNode::handleUpdateMessage(routerMsg* msg){
    deque<tuple<int, double , routerMsg *>> q;
    int sender = msg->getRoute()[msg->getHopCount()-1];
    //remove value from sent_ack_trans_units  --- note: sent_ack means want_update
    sent_ack_trans_units.erase(msg->getTransactionId());
    //increment the in flight funds back
    node_to_balance[sender] = node_to_balance[sender] + msg->getAmount();
    delete msg; //delete update message

    //see if we can send more jobs out
    q = node_to_queued_trans_units[sender];
   processTransUnits(sender, q);
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
      // rMsg->set route = get_route(transUnit.sender,transUnit.receiver) , hopcount = 0, encapIsAck = false;
      vector<int> route={getIndex(),receiver};
      rMsg->setRoute(route);
      rMsg->setHopCount(0);
      rMsg->setMessageType(2); //2 means nothing encapsulated inside
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
          EV << "Forwarding message " << msg << " on gate[" << nextDest << "]\n";
         send(msg, node_to_gate[nextDest]);
}


/* handleTransactionMessage - checks if message has arrived
 *      1. has arrived - turn transactionMsg into ackMsg, forward ackMsg
 *      2. has not arrived - add to appropriate job queue q, process q as
 *          much as we have funds for
 */
void routerNode::handleTransactionMessage(routerMsg* ttmsg){
    int hopcount = ttmsg->getHopCount();
       deque<tuple<int, double , routerMsg *>> q;

       if(ttmsg->getHopCount() ==  ttmsg->getRoute().size()-1){ // are at last index of route

         //ref: routerMsg *ttmsg = check_and_cast<routerMsg *>(msg);
          routerMsg* newMsg =  generateAckMessage(ttmsg);
        //forward ack message - no need to wait;
          forwardAckMessage(newMsg);
          /*
          //emit(arrivalSignal, hopcount);
          EV << "Message " << ttmsg << " arrived after " << hopcount << " hops.\n";
          bubble("Message arrived!");
          delete ttmsg;
          */
       }
       else{
          //displays string of balances remaining on connected channels
          bubble(string_node_to_balance().c_str());
          int nextStop = ttmsg->getRoute()[hopcount+1];
          q = node_to_queued_trans_units[nextStop];
          q.push_back(make_tuple(ttmsg->getPriorityClass(), ttmsg->getAmount(),
                   ttmsg));
          // re-sort queued transUnits for next stop based on lowest priority, then lowest amount
          sort(q.begin(), q.end(), sortFunction);
          processTransUnits(nextStop, q);
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
   // msg->set transactionId; "use .getId()"?

   sprintf(msgname, "tic-%d-to-%d routerMsg", transUnit.sender, transUnit.receiver);
   routerMsg *rMsg = new routerMsg(msgname);
   // rMsg->set route = get_route(transUnit.sender,transUnit.receiver) , hopcount = 0, encapIsAck = false;
   rMsg->setRoute(get_route(transUnit.sender,transUnit.receiver));
   rMsg->setHopCount(0);
   rMsg->setMessageType(0);
   rMsg->setAmount(transUnit.amount);
   rMsg->setPriorityClass(transUnit.priorityClass);
   rMsg->setTransactionId(msg->getId());
   //encapsulate msg inside rMsg
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
      q.pop_front();
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
   EV << "Forwarding message " << msg << " on gate[" << nextDest << "]\n";
   int amt = msg->getAmount();
   node_to_balance[nextDest] = node_to_balance[nextDest] - amt;
   int transId = msg->getTransactionId();
   want_ack_trans_units[transId] = amt;
   EV << "before forwarding, nextDest is "<<nextDest<<" \n";
   send(msg, node_to_gate[nextDest]);
   EV << "after forwarding \n";

}
