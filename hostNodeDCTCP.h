#ifndef ROUTERNODE_DCTCP_H
#define ROUTERNODE_DCTCP_H

#include "priceQueryMsg_m.h"
#include "priceUpdateMsg_m.h"
#include "hostNodeBase.h"

using namespace std;
using namespace omnetpp;

class hostNodeDCTCP : public hostNodeBase {

    private:
        // DCTCP signals
        //

    protected:
        virtual void initialize() override;
        virtual void handleAckMessageSpecialized(routerMsg *msg) override;
        virtual void handleTimeOutMessage(routerMsg *msg) override;
        virtual void handleTransactionMessageSpecialized(routerMsg *msg) override;
        virtual void handleStatMessage(routerMsg *msg) override;
        virtual void handleClearStateMessage(routerMsg *msg) override;

        // helper method
        virtual void sendMoreTransactionsOnPath(int destNode, int pathIndex);
};
#endif
