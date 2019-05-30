#include "routerNodeDCTCPBal.h"

Define_Module(routerNodeDCTCPBal);

/* handles transactions the way other schemes' routers do except that it marks the packet
 * if the available balance is less than the threshold (compared to capacity of the 
 * payment channel) */ 
void routerNodeDCTCPBal::handleTransactionMessage(routerMsg *ttmsg) {
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

    if (neighbor->balance < _balEcnThreshold * neighbor->origTotalCapacity)
        transMsg->setIsMarked(true); 
    
    ttmsg->encapsulate(transMsg);
    routerNodeBase::handleTransactionMessage(ttmsg);
}
