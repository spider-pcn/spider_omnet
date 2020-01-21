
using namespace std;
using namespace omnetpp;

class SplitState{
public:
    int numTotal;
    int numReceived;
    int numArrived;
    int numAttempted;
    double firstAttemptTime = -1;
    double totalAmount;
    double numTries;
};
