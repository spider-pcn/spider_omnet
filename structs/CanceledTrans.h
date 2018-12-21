
using namespace std;
using namespace omnetpp;

/*
class CanceledTrans{
public:
    int transactionId;
    simtime_t msgArrivalTime;
    int prevNode = -1;
    int nextNode = -1;


};
*/

typedef tuple<int, simtime_t, int, int > CanceledTrans ;
    // (transactionId, msgArrivalTime, prevNode, nextNode)
