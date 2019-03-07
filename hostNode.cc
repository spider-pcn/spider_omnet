#include "hostNode.h"
#include <queue>
int hostNode::myIndex(){
   return getIndex();
}


/* initialize the global variables and the
 * right base class for each of the nodes
 */
void hostNode::initialize() {
    cout << "starting initialization" ;
    string topologyFile_ = par("topologyFile");
    string workloadFile_ = par("workloadFile");


    // initialize global parameters once
    if (getIndex() == 0){  
        _simulationLength = par("simulationLength");
        _statRate = par("statRate");
        _clearRate = par("timeoutClearRate");
        _waterfillingEnabled = par("waterfillingEnabled");
        _timeoutEnabled = par("timeoutEnabled");
        _signalsEnabled = par("signalsEnabled");
        _loggingEnabled = par("loggingEnabled");
        _priceSchemeEnabled = par("priceSchemeEnabled");

        _hasQueueCapacity = false;
        _queueCapacity = 0;


        _landmarkRoutingEnabled = false;
        if (_landmarkRoutingEnabled){
            _hasQueueCapacity = true;
            _queueCapacity = 0;
            _timeoutEnabled = false;
        }

        _epsilon = pow(10, -6);
        cout << "epsilon" << _epsilon << endl;
        
        if (_waterfillingEnabled || _priceSchemeEnabled || _landmarkRoutingEnabled){
           _kValue = par("numPathChoices");
        }

        _maxTravelTime = 0.0;
        _delta = 0.01; // to avoid divide by zero 
        setNumNodes(topologyFile_);
        // add all the TransUnits into global list
        generateTransUnitList(workloadFile_);

        //create "channels" map - from topology file
        generateChannelsBalancesMap(topologyFile_);
    }

    // instan
    if (_priceSchemeEnabled) 
        hostNodeObj = new hostNodePriceScheme();
    else if (_waterfillingEnabled)
        hostNodeObj = new hostNodeWaterfilling();
    else if (_landmarkRoutingEnabled)
        hostNodeObj = new hostNodeLandmarkRouting();
    else
        hostNodeObj = new hostNodeBase();
    hostNodeObj->setIndex(myIndex());

    // call the object to initialize all of the other parameters for this node
    // with the first out gate
    cout << "calling on host object";
    hostNodeObj->initialize();
}

/* overall controller for handling messages that dispatches the right function
 * based on message type
 */
void hostNode::handleMessage(cMessage *msg) {
    routerMsg *ttmsg = check_and_cast<routerMsg *>(msg);
 
    //Radhika TODO: figure out what's happening here
    if (simTime() > _simulationLength){
        auto encapMsg = (ttmsg->getEncapsulatedPacket());
        ttmsg->decapsulate();
        delete ttmsg;
        delete encapMsg;
        return;
    }

    cout << "maxTravelTime:" << _maxTravelTime << endl;

    // handle all messges by type
    switch (ttmsg->getMessageType()) {
        case ACK_MSG:
            if (_loggingEnabled) 
                cout << "[HOST "<< myIndex() <<": RECEIVED ACK MSG] " << msg->getName() << endl;
            if (_timeoutEnabled)
                hostNodeObj->handleAckMessageTimeOut(ttmsg);
            hostNodeObj->handleAckMessageSpecialized(ttmsg);
            if (_loggingEnabled) cout << "[AFTER HANDLING:]" <<endl;
            break;

        case TRANSACTION_MSG: 
            { 
                if (_loggingEnabled) 
                    cout<< "[HOST "<< myIndex() <<": RECEIVED TRANSACTION MSG]  "
                     << msg->getName() <<endl;
             
                transactionMsg *transMsg = 
                    check_and_cast<transactionMsg *>(ttmsg->getEncapsulatedPacket());
                if (transMsg->isSelfMessage() && simTime() == transMsg->getTimeSent()) {
                    hostNodeObj->generateNextTransaction();
                }
             
                if (_timeoutEnabled && hostNodeObj->handleTransactionMessageTimeOut(ttmsg)){
                    return;
                }
                hostNodeObj->handleTransactionMessageSpecialized(ttmsg);
                if (_loggingEnabled) cout << "[AFTER HANDLING:]" << endl;
            }
            break;

        case UPDATE_MSG:
            if (_loggingEnabled) cout<< "[HOST "<< myIndex() 
                <<": RECEIVED UPDATE MSG] "<< msg->getName() << endl;
            hostNodeObj->handleUpdateMessage(ttmsg);
            if (_loggingEnabled) cout<< "[AFTER HANDLING:]  "<< endl;
            break;

        case STAT_MSG:
            if (_loggingEnabled) cout<< "[HOST "<< myIndex() 
                <<": RECEIVED STAT MSG] "<< msg->getName() << endl;
            hostNodeObj->handleStatMessage(ttmsg);
            if (_loggingEnabled) cout<< "[AFTER HANDLING:]  "<< endl;
            break;

        case TIME_OUT_MSG:
            if (_loggingEnabled) cout<< "[HOST "<< myIndex() 
                <<": RECEIVED TIME_OUT_MSG] "<< msg->getName() << endl;
       
            if (!_timeoutEnabled){
                cout << "timeout message generated when it shouldn't have" << endl;
                return;
            }

            hostNodeObj->handleTimeOutMessage(ttmsg);
            if (_loggingEnabled) cout<< "[AFTER HANDLING:]  "<< endl;
            break;
        
        case CLEAR_STATE_MSG:
            if (_loggingEnabled) cout<< "[HOST "<< myIndex() 
                <<": RECEIVED CLEAR_STATE_MSG] "<< msg->getName() << endl;
            hostNodeObj->handleClearStateMessage(ttmsg);
            if (_loggingEnabled) cout<< "[AFTER HANDLING:]  "<< endl;
            break;
        
        default:
           hostNodeObj->handleMessage(ttmsg);

    }
}

/* calls actual object to finish */
void hostNode::finish() {
    hostNodeObj->finish();
    delete hostNodeObj;
}
