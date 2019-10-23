#include "routerNodeCeler.h"

Define_Module(routerNodeCeler);

/* overall controller for handling messages that dispatches the right function
 * based on message type in waterfilling
 */
/*
void routerNodeWaterCeler::handleMessage(cMessage *msg) {
    routerMsg *ttmsg = check_and_cast<routerMsg *>(msg);

    //Radhika TODO: figure out what's happening here
    if (simTime() > _simulationLength){
        auto encapMsg = (ttmsg->getEncapsulatedPacket());
        ttmsg->decapsulate();
        delete ttmsg;
        delete encapMsg;
        return;
    } 

    switch(ttmsg->getMessageType()) {
        case PROBE_MSG:
             if (_loggingEnabled) cout<< "[ROUTER "<< myIndex() 
                 <<": RECEIVED PROBE MSG] "<< ttmsg->getName() << endl;
             handleProbeMessage(ttmsg);
             if (_loggingEnabled) cout<< "[AFTER HANDLING:]  "<< endl;
             break;
        default:
             routerNodeBase::handleMessage(msg);
    }
}
*/

/* method to receive and forward probe messages to the next node */
/*
void routerNodeWaterfilling::handleProbeMessage(routerMsg* ttmsg){
    probeMsg *pMsg = check_and_cast<probeMsg *>(ttmsg->getEncapsulatedPacket());
    bool isReversed = pMsg->getIsReversed();
    int nextDest = ttmsg->getRoute()[ttmsg->getHopCount()+1];

    forwardProbeMessage(ttmsg);
}
*/

/* forwards probe messages 
 * after appending this channel's balances to the probe message on the
 * way back
 */
/*
void routerNodeWaterfilling::forwardProbeMessage(routerMsg *msg){
    int prevDest = msg->getRoute()[msg->getHopCount() - 1];
    bool updateOnReverse = true;
    
    // Increment hop count.
    msg->setHopCount(msg->getHopCount()+1);
    //use hopCount to find next destination
    int nextDest = msg->getRoute()[msg->getHopCount()];

    probeMsg *pMsg = check_and_cast<probeMsg *>(msg->getEncapsulatedPacket());

    if (pMsg->getIsReversed() == true && updateOnReverse == true){
        vector<double> *pathBalances = & ( pMsg->getPathBalances());
        (*pathBalances).push_back(nodeToPaymentChannel[prevDest].balance);
    } 
    else if (pMsg->getIsReversed() == false && updateOnReverse == false) {
        vector<double> *pathBalances = & ( pMsg->getPathBalances());
        (*pathBalances).push_back(nodeToPaymentChannel[nextDest].balance);
    }
    send(msg, nodeToPaymentChannel[nextDest].gate);
}
*/

