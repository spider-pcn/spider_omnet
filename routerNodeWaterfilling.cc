#include "routerNodeWaterfilling.h"

Define_Module(routerNodeWaterfilling);

/* overall controller for handling messages that dispatches the right function
 * based on message type in waterfilling
 */
void routerNodeWaterfilling::handleMessage(cMessage *msg) {
    routerMsg *ttmsg = check_and_cast<routerMsg *>(msg);
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

/* method to receive and forward probe messages to the next node */
void routerNodeWaterfilling::handleProbeMessage(routerMsg* ttmsg){
    probeMsg *pMsg = check_and_cast<probeMsg *>(ttmsg->getEncapsulatedPacket());
    bool isReversed = pMsg->getIsReversed();
    int nextDest = ttmsg->getRoute()[ttmsg->getHopCount()+1];
    forwardProbeMessage(ttmsg);
}

/* forwards probe messages 
 * after appending this channel's balances to the probe message on the
 * way back
 */
void routerNodeWaterfilling::forwardProbeMessage(routerMsg *msg){
    int prevDest = msg->getRoute()[msg->getHopCount() - 1];
    bool updateOnReverse = true;
    int nextDest = msg->getRoute()[msg->getHopCount()];
    
    probeMsg *pMsg = check_and_cast<probeMsg *>(msg->getEncapsulatedPacket());
    msg->setHopCount(msg->getHopCount()+1);

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

