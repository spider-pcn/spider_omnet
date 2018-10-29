#include <stdio.h>
#include <string.h>
#include <omnetpp.h>
#include <vector>
#include "routerMsg_m.h"
#include "transactionMsg_m.h"
#include "ackMsg_m.h"
#include <iostream>
#include <algorithm>
#include <string>
#include <sstream>
#include <deque>
#include <map>
#include <fstream>

using namespace omnetpp;

using namespace std;

class transUnit{
   public:
      double amount;
      double timeSent;  //time delay after start time of simulation that trans-unit is active
      int sender;
      int receiver;
      //vector<int> route; //includes sender and reciever as first and last entries
      int priorityClass;

      transUnit(double amount1, double timeSent1, int sender1, int receiver1, int priorityClass1){
         amount = amount1;
         timeSent = timeSent1;
         sender = sender1;
         receiver = receiver1;
         priorityClass=  priorityClass1;
      }
};

//global parameters
vector<transUnit> trans_unit_list; //list of all transUnits
int numNodes = 4; //number of nodes in network
map<int, vector<int>> channels; //adjacency list format of graph edges of network
map<tuple<int,int>,double> balances;
//map of balances for each edge; key = <int,int> is <source, destination>

vector<int> breadthFirstSearch(int sender, int receiver){
    deque<vector<int>> nodesToVisit;
    bool visitedNodes[numNodes];
    for (int i=0; i<numNodes; i++){
        visitedNodes[i] =false;
    }
    visitedNodes[sender] = true;

    vector<int> temp;
    temp.push_back(sender);
    nodesToVisit.push_back(temp);

    while ((int)nodesToVisit.size()>0){

        vector<int> current = nodesToVisit[0];
         nodesToVisit.pop_front();
        int lastNode = current.back();
        for (int i=0; i<(int)channels[lastNode].size();i++){

            if (!visitedNodes[channels[lastNode][i]]){
                temp = current; // assignment copies in case of vector
                temp.push_back(channels[lastNode][i]);
                nodesToVisit.push_back(temp);
                visitedNodes[channels[lastNode][i]] = true;

                if (channels[lastNode][i]==receiver){

                    return temp;
                } //end if (channels[lastNode][i]==receiver)
            } //end if (!visitedNodes[channels[lastNode][i]])
         }//end for (i)
      }//end while
   vector<int> empty;
   return empty;
}

/*get_route- take in sender and receiver graph indicies, and returns
 *  BFS shortest path from sender to receiver in form of node indicies,
 *  includes sender and reciever as first and last entry
 */
vector<int> get_route(int sender, int receiver){
  //do searching without regard for channel capacities, DFS right now

  // printf("sender: %i; receiver: %i \n [", sender, receiver);
   vector<int> route =  breadthFirstSearch(sender, receiver);
/*
   for (int i=0; i<(int)route.size(); i++){
        printf("%i, ", route[i]);
    }
    printf("] \n");
*/
    return route;

}


class routerNode : public cSimpleModule
{
   private:
      //simsignal_t arrivalSignal;
      vector< transUnit > my_trans_units; //list of transUnits that have me as sender
      map<int, cGate*> node_to_gate; //map that takes in index of node adjacent to me, returns gate to that node
      map<int, double> node_to_balance; //map takes in index of adjacent node, returns outgoing balance
      map<int, deque<tuple<int, double , routerMsg *>>> node_to_queued_trans_units;
      map<int, double> want_ack_trans_units;
      map<int, double> sent_ack_trans_units;
          //map takes in index of adjacent node, returns queue of trans_units to send to that node
   protected:
      virtual routerMsg *generateTransactionMessage(transUnit transUnit);
      virtual routerMsg *generateAckMessage(routerMsg *msg);
      virtual routerMsg *generateUpdateMessage(int transId, int receiver, double amount);
          //just generates routerMsg with no encapsulated msg inside
      virtual void initialize() override;
      virtual void handleMessage(cMessage *msg) override;
      virtual void handleTransactionMessage(routerMsg *msg);
      virtual void handleAckMessage(routerMsg *msg);
      virtual void handleUpdateMessage(routerMsg *msg);
      virtual void forwardTransactionMessage(routerMsg *msg);
      virtual void forwardAckMessage(routerMsg *msg);
      virtual void forwardUpdateMessage(routerMsg *msg);
      virtual void processTransUnits(int dest, deque<tuple<int, double , routerMsg *>>& q);
      virtual string string_node_to_balance();
};
Define_Module(routerNode);

/* string_node_to_balance - helper function, turns the node_to_balance map into printable string
 *
 */
string routerNode:: string_node_to_balance(){
   string result = "";
   for(map<int,double>::iterator iter = node_to_balance.begin(); iter != node_to_balance.end(); ++iter)
   {
      int key =  iter->first;
      int value = iter->second;
      result = result + "("+to_string(key)+":"+to_string(value)+") ";
   }
   return result;
}


vector<string> split(string str, char delimiter) {
  vector<string> internal;
  stringstream ss(str); // Turn the string into a stream.
  string tok;

  while(getline(ss, tok, delimiter)) {
    internal.push_back(tok);
  }
  return internal;
}

/* generate_channels_balances_map - reads from file and constructs adjacency list of graph topology (channels), and hash map
 *      of directed edges to initial balances, modifies global maps in place
 *      each line of file is of form
 *      [node1] [node2] [1->2 delay] [2->1 delay] [balance at node1 end] [balance at node2 end]
 */
