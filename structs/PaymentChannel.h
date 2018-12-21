
using namespace std;
using namespace omnetpp;

class PaymentChannel{
public:
    //channel information
    cGate* gate;
    double balance;
    vector<tuple<int, double, routerMsg*>> queuedTransUnits; //make_heap in initialization
    map<int, double> incomingTransUnits; //(key,value) := (transactionId, amount)
    map<int, double> outgoingTransUnits;

    //statistics - ones for per payment channel
    int statNumProcessed;
    int statNumSent;
    simsignal_t numInQueuePerChannelSignal;
    simsignal_t numProcessedPerChannelSignal;
    simsignal_t balancePerChannelSignal;
    simsignal_t numSentPerChannelSignal;

};
