
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


struct CanceledTransComp {
    using is_transparent = std::true_type;

    bool operator()(CanceledTrans const& ct1, CanceledTrans const& ct2) const
    {
        return get<0>(ct1) < get<0>(ct2);
    };

    bool operator()(CanceledTrans const& ct1, int id) const
    {
        return get<0>(ct1) < id;
    };

    bool operator()(int id, CanceledTrans const& ct2) const
    {
        return id < get<0>(ct2);
    };
};

struct hashCanceledTrans {
    public:
    size_t operator()(const CanceledTrans &ct) const
    {
        return hash<int>()(get<0>(ct));
    }
};

struct equalCanceledTrans {
    public:
    size_t operator()(const CanceledTrans &ct1, const CanceledTrans &ct2) const
    {
        return (get<0>(ct1) == get<0>(ct2)) ;
    }
};