void generate_channels_balances_map(map<int, vector<int>> &channels, map<tuple<int,int>,double> &balances){
      string line;
      ifstream myfile ("sample-topology.txt");
      if (myfile.is_open())
      {
        while ( getline (myfile,line) )
        {
          vector<string> data = split(line, ' ');
           for (int i=0; i<data.size(); i++){
               cout << data[i] << "\n";

           }
          //generate channels - adjacency map
          int node1 = stoi(data[0]); //
          int node2 = stoi(data[1]); //
          if (channels.count(node1)==0){ //node 1 is not in map
              vector<int> tempVector = {node2};
              channels[node1] = tempVector;
          }
          else{ //(node1 is in map)
              if ( find(channels[node1].begin(), channels[node1].end(), node2) == channels[node1].end() ){ //(node2 is not already in node1's adjacency list')
                 channels[node1].push_back(node2);
              }
          }

          if (channels.count(node2)==0){ //node 1 is not in map
                      vector<int> tempVector = {node1};
                      channels[node2] = tempVector;
                  }
                  else{ //(node1 is in map)
                      if ( find(channels[node2].begin(), channels[node2].end(), node1) == channels[node2].end() ){ //(node2 is not already in node1's adjacency list')
                         channels[node2].push_back(node1);
                      }
                  }
          //generate balances map
          double balance1 = stod( data[4]);
          double balance2 = stod( data[5]);
          balances[make_tuple(node1,node2)] = balance1;
          balances[make_tuple(node2,node1)] = balance2;
        }
        myfile.close();
      }

      else cout << "Unable to open file";

    return;
}
/*
 *  generate_trans_unit_list - reads from file and generates global transaction unit job list.
 *      each line of file is of form:
 *      [amount] [timeSent] [sender] [receiver] [priorityClass]
 */
void generate_trans_unit_list(vector<transUnit> &trans_unit_list){
          string line;
          ifstream myfile ("sample-workload.txt");
          if (myfile.is_open())
          {
            while ( getline (myfile,line) )
            {
              vector<string> data = split(line, ' ');
              for (int i=0; i< data.size(); i++){
                  cout<< data[i]<<endl;
              }
              //data[0] = amount, data[1] = timeSent, data[2] = sender, data[3] = receiver, data[4] = priority class
              double amount = stod(data[0]);
              double timeSent = stod(data[1]);
              int sender = stoi(data[2]);
              int receiver = stoi(data[3]);
              int priorityClass = stoi(data[4]);

              // instantiate all the transUnits that need to be sent
              transUnit tempTU = transUnit(amount, timeSent, sender, receiver, priorityClass);

                 // add all the transUnits into global list
              trans_unit_list.push_back(tempTU);

            }
            myfile.close();
          }

          else cout << "Unable to open file";
          return;

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

routerMsg *routerNode::generateAckMessage(routerMsg* msg){
    transactionMsg *transMsg = check_and_cast<transactionMsg *>(msg->getEncapsulatedPacket());
    char msgname[30];
     sprintf(msgname, "receiver-%d-to-sender-%d ackMsg", transMsg->getReceiver(), transMsg->getSender());

     ackMsg *aMsg = new ackMsg(msgname);
     /*
     virtual void setTransactionId(int transactionId);
        virtual bool getIsSuccess() const;
        virtual void setIsSuccess(bool isSuccess);
        virtual const char * getSecret() const;
        virtual void setSecret(const char * secret);
     */
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


/* generateTransactionMessage - takes in a transUnit object and returns corresponding routerMsg message;
 *      transUnit and routerMsg have the same fields except routerMsg has extra field "hopCount",
 *      representing distance traveled so far
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
 * sortFunction - helper function used to sort queued transUnit list by ascending priorityClass, then by
 *      ascending amount
 */
bool sortFunction(const tuple<int,double, routerMsg*> &a,
      const tuple<int,double, routerMsg*> &b)
{
   if (get<0>(a) < get<0>(b)){
      return true;
   }
   else if (get<0>(a) == get<0>(b)){
      return (get<1>(a) < get<1>(b));
   }
   return false;
}

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


/* handleMessage -
 *    1. checks if message is self-message (sent from initialize function); if not self-message, DO NOT re-adjust
 *      (increment) balance, DO NOT send out jobs if possible to node that just sent me units (call processTransUnits)
 *    2. checks if message is has arrived. If has arrived delete message. If not arrived, add new job to queue,
 *       send out jobs if possible to node that is the next node of message I received
 *          - deals with checking if transUnit queue is empty, then only send if enough balance left
 */
void routerNode::handleTransactionMessage(routerMsg* ttmsg){
    int hopcount = ttmsg->getHopCount();
       deque<tuple<int, double , routerMsg *>> q;

       if (!(ttmsg -> isSelfMessage())){
          int hopcount = ttmsg->getHopCount();
          int sender = ttmsg->getRoute()[hopcount-1];
          //NOTE: do not increment amount back, implementing in-flight funds
          /*
          node_to_balance[sender] = node_to_balance[sender] + ttmsg->getAmount();
          */
          //NOTE: do not see if any new messages can be sent out based on priority for the channel that just came in
          /*
          q = node_to_queued_trans_units[sender];
          processTransUnits(sender, q);
          */
       }

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


void routerNode::forwardUpdateMessage(routerMsg *msg){
    // Increment hop count.
          msg->setHopCount(msg->getHopCount()+1);
          //use hopCount to find next destination
          int nextDest = msg->getRoute()[msg->getHopCount()];
          EV << "Forwarding message " << msg << " on gate[" << nextDest << "]\n";
         send(msg, node_to_gate[nextDest]);
}


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
 *  forwardMessage - given a message representing a transUnit, increments hopCount, finds next destination,
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
