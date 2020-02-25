#ifndef ROUTERNODE_PS_H
#define ROUTERNODE_PS_H

#include "priceQueryMsg_m.h"
#include "priceUpdateMsg_m.h"
#include "hostNodeBase.h"

using namespace std;
using namespace omnetpp;

class hostNodePriceScheme : public hostNodeBase {

    private:
        // price scheme specific signals
        vector<simsignal_t> probabilityPerDestSignals = {};
        map<int, simsignal_t> numWaitingPerDestSignals = {};
        vector<simsignal_t> numTimedOutAtSenderSignals = {};
        vector<simsignal_t> pathPerTransPerDestSignals = {};
        map<int, simsignal_t> demandEstimatePerDestSignals = {};


    protected:
        // message generators
        virtual routerMsg *generateTriggerPriceUpdateMessage();
        virtual routerMsg *generatePriceUpdateMessage(double nLocal, double serviceRate, double arrivalRate, 
                double queueSize, int reciever);
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
        virtual void handleMessage(cMessage *msg) override;
        virtual void handleTimeOutMessage(routerMsg *msg) override;
        virtual void handleTransactionMessageSpecialized(routerMsg *msg) override;
        virtual void handleStatMessage(routerMsg *msg) override;
        virtual void handleAckMessageSpecialized(routerMsg* ttmsg) override;
        virtual void handleClearStateMessage(routerMsg *msg) override;

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
        virtual void initialize() override;
};
#endif
