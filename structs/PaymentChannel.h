
using namespace std;
using namespace omnetpp;

typedef tuple<int,int> Id;

class PaymentChannel{
public:
    //channel information
    cGate* gate;
    double balance;
    vector<tuple<int, double, routerMsg*,  Id >> queuedTransUnits; //make_heap in initialization
    map<Id, double> incomingTransUnits; //(key,value) := ((transactionId, htlcIndex), amount)
    map<Id, double> outgoingTransUnits;

    //statistics - ones for per payment channel
    int statNumProcessed;
    int statNumSent;
    simsignal_t numInQueuePerChannelSignal;
    simsignal_t numProcessedPerChannelSignal;
    simsignal_t balancePerChannelSignal;
    simsignal_t numSentPerChannelSignal;

};
