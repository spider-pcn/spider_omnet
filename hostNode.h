#ifndef HOST_NODE_H
#define HOST_NODE_H

#include "hostNodeBase.h"
#include "hostNodeWaterfilling.h"
#include "hostNodePriceScheme.h"
#include "hostNodeLandmarkRouting.h"
#include "hostNodeLNDBaseline.h"

using namespace std;
using namespace omnetpp;

class hostNode : public cSimpleModule {
    private:
        // controls which hostNodeType is currently initialized
        hostNodeBase* hostNodeObj = NULL;  
    protected:
        //if host node, return getIndex(), if routerNode, return getIndex()+numHostNodes
        virtual int myIndex(); 
        virtual void handleMessage(cMessage *msg) override;
        virtual void initialize() override;
        virtual void finish() override;
};
#endif
