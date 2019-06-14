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

/* handler for the statistic message triggered every x seconds to also
 * output DCTCP scheme stats in addition to the default
 */
void routerNodeDCTCP::handleStatMessage(routerMsg* ttmsg) {
    if (_signalsEnabled) {
        // per router payment channel stats
        for ( auto it = nodeToPaymentChannel.begin(); it!= nodeToPaymentChannel.end(); it++){
            int node = it->first; //key
            PaymentChannel* p = &(nodeToPaymentChannel[node]);
        
            // get latest queueing delay
            auto lastTransTimes =  p->serviceArrivalTimeStamps.back();
            double curQueueingDelay = get<1>(lastTransTimes).dbl() - get<2>(lastTransTimes).dbl();
            p->queueDelayEWMA = 0.6*curQueueingDelay + 0.4*p->queueDelayEWMA;
            
            emit(p->queueDelayEWMASignal, p->queueDelayEWMA);
        }
    }

    // call the base method to output rest of the stats
    routerNodeBase::handleStatMessage(ttmsg);
}

