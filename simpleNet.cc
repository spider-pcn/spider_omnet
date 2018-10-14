#include <stdio.h>
#include <string.h>
#include <omnetpp.h>
#include <vector>
#include "simpleMsg_m.h"
#include <deque>

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

   printf("sender: %i; receiver: %i \n [", sender, receiver);
   vector<int> route =  breathFirstSearch(sender, receiver);

   for (int i=0; i<(int)route.size(); i++){
        printf("%i, ", route[i]);
    }
    printf("] \n");

   // vector<int> empty;
    //return empty;
    return route;
    //passed by value, not by reference
}


class simpleNode : public cSimpleModule
{
private:
    simsignal_t arrivalSignal;
    vector< Job > my_jobs;
    vector< Job > queued_jobs;
protected:
    virtual simpleMsg *generateMessage(Job job);
    //virtual void forwardMessage(simpleMsg *msg);
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void forwardMessage(simpleMsg *msg);
};
Define_Module(simpleNode);


void simpleNode::initialize()
{

    std::cout << "in initialize \n";
    EV << "\n EV in initialize \n";
    if (getIndex() == 0){    //main initialization for global parameters

        // create job queue
        Job j1 = Job(3,2,0,3,0);
        Job j2 = Job(5,0.5,2,0,0);
        job_list.push_back(j1);
        job_list.push_back(j2);

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
    }

    //iterate through the global job_list and find my jobs

    for (int i=0; i<(int)job_list.size(); i++){
        if (job_list[i].sender == getIndex()){
            my_jobs.push_back(job_list[i]);
        }
    }

    arrivalSignal = registerSignal("arrival");

    //send my first message  - not yet implementing timeSent parameter, send all msgs at beginning
   if ((int)my_jobs.size()>0){

    Job firstJob = my_jobs[0];
    my_jobs.erase(my_jobs.begin()+0);
    simpleMsg *msg = generateMessage(firstJob);
    //scheduleAt(1.0, msg); //this is just sending a self message
    forwardMessage(msg);//send message
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
        bubble("ARRIVED, starting new one!");

        delete ttmsg;
        }
        else{
            forwardMessage(ttmsg);
        }

}





void simpleNode::forwardMessage(simpleMsg *msg)
{
    // Increment hop count.
    msg->setHopCount(msg->getHopCount()+1);
    //use hopCount to find next destination
    int nextDest = msg->getRoute()[msg->getHopCount()];

    /*
    // Same routing as before: random gate.
    int n = gateSize("out");
    int k = intuniform(0, n-1);
    */
    //EV << "n value: " << n << "; k value: " << k << "\n";


    //NEED TO CLARIFY HOW TO SEND MESSAGE
    EV << "Forwarding message " << msg << " on gate[" << nextDest << "]\n";

    const char * gateName = "out";
    int destIndex = nextDest; // index of destination lcn
    cGate *destGate = NULL;
    bool found = false;

    int i = 0;
    int gateSize = gate(gateName, 0)->size();

    do {
        destGate = gate(gateName, i++);
        cGate *nextGate = destGate->getNextGate();
        if (nextGate && nextGate->getOwnerModule()->getIndex() == destIndex) {
            found = true;
            send(msg, destGate);
        }
    } while (!found && i < gateSize);
    // here: found equals to false means that a packet was not sent

}
