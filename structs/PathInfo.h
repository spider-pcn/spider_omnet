
using namespace std;
using namespace omnetpp;
#include "routerMsg_m.h"

class PathInfo{
public:
    vector<int> path = {};
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

};
