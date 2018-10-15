#include <stdio.h>
#include <string.h>
#include <omnetpp.h>
#include <vector>
#include "simpleMsg_m.h"
#include <deque>
#include <map>

using namespace omnetpp;

using namespace std;



class Job{
public:
    double amount;
    double timeSent;  //time after start time that job is active
    int sender;
    int receiver;
    vector<int> route;
    int priorityClass;

    Job(double amount1, double timeSent1, int sender1, int receiver1, int priorityClass1){
      amount = amount1;
      timeSent = timeSent1;
      sender = sender1;
      receiver = receiver1;
      priorityClass=  priorityClass1;
    }
};
//global parameters

vector<Job> job_list;
int numNodes = 4;
map<int, vector<int>> channels;
map<tuple<int,int>,double> capacities; // where <int,int> is <source, destination>

vector<int> breathFirstSearch(int sender, int receiver){
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
                }
            }
        }

    }
    vector<int> empty;
    return empty; //returning empty
}

vector<int> get_route(int sender, int receiver){
  //do searching without regard for channel capacities, DFS right now

  // printf("sender: %i; receiver: %i \n [", sender, receiver);
   vector<int> route =  breathFirstSearch(sender, receiver);
/*
   for (int i=0; i<(int)route.size(); i++){
        printf("%i, ", route[i]);
    }
    printf("] \n");
*/
    return route;

}


class simpleNode : public cSimpleModule
{
private:
    simsignal_t arrivalSignal;
    vector< Job > my_jobs;
    map<int, cGate*> node_to_gate; //key is node index,
    map<int, double> node_to_capacity;
    map<int, deque<tuple<int, double , simpleMsg *>>> node_to_queued_jobs;
protected:
    virtual simpleMsg *generateMessage(Job job);
    //virtual void forwardMessage(simpleMsg *msg);
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void forwardMessage(simpleMsg *msg);
    virtual void processJobs(int dest, deque<tuple<int, double , simpleMsg *>>& q);
    //forwardMessage called only when guaranteed enough channel funds to get across
    virtual string string_node_to_capacity();
};
Define_Module(simpleNode);

string simpleNode:: string_node_to_capacity(){
    //initialize node_to_queued_jobs
        string result = "";
         for(map<int,double>::iterator iter = node_to_capacity.begin(); iter != node_to_capacity.end(); ++iter)
         {
              int key =  iter->first;
              int value = iter->second;
              result = result + "("+to_string(key)+":"+to_string(value)+") ";
              //ignore value
              //Value v = iter->second;
        }
        return result;

}


void simpleNode::initialize()
{

    std::cout << "in initialize \n";
    EV << "\n EV in initialize \n";
    if (getIndex() == 0){    //main initialization for global parameters

        // create job queue
        Job j1 = Job(3,0,0,3,0);
        Job j2 = Job(5,0.25,2,0,0);
        Job j3 = Job(5,0.25,2,0,0);
        Job j4 = Job(5,0.25,2,0,0);
        Job j5 = Job(3,0.5,0,3,0);
        Job j6 = Job(3,0.75,0,3,0);
        job_list.push_back(j1);
        job_list.push_back(j2);
        job_list.push_back(j3);
        job_list.push_back(j4);
        job_list.push_back(j5);
        job_list.push_back(j6);

        //create channels map - graph of edges
        //note: all edges bidirectional
        vector<int>v0 = {1};
        vector<int>v1 = {0,2};
        vector<int>v2 = {1,3};
        vector<int>v3 = {2};
        channels[0] = v0;
        channels[1] = v1;
        channels[2] = v2;
        channels[3] = v3;

        capacities[make_tuple(0,1)] = 10;
        capacities[make_tuple(1,0)] = 10;

        capacities[make_tuple(1,2)] = 10;
        capacities[make_tuple(2,1)] = 10;

        capacities[make_tuple(2,3)] = 10;
        capacities[make_tuple(3,2)] = 10;
    }

    //iterate through the global job_list and find my jobs

    for (int i=0; i<(int)job_list.size(); i++){
        if (job_list[i].sender == getIndex()){
            my_jobs.push_back(job_list[i]);
        }
    }

    //find gate indices corresponding to node indicies and set channel capacities
    const char * gateName = "out";
      cGate *destGate = NULL;
      bool found = false;

      int i = 0;
      int gateSize = gate(gateName, 0)->size();

      do {
          destGate = gate(gateName, i++);
          cGate *nextGate = destGate->getNextGate();
          if (nextGate ) {
              node_to_gate[nextGate->getOwnerModule()->getIndex()] = destGate;
          }
      } while (!found && i < gateSize);
      // here: found equals to false means that a packet was not sent

      //get map for (getIndex, dest) = channel capacity
      for(map<int,cGate *>::iterator iter = node_to_gate.begin(); iter != node_to_gate.end(); ++iter)
      {
      int key =  iter->first;
      node_to_capacity[key] = capacities[make_tuple(getIndex(),key)];

      //ignore value
      //Value v = iter->second;
      }

      WATCH_MAP(node_to_capacity);
      //initialize node_to_queued_jobs
      for(map<int,cGate *>::iterator iter = node_to_gate.begin(); iter != node_to_gate.end(); ++iter)
      {
           int key =  iter->first;
           deque<tuple<int, double , simpleMsg *>> temp;
           node_to_queued_jobs[key] = temp;
           //ignore value
           //Value v = iter->second;
     }

    arrivalSignal = registerSignal("arrival");

    //send my first message  - not yet implementing timeSent parameter, send all msgs at beginning
   for (int i=0 ; i<(int)my_jobs.size(); i++){
       Job j = my_jobs[i];
       double timeSent = j.timeSent;
       simpleMsg *msg = generateMessage(j);
       scheduleAt(timeSent, msg);
   }

   }

