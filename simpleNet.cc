#include <stdio.h>
#include <string.h>
#include <omnetpp.h>
#include "simpleMsg_m.h"

using namespace omnetpp;

using namespace std;


class endpointNode : public cSimpleModule
{
private:
    simsignal_t arrivalSignal;
    vector< tuple<int, int, int> > jobs;

protected:
    virtual simpleMsg *generateMessage(tuple<int,int,int> job);
    //virtual void forwardMessage(simpleMsg *msg);
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
};



class routerNode : public cSimpleModule
{
private:
    vector< tuple<int, int, int> > jobs;
protected:
    virtual void forwardMessage(simpleMsg *msg);
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
};



Define_Module(routerNode);
Define_Module(endpointNode);

void endpointNode::initialize()
{
    if (getIndex() == 0){

        jobs = {std::make_tuple(0,1,12), std::make_tuple(0,1,5)}; //(source, destination, size)

    }
    else{

        jobs = {std::make_tuple(1,0,2), std::make_tuple(1,0,15)};
    }
    arrivalSignal = registerSignal("arrival");
    // Module 0 sends the first message
    //if (getIndex() == 0) {
        // Boot the process scheduling the initial message as a self-message.
    tuple<int,int,int> job = jobs[0];
    jobs.erase(jobs.begin()+0);
    simpleMsg *msg = generateMessage(job);
        scheduleAt(0.0, msg);
    //}
}


void routerNode::initialize(){

}




simpleMsg *endpointNode::generateMessage(tuple<int,int,int> job)
{
    // Produce source and destination addresses.
    int src = getIndex();
    //int n = getVectorSize();
    //EV <<  "getVectorSize(): " << n;


   // int dest = intuniform(0, n-2);
   // if (dest >= src)
     //   dest++;
    int dest = get<1>(job);
    int size = get<2>(job);
    char msgname[20];
    sprintf(msgname, "tic-%d-to-%d", src, dest);

    // Create message object and set source and destination field.
    simpleMsg *msg = new simpleMsg(msgname);
    msg->setSource(src);
    msg->setDestination(dest); // <--- this should send to a router gate
    msg->setSize(size);
    return msg;
}




void endpointNode::handleMessage(cMessage *msg)
{
    simpleMsg *ttmsg = check_and_cast<simpleMsg *>(msg);

        // Message arrived
        int hopcount = ttmsg->getHopCount();
        // send a signal
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



void routerNode::handleMessage(cMessage *msg)
{
    simpleMsg *ttmsg = check_and_cast<simpleMsg *>(msg);

    if (ttmsg->getDestination() == getIndex()) {
        // Message arrived
        int hopcount = ttmsg->getHopCount();
        // send a signal
    //    emit(arrivalSignal, hopcount);

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

        // Generate another one.

    }
    else {
        // We need to forward the message.
        forwardMessage(ttmsg);
    }
}



void routerNode::forwardMessage(simpleMsg *msg)
{
    // Increment hop count.
    msg->setHopCount(msg->getHopCount()+1);

    // Same routing as before: random gate.
    int n = gateSize("gate");
    int k = intuniform(0, n-1);

    EV << "Forwarding message " << msg << " on gate[" << k << "]\n";
    send(msg, "gate$o", k);
}
