using namespace std;
using namespace omnetpp;


class DestNodeStruct{
public:
    vector<tuple<int, double, routerMsg*,  Id, simtime_t >> queuedTransUnits; //make_heap in initialization
    double queueSizeSum = 0;
    double totalAmtInQueue = 0; // total amount in the queue
    double totalNumTimedOut = 0;
    simsignal_t destQueueSignal;
    simsignal_t queueTimedOutSignal;
};
