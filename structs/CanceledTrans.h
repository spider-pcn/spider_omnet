
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
// TODO: add apath id or something else to uniquely identify the path also here
typedef tuple<int, simtime_t, int, int, int> CanceledTrans ;
    // (transactionId, msgArrivalTime, prevNode, nextNode, destNode)

