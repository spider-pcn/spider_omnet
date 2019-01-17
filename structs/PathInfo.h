
using namespace std;
using namespace omnetpp;
#include "routerMsg_m.h"

class PathInfo{
public:
    vector<int> path = {};
    double probability = -1;
    simtime_t lastUpdated = -1;
    double bottleneck = -1;
    vector<double> pathBalances = {};
    simsignal_t bottleneckPerDestPerPathSignal;
    simsignal_t probeBackPerDestPerPathSignal;
    simsignal_t rateCompletedPerDestPerPathSignal;
    simsignal_t rateAttemptedPerDestPerPathSignal;
    int statRateCompleted = 0; //rate value, is reset on handle stat message TODO: .ned file, initialize/register, emit
    int statRateAttempted = 0;

    //additional parameters for price scheme
    double rateToSendTrans = 0;
    simtime_t timeToNextSend = 0;
    double sumOfTransUnitsInFlight = 0;
    simtime_t priceLastUpdated = 0;
    routerMsg * triggerTransSendMsg;
    bool isSendTimerSet = false;
    bool isProbeOutstanding = false;

    //signals for price scheme per path
    simsignal_t rateToSendTransSignal;
    simsignal_t timeToNextSendSignal;
    simsignal_t sumOfTransUnitsInFlightSignal;
    simsignal_t priceLastUpdatedSignal;
    simsignal_t isSendTimerSetSignal;


};
