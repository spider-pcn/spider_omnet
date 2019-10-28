using namespace std;
using namespace omnetpp;
#include "routerMsg_m.h"
#include "structs/SplitState.h"

// declared here so as to use it for transCompare
extern unordered_map<int, unordered_map<double, SplitState>> _numSplits;

// scheduling algorithm knob
extern bool _LIFOEnabled;
extern bool _FIFOEnabled;
extern bool _SPFEnabled;
extern bool _RREnabled;
extern bool _EDFEnabled;
extern bool (*_schedulingAlgorithm) (const tuple<int,double, routerMsg*, Id, simtime_t> &a,
      const tuple<int,double, routerMsg*, Id, simtime_t> &b);

// sender queue sorter functions
struct transCompare {
    bool operator() (const routerMsg* lhs, const routerMsg* rhs) const {
        transactionMsg *transLHS = check_and_cast<transactionMsg *>(lhs->getEncapsulatedPacket());
        transactionMsg *transRHS = check_and_cast<transactionMsg *>(rhs->getEncapsulatedPacket());

        if (_LIFOEnabled) {
            return transLHS->getTimeSent() > transRHS->getTimeSent();
        }
        else if (_FIFOEnabled || _EDFEnabled){
            return transLHS->getTimeSent() < transRHS->getTimeSent();
        }
        else if (_SPFEnabled){
            SplitState splitInfoA = _numSplits[transLHS->getSender()][transLHS->getLargerTxnId()];
            SplitState splitInfoB = _numSplits[transRHS->getSender()][transRHS->getLargerTxnId()];

            if (splitInfoA.totalAmount != splitInfoB.totalAmount)
                return splitInfoA.totalAmount < splitInfoB.totalAmount;
            else
                return transLHS->getTimeSent() > transRHS->getTimeSent();
        }
        return true;
    }
};


class DestInfo{
public:
    multiset<routerMsg *, transCompare > transWaitingToBeSent;
    

    // units are txns per second
    double demand = 0;

    // number of transactions since the last interval
    double transSinceLastInterval = 0;

    // path and balance of the path with highest current
    // bottleneck balance - amt Inflight
    int highestBottleneckPathIndex;
    double highestBottleneckBalance;

    // number of transactions waiting at the sender over the last x seconds
    // of a monitor paths message
    double sumTxnsWaiting;

    // maximum path Id in use right now
    int maxPathId;
};
