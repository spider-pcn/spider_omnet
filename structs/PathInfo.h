
using namespace std;
using namespace omnetpp;

class PathInfo{
public:
    vector<int> path;
    simtime_t lastUpdated = -1;
    double bottleneck = -1;
    vector<double> pathBalances;
    simsignal_t bottleneckPerDestPerPathSignal;
    simsignal_t probeBackPerDestPerPathSignal;
    simsignal_t rateCompletedPerDestPerPathSignal;
    simsignal_t rateAttemptedPerDestPerPathSignal;
    int statRateCompleted = 0; //rate value, is reset on handle stat message TODO: .ned file, initialize/register, emit
    int statRateAttempted = 0;

    //additional parameters for price scheme
    double rateToSendTrans;
    double timeToNextSend;
    double sumOfTransUnitsInFlight;
    simsignal_t priceLastUpdated;

};
