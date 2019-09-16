//
// Generated file, do not edit! Created by nedtool 5.4 from addFundsMsg.msg.
//

#if defined(__clang__)
#  pragma clang diagnostic ignored "-Wreserved-id-macro"
#endif
#ifndef __ADDFUNDSMSG_M_H
#define __ADDFUNDSMSG_M_H

#include <omnetpp.h>

// nedtool version check
#define MSGC_VERSION 0x0504
#if (MSGC_VERSION!=OMNETPP_VERSION)
#    error Version mismatch! Probably this file was generated by an earlier version of nedtool: 'make clean' should help.
#endif



// cplusplus {{
#include <map>

typedef std::map<int,double> FundsVector;
// }}

/**
 * Class generated from <tt>addFundsMsg.msg:24</tt> by nedtool.
 * <pre>
 * packet addFundsMsg
 * {
 *     FundsVector pcsNeedingFunds;
 * }
 * </pre>
 */
class addFundsMsg : public ::omnetpp::cPacket
{
  protected:
    FundsVector pcsNeedingFunds;

  private:
    void copy(const addFundsMsg& other);

  protected:
    // protected and unimplemented operator==(), to prevent accidental usage
    bool operator==(const addFundsMsg&);

  public:
    addFundsMsg(const char *name=nullptr, short kind=0);
    addFundsMsg(const addFundsMsg& other);
    virtual ~addFundsMsg();
    addFundsMsg& operator=(const addFundsMsg& other);
    virtual addFundsMsg *dup() const override {return new addFundsMsg(*this);}
    virtual void parsimPack(omnetpp::cCommBuffer *b) const override;
    virtual void parsimUnpack(omnetpp::cCommBuffer *b) override;

    // field getter/setter methods
    virtual FundsVector& getPcsNeedingFunds();
    virtual const FundsVector& getPcsNeedingFunds() const {return const_cast<addFundsMsg*>(this)->getPcsNeedingFunds();}
    virtual void setPcsNeedingFunds(const FundsVector& pcsNeedingFunds);
};

inline void doParsimPacking(omnetpp::cCommBuffer *b, const addFundsMsg& obj) {obj.parsimPack(b);}
inline void doParsimUnpacking(omnetpp::cCommBuffer *b, addFundsMsg& obj) {obj.parsimUnpack(b);}


#endif // ifndef __ADDFUNDSMSG_M_H
