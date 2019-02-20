#ifndef ROUTERNODE_H
#define ROUTERNODE_H

#include <stdio.h>
#include <string.h>
#include <omnetpp.h>
#include <vector>
#include "routerMsg_m.h"
#include "transactionMsg_m.h"
#include "ackMsg_m.h"
#include "updateMsg_m.h"
#include "timeOutMsg_m.h"
#include <iostream>
#include <algorithm>
#include <string>
#include <sstream>
#include <deque>
#include <map>
#include <fstream>
#include "global.h"

using namespace std;
using namespace omnetpp;

class hostNode:: public cSimpleModule {
    private:
        // controls which hostNodeType is currently initialized
        std::unique_ptr<Object> hostNodeObj(NULL);  
    protected:
        //if host node, return getIndex(), if routerNode, return getIndex()+numHostNodes
        virtual int myIndex(); 
        virtual void handleMessage(cMessage *msg) override;
        virtual void initialize() override;
        virtual void finish() override;
}
