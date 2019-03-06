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
        // message generating functions
       //handleMessage - need to call special functions?
        //virtual void handleMessage(cMessage *msg) override;

        //handleTransactionMessageSpecialized - need to set path index? (always set to index 0, shortest)
        virtual void handleTransactionMessageSpecialized(routerMsg *msg) override;

        //virtual void handleTimeOutMessage(routerMsg *msg) override;
        //virtual void handleProbeMessage(routerMsg *msg);
        //virtual void handleClearStateMessage(routerMsg *msg) override;
        
        //handleAckMessageSpecialized - need to resend if timeout and have paths left, else fail it
        virtual void handleAckMessageSpecialized(routerMsg *msg) override;


        //HELPER FUNCTIONS
        //initializePathInfoLNDBaseline - store into datastructure
        virtual void initializePathInfoLNDBaseline(vector<vector<int>> kShortestRoutes, int destNode);

        virtual void initializeMyChannels();
        // message handlers
       
        //TODO: handle return ack, prune an edge
        virtual void pruneEdge(int sourceNode, int destNode);

        //TODO: generate path
            //TODO(1): readd edges according to restoration period

        virtual vector<int> generateNextPath(int destNode);
 
        //virtual bool sortPrunedChannelsFunction(tuple<simtime_t, tuple<int,int>> x, tuple<simtime_t,tuple<int, int>> y); 



};
#endif
