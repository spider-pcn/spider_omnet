
using namespace std;
using namespace omnetpp;

class PathInfo{
public:
    vector<int> path;
    simtime_t lastUpdated = -1;
    double bottleneck = -1;
    vector<double> pathBalances;
};
