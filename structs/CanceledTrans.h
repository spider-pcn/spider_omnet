
using namespace std;
using namespace omnetpp;

class CanceledTrans{
public:
    int transactionId;
    simtime_t msgArrivalTime;
    int prevNode;
    int nextNode;


};
