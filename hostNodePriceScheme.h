#ifndef ROUTERNODE_PS_H
#define ROUTERNODE_PS_H

#include "priceQueryMsg_m.h"
#include "priceUpdateMsg_m.h"

using namespace std;
using namespace omnetpp;

class hostNodePriceScheme : public hostNodeBase {

    private:
        // price scheme specific signals
        vector<simsignal_t> probabilityPerDestSignals = {};
        vector<simsignal_t> numWaitingPerDestSignals = {};
        vector<simsignal_t> numTimedOutAtSenderSignals = {};
        vector<simsignal_t> pathPerTransPerDestSignals = {};  

    protected:
        // message generators
        virtual routerMsg *generateTriggerPriceUpdateMessage();
        virtual routerMsg *generatePriceUpdateMessage(double xLocal, int dest);
        virtual routerMsg *generateTriggerPriceQueryMessage();
        virtual routerMsg *generatePriceQueryMessage(vector<int> route, int routeIndex);
        virtual routerMsg *generateTriggerTransactionSendMessage(vector<int> route, 
                int routeIndex, int destNode);

        // helpers
        // functions to compute projections while ensure rates are feasible
        virtual bool ratesFeasible(vector<PathRateTuple> actualRates, double demand);
        virtual vector<PathRateTuple> computeProjection(vector<PathRateTuple> recommendedRates, 
                double demand);

        // modified message handlers
        virtual void handleTransactionMessagePriceScheme(routerMsg *msg);
        virtual void handleStatMessagePriceScheme(routerMsg *msg);
        virtual void handleAckMessageSpecialized(routerMsg* ttmsg) override;
        
        // special messages for priceScheme
        virtual void handleTriggerPriceUpdateMessage(routerMsg *msg);
        virtual void handlePriceUpdateMessage(routerMsg* ttmsg);
        virtual void handleTriggerPriceQueryMessage(routerMsg *msg);
        virtual void handlePriceQueryMessage(routerMsg* ttmsg);
        virtual void handleTriggerTransactionSendMessage(routerMsg* ttmsg);


        /**** CORE LOGIC ******/
        // initialize price probes for a given set of paths to a destination and store the paths
        // for that destination
        virtual void initializePriceProbes(vector<vector<int>> kShortestPaths, int destNode);


        
        // updates timers once rates have been updated on a certain path
        virtual void updateTimers(int destNode, int pathIndex, double newRate);
}
#endif
