#include "routerNodeDCTCP.h"

Define_Module(routerNodeDCTCP);

/* handles transactions the way other schemes' routers do except that it marks the packet
 * if the queue is larger than the threshold */ 
void routerNodeDCTCP::handleTransactionMessage(routerMsg *ttmsg) {
    //increment nValue
    int hopcount = ttmsg->getHopCount();
    transactionMsg *transMsg = check_and_cast<transactionMsg *>(ttmsg->getEncapsulatedPacket());
    ttmsg->decapsulate();

    //not a self-message, add to incoming_trans_units
    int nextNode = ttmsg->getRoute()[hopcount + 1];
    
    // check the queue size before marking
    PaymentChannel *neighbor = &(nodeToPaymentChannel[nextNode]);
    vector<tuple<int, double , routerMsg *, Id, simtime_t>> *q;
    q = &(neighbor->queuedTransUnits);

    if (getTotalAmount(*q) > _qEcnThreshold)
        transMsg->setIsMarked(true); 
    
    ttmsg->encapsulate(transMsg);
    routerNodeBase::handleTransactionMessage(ttmsg);
}
