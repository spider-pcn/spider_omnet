
using namespace std;
using namespace omnetpp;

class AckState{
public:
    double amtSent;
    double amtReceived;
    bool attemptMade;
    simtime_t attemptTime;
};
