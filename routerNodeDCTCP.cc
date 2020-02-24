#include "routerNodeDCTCP.h"

Define_Module(routerNodeDCTCP);

/* handles forwarding of  transactions out of the queue
 * the way other schemes' routers do except that it marks the packet
 * if the queue is larger than the threshold, therfore mostly similar to the base code */ 
int routerNodeDCTCP::forwardTransactionMessage(routerMsg *msg, int dest, simtime_t arrivalTime) {
    transactionMsg *transMsg = check_and_cast<transactionMsg *>(msg->getEncapsulatedPacket());
    int nextDest = msg->getRoute()[msg->getHopCount()+1];
    PaymentChannel *neighbor = &(nodeToPaymentChannel[nextDest]);
    
    //don't mark yet if the packet can't be sent out
    if (neighbor->balance <= 0 || transMsg->getAmount() > neighbor->balance){
        return 0;
    }
 
    // else mark before forwarding if need be
    vector<tuple<int, double , routerMsg *, Id, simtime_t>> *q;
    q = &(neighbor->queuedTransUnits);
    if (_qDelayVersion) {
        simtime_t curQueueingDelay = simTime()  - arrivalTime;
        if (curQueueingDelay.dbl() > _qDelayEcnThreshold) {
            transMsg->setIsMarked(true); 
        }
    } 
    else {
        if (getTotalAmount(nextDest) > _qEcnThreshold) {
            transMsg->setIsMarked(true); 
        }
    }
    return routerNodeBase::forwardTransactionMessage(msg, dest, arrivalTime);
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

