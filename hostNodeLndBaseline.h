#ifndef ROUTERNODE_LNDB_H
#define ROUTERNODE_LNDB_H

#include "hostNodeBase.h"

using namespace std;
using namespace omnetpp;

class hostNodeLndBaseline : public hostNodeBase {

    private:
        vector<simsignal_t> numPathsPerTransPerDestSignals = {};  
        unordered_map<int, set<int>> _activeChannels;
        
        //list order oldest pruned channels in the front, most newly pruned in the back
        list <tuple<simtime_t, tuple<int, int>>> _prunedChannelsList;
    
    protected:
        virtual void initialize() override;
        virtual void handleTransactionMessageSpecialized(routerMsg *msg) override;
        virtual void handleAckMessageSpecialized(routerMsg *msg) override;
        virtual void handleAckMessageNoMoreRoutes(routerMsg *msg, bool toDelete);
        virtual routerMsg *generateAckMessage(routerMsg *msg, bool isSuccess = true) override;
        virtual void finish() override;

        // helpers
        virtual void initializeMyChannels();
        virtual void pruneEdge(int sourceNode, int destNode);
        virtual vector<int> generateNextPath(int destNode);
        virtual void recordTailRetries(simtime_t timeSent, bool success, int retries);


};
#endif