simpleMsg *simpleNode::generateMessage(Job job)
{

    char msgname[20];
    sprintf(msgname, "tic-%d-to-%d", job.sender, job.receiver);

    // Create message object and set source and destination field.
    simpleMsg *msg = new simpleMsg(msgname);
    msg->setAmount(job.amount);
    msg->setTimeSent(job.timeSent);
    msg->setSender(job.sender);
    msg->setReceiver(job.receiver); // <--- this should send to a router gate
    msg->setRoute(get_route(job.sender,job.receiver)); // need to calculate route
    msg->setPriorityClass(job.priorityClass);
    msg->setHopCount(0);
    return msg;
}


bool sortFunction(const tuple<int,double, simpleMsg*> &a,
              const tuple<int,double, simpleMsg*> &b)
{
    if (get<0>(a) < get<0>(b)){
        return true;
    }
    else if (get<0>(a) == get<0>(b)){
        return (get<1>(a) < get<1>(b));
    }
    return false;
}

void simpleNode::handleMessage(cMessage *msg)
{
    simpleMsg *ttmsg = check_and_cast<simpleMsg *>(msg);

        // Message arrived
        int hopcount = ttmsg->getHopCount();
        // send a signal
        if(getIndex() == ttmsg->getReceiver()){
        emit(arrivalSignal, hopcount);



        if (hasGUI()) {
            char label[50];
            // Write last hop count to string
            sprintf(label, "last hopCount = %d", hopcount);
            // Get pointer to figure
            cCanvas *canvas = getParentModule()->getCanvas();
            cTextFigure *textFigure = check_and_cast<cTextFigure*>(canvas->getFigure("lasthopcount"));
            // Update figure text
            textFigure->setText(label);
        }

        EV << "Message " << ttmsg << " arrived after " << hopcount << " hops.\n";
        bubble("Message arrived!");

        delete ttmsg;


        }
        else{


            deque<tuple<int, double , simpleMsg *>> q;

            // Message arrived
            if (!(msg -> isSelfMessage())){
            //re-adjust channel capacities
            int hopcount = ttmsg->getHopCount();
            int sender = ttmsg->getRoute()[hopcount-1];
            node_to_capacity[sender] = node_to_capacity[sender] + ttmsg->getAmount();
            //add message to queue for next stop




            //see if any new messages can be sent out based on priority for the channel that just came in
          q = node_to_queued_jobs[sender];
              //send jobs until channel funds are too low;
             processJobs(sender, q);
            }

            bubble(string_node_to_capacity().c_str());

            int nextStop = ttmsg->getRoute()[hopcount+1];

             q = node_to_queued_jobs[nextStop];

            q.push_back(make_tuple(ttmsg->getPriorityClass(), ttmsg->getAmount(),
                    ttmsg));
            // resort queued jobs for next stop based on lowest priority, then lowest amount
            sort(q.begin(), q.end(), sortFunction);

            // see if newly received job can be sent out
            processJobs(nextStop, q);
            bubble(string_node_to_capacity().c_str());

        }

}

void simpleNode:: processJobs(int dest, deque<tuple<int, double , simpleMsg *>>& q){
    while((int)q.size()>0 && get<1>(q[0])<=node_to_capacity[dest]){
          forwardMessage(get<2>(q[0]));
          q.pop_front();
    } //end while

}

void simpleNode::forwardMessage(simpleMsg *msg)
{
    // Increment hop count.
    msg->setHopCount(msg->getHopCount()+1);
    //use hopCount to find next destination
    int nextDest = msg->getRoute()[msg->getHopCount()];

    EV << "Forwarding message " << msg << " on gate[" << nextDest << "]\n";
    int amt = msg->getAmount();
    node_to_capacity[nextDest] = node_to_capacity[nextDest] - amt;
    send(msg, node_to_gate[nextDest]);

}
