#include "hostNode.h"
#include <queue>
#include "hostInitialize.h"

//global parameters
map<int, priority_queue<TransUnit, vector<TransUnit>, LaterTransUnit>> _transUnitList;
int _numNodes;
int _numRouterNodes;
int _numHostNodes;
double _maxTravelTime;
map<int, vector<pair<int,int>>> _channels; //adjacency list format of graph edges of network
map<tuple<int,int>,double> _balances;
//map of balances for each edge; key = <int,int> is <source, destination>

double _statRate;
double _clearRate;
int _kValue;
double _simulationLength;

bool _waterfillingEnabled;
bool _smoothWaterfillingEnabled;
bool _timeoutEnabled;
bool _loggingEnabled;
bool _signalsEnabled;
bool _priceSchemeEnabled;
bool _landmarkRoutingEnabled;
vector<tuple<int,int>> _landmarksWithConnectivityList = {};

double _epsilon; // for all precision errors

//global parameters for fixed size queues
bool _hasQueueCapacity;
int _queueCapacity;


#define MSGSIZE 100
#define SMALLEST_INDIVISIBLE_UNIT 1

Define_Module(hostNode);

int hostNode::myIndex(){
   return getIndex();
}


/* initialize the global variables and the
 * right base class for each of the nodes
 */
void hostNode::initialize() {
    string topologyFile_ = par("topologyFile");
    string workloadFile_ = par("workloadFile");


    // initialize global parameters once
    if (getIndex() == 0){  
        _simulationLength = par("simulationLength");
        _statRate = par("statRate");
        _clearRate = par("timeoutClearRate");
        _waterfillingEnabled = par("waterfillingEnabled");
        _smoothWaterfillingEnabled = par("smoothWaterfillingEnabled");
        _timeoutEnabled = par("timeoutEnabled");
        _signalsEnabled = par("signalsEnabled");
        _loggingEnabled = par("loggingEnabled");
        _priceSchemeEnabled = par("priceSchemeEnabled");

        _hasQueueCapacity = false;
        _queueCapacity = 0;
        _reschedulingEnabled = true;

        _landmarkRoutingEnabled = false;
        if (_landmarkRoutingEnabled){
            _hasQueueCapacity = true;
            _queueCapacity = 0;
            _timeoutEnabled = false;
        }

        // price scheme parameters         
        _nesterov = false;
        _secondOrderOptimization = true;

        _eta = par("eta");
        _kappa = par("kappa");
        _tUpdate = par("updateQueryTime");
        _tQuery = par("updateQueryTime");
        _alpha = par("alpha");
        _gamma = 1; // ewma factor to compute per path rates
        _zeta = par("zeta"); // ewma for d_ij every source dest demand
        _minPriceRate = par("minRate");
        _rho = _rhoLambda = _rhoMu = par("rhoValue");

        _epsilon = pow(10, -6);
        cout << "epsilon" << _epsilon << endl;
        
        // smooth waterfilling parameters
        _Tau = par("tau");
        _Normalizer = par("normalizer"); // TODO: C from discussion with Mohammad)
        _ewmaFactor = 1; // EWMA factor for balance information on probes

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
        hostNodeObj.reset(new hostNodePriceScheme());
    else if (_waterfillingEnabled)
        hostNodeObj.reset(new hostNodeWaterfilling());
    else if (_landmarkRoutingEnabled)
        hostNodeObj.reset(new hostNodeLandmarkRouting());
    else
        hostNodeObj.reset(new hostNodeBase());
    hostNodeObj->setIndex(myIndex());

    // call the object to initialize all of the other parameters for this node
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
    
    // compute number of transactions that were split
    int splitTrans = 0;
    for (auto p: transactionIdToNumHtlc) {
        if (p.second>1) splitTrans++;
    }
    if (_loggingEnabled) {
        cout << endl;
        cout << "number greater than 1: " << splitTrans << endl;
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
             if (_loggingEnabled) 
                 cout<< "[HOST "<< myIndex() <<": RECEIVED TRANSACTION MSG]  "
                     << msg->getName() <<endl;
             
             transactionMsg *transMsg = 
                 check_and_cast<transactionMsg *>(ttmsg->getEncapsulatedPacket());
             if (transMsg->isSelfMessage() && simTime() == transMsg->getTimeSent()) {
                 generateNextTransaction();
             }
             
             if (_timeoutEnabled && hostNodeObj->handleTransactionMessageTimeOut(ttmsg)){
                 return;
             }
             hostNodeObj->handleTransactionMessage(ttmsg);
             if (_loggingEnabled) cout << "[AFTER HANDLING:]" << endl;
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
        
        case PROBE_MSG:
             if (_loggingEnabled) cout<< "[HOST "<< myIndex() 
                 <<": RECEIVED PROBE MSG] "<< msg->getName() << endl;
             hostNodeObj->handleProbeMessage(ttmsg);
             if (_loggingEnabled) cout<< "[AFTER HANDLING:]  "<< endl;
             break;
        
        case CLEAR_STATE_MSG:
             if (_loggingEnabled) cout<< "[HOST "<< myIndex() 
                 <<": RECEIVED CLEAR_STATE_MSG] "<< msg->getName() << endl;
             hostNodeObj->handleClearStateMessage(ttmsg);
             if (_loggingEnabled) cout<< "[AFTER HANDLING:]  "<< endl;
             break;
        
        case TRIGGER_PRICE_UPDATE_MSG:
             if (_loggingEnabled) cout<< "[HOST "<< myIndex() 
                 <<": RECEIVED TRIGGER_PRICE_UPDATE MSG] "<< msg->getName() << endl;
             hostNodeObj->handleTriggerPriceUpdateMessage(ttmsg);
             if (_loggingEnabled) cout<< "[AFTER HANDLING:]  "<< endl;
             break;

        case PRICE_UPDATE_MSG:
             if (_loggingEnabled) cout<< "[HOST "<< myIndex() 
                 <<": RECEIVED PRICE_UPDATE MSG] "<< msg->getName() << endl;
             hostNodeObj->handlePriceUpdateMessage(ttmsg);
             if (_loggingEnabled) cout<< "[AFTER HANDLING:]  "<< endl;
             break;

        case TRIGGER_PRICE_QUERY_MSG:
             if (_loggingEnabled) cout<< "[HOST "<< myIndex() 
                 <<": RECEIVED TRIGGER_PRICE_QUERY MSG] "<< msg->getName() << endl;
             hostNodeObj->handleTriggerPriceQueryMessage(ttmsg);
             if (_loggingEnabled) cout<< "[AFTER HANDLING:]  "<< endl;
             break;
        
        case PRICE_QUERY_MSG:
             if (_loggingEnabled) cout<< "[HOST "<< myIndex() 
                 <<": RECEIVED PRICE_QUERY MSG] "<< msg->getName() << endl;
             hostNodeObj->handlePriceQueryMessage(ttmsg);
             if (_loggingEnabled) cout<< "[AFTER HANDLING:]  "<< endl;
             break;

        case TRIGGER_TRANSACTION_SEND_MSG:
             if (_loggingEnabled) cout<< "[HOST "<< myIndex() 
                 <<": RECEIVED TRIGGER_TXN_SEND MSG] "<< msg->getName() << endl;
             hostNodeObj->handleTriggerTransactionSendMessage(ttmsg);
             if (_loggingEnabled) cout<< "[AFTER HANDLING:]  "<< endl;
             break;
    }
}

/* calls actual object to finish */
void hostNode::finish() {
    hostNodeObj->finish();
}
