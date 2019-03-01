#ifndef ROUTERNODE_LNDB_H
#define ROUTERNODE_LNDB_H

#include "probeMsg_m.h"
#include "hostNodeBase.h"

using namespace std;
using namespace omnetpp;

class hostNodeLNDBaseline : public hostNodeBase {

    private:

        vector<simsignal_t> pathPerTransPerDestSignals = {};  

    protected:
        virtual void initialize() override;
        // message generating functions

        // message handlers
        
        //handleMessage - need to call special functions?
        //virtual void handleMessage(cMessage *msg) override;

        //handleTransactionMessageSpecialized - need to set path index? (always set to index 0, shortest)
        virtual void handleTransactionMessageSpecialized(routerMsg *msg) override;

        //virtual void handleTimeOutMessage(routerMsg *msg) override;
        //virtual void handleProbeMessage(routerMsg *msg);
        //virtual void handleClearStateMessage(routerMsg *msg) override;
        
        //handleAckMessageSpecialized - need to resend if timeout and have paths left, else fail it
        virtual void handleAckMessageSpecialized(routerMsg *msg) override;

        //initializePathInfoLNDBaseline - store into datastructure
        virtual void initializePathInfoLNDBaseline(vector<vector<int>> kShortestRoutes, int destNode);


};
#endif
