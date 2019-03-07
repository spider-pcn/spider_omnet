#ifndef ROUTERNODE_LNDB_H
#define ROUTERNODE_LNDB_H

#include "probeMsg_m.h"
#include "hostNodeBase.h"

using namespace std;
using namespace omnetpp;

class hostNodeLNDBaseline : public hostNodeBase {

    private:

        vector<simsignal_t> pathPerTransPerDestSignals = {};  
        map<int, vector<int>> _myChannels;

        //heap for oldest channel we pruned first
        vector<tuple<simtime_t, tuple<int, int>>> _prunedChannelsHeap;
        //note:  tuple of form (time, (sourceNode, destNode))

    protected:
        virtual void initialize() override;
        virtual void handleTransactionMessageSpecialized(routerMsg *msg) override;
        virtual void handleAckMessageSpecialized(routerMsg *msg) override;
        virtual routerMsg *generateAckMessage(routerMsg *msg, bool isSuccess = true) override;

        //HELPER FUNCTIONS
        virtual void initializeMyChannels();
        virtual void pruneEdge(int sourceNode, int destNode);
        virtual vector<int> generateNextPath(int destNode);


};
#endif
