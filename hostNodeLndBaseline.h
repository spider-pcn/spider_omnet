#ifndef ROUTERNODE_LNDB_H
#define ROUTERNODE_LNDB_H

#include "hostNodeBase.h"

using namespace std;
using namespace omnetpp;

class hostNodeLndBaseline : public hostNodeBase {

    private:

        vector<simsignal_t> numPathsPerTransPerDestSignals = {};  
        map<int, vector<int>> _activeChannels;

        //heap for oldest channel we pruned first
        list <tuple<simtime_t, tuple<int, int>>> _prunedChannelsList;
        //note:  tuple of form (time, (sourceNode, destNode))
        //list order oldest pruned channels in the front, most newly pruned in the back
    protected:
        virtual void initialize() override;
        virtual void handleTransactionMessageSpecialized(routerMsg *msg) override;
        virtual void handleAckMessageSpecialized(routerMsg *msg) override;
        virtual void handleAckMessageNoMoreRoutes(routerMsg *msg);
        virtual routerMsg *generateAckMessage(routerMsg *msg, bool isSuccess = true) override;

        //HELPER FUNCTIONS
        virtual void initializeMyChannels();
        virtual void pruneEdge(int sourceNode, int destNode);
        virtual vector<int> generateNextPath(int destNode);


};
#endif
