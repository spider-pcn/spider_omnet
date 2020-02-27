#ifndef ROUTERNODE_PRS_H
#define ROUTERNODE_PRS_H

#include "priceQueryMsg_m.h"
#include "priceUpdateMsg_m.h"
#include "hostNodeBase.h"

using namespace std;
using namespace omnetpp;

class hostNodePropFairPriceScheme : public hostNodeBase {

    private:
        // price scheme specific signals
        map<int, simsignal_t> numWaitingPerDestSignals = {};
        vector<simsignal_t> numTimedOutAtSenderSignals = {};
        map<int, simsignal_t> demandEstimatePerDestSignals = {};

    protected:
        // message generators
        virtual routerMsg *generateTriggerTransactionSendMessage(vector<int> route, 
                int routeIndex, int destNode);
        virtual routerMsg *generateComputeDemandMessage();
        virtual routerMsg *generateTriggerRateDecreaseMessage();

        // helpers
        // functions to compute projections while ensure rates are feasible
        virtual bool ratesFeasible(vector<PathRateTuple> actualRates, double demand);
        virtual vector<PathRateTuple> computeProjection(vector<PathRateTuple> recommendedRates, 
                double demand);

        // modified message handlers
        virtual void handleMessage(cMessage *msg) override;
        virtual void handleTimeOutMessage(routerMsg *msg) override;
        virtual void handleTransactionMessageSpecialized(routerMsg *msg) override;
        virtual void handleComputeDemandMessage(routerMsg *msg);
        virtual void handleTriggerRateDecreaseMessage(routerMsg *msg);
        virtual void handleStatMessage(routerMsg *msg) override;
        virtual void handleAckMessageSpecialized(routerMsg* ttmsg) override;
        virtual void handleClearStateMessage(routerMsg *msg) override;

        // special messages for priceScheme
        virtual void handleTriggerTransactionSendMessage(routerMsg* ttmsg);

        /**** CORE LOGIC ******/
        // initialize price probes for a given set of paths to a destination and store the paths
        // for that destination
        virtual void initializePathInfo(vector<vector<int>> kShortestPaths, int destNode);

        // updates timers once rates have been updated on a certain path
        virtual void updateTimers(int destNode, int pathIndex, double newRate);
        virtual void initialize() override;
};
#endif
