
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

    //channel information for price scheme
    int nValue;// Total amount across all transactions (i.e. the sum of the amount in each transaction)
        // that have arrived to be sent along the channel
    double xLocal; //Transaction arrival rate ($x_local$)
    double totalCapacity; //Total channel capacity ($C$)
    double lambda; //Price due to load ($\lambda$)
    double muLocal; //Price due to channel imbalance at my end  ($\mu_{local}$)
    double muRemote; //Price due to imbalance at the other end of the channel ($\mu_{remote}$)

    //statistics for price scheme per payment channel
    simsignal_t nValueSignal;
    simsignal_t xLocalSignal;
    simsignal_t lambdaSignal;
    simsignal_t muLocalSignal;
    simsignal_t muRemoteSignal;

    //statistics - ones for per payment channel
    int statNumProcessed;
    int statNumSent;
    simsignal_t numInQueuePerChannelSignal;
    simsignal_t numProcessedPerChannelSignal;
    simsignal_t balancePerChannelSignal;
    simsignal_t numSentPerChannelSignal;

};
