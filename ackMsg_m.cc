//
// Generated file, do not edit! Created by nedtool 5.4 from ackMsg.msg.
//

// Disable warnings about unused variables, empty switch stmts, etc:
#ifdef _MSC_VER
#  pragma warning(disable:4101)
#  pragma warning(disable:4065)
#endif

#if defined(__clang__)
#  pragma clang diagnostic ignored "-Wshadow"
#  pragma clang diagnostic ignored "-Wconversion"
#  pragma clang diagnostic ignored "-Wunused-parameter"
#  pragma clang diagnostic ignored "-Wc++98-compat"
#  pragma clang diagnostic ignored "-Wunreachable-code-break"
#  pragma clang diagnostic ignored "-Wold-style-cast"
#elif defined(__GNUC__)
#  pragma GCC diagnostic ignored "-Wshadow"
#  pragma GCC diagnostic ignored "-Wconversion"
#  pragma GCC diagnostic ignored "-Wunused-parameter"
#  pragma GCC diagnostic ignored "-Wold-style-cast"
#  pragma GCC diagnostic ignored "-Wsuggest-attribute=noreturn"
#  pragma GCC diagnostic ignored "-Wfloat-conversion"
#endif

#include <iostream>
#include <sstream>
#include "ackMsg_m.h"

namespace omnetpp {

// Template pack/unpack rules. They are declared *after* a1l type-specific pack functions for multiple reasons.
// They are in the omnetpp namespace, to allow them to be found by argument-dependent lookup via the cCommBuffer argument

// Packing/unpacking an std::vector
template<typename T, typename A>
void doParsimPacking(omnetpp::cCommBuffer *buffer, const std::vector<T,A>& v)
{
    int n = v.size();
    doParsimPacking(buffer, n);
    for (int i = 0; i < n; i++)
        doParsimPacking(buffer, v[i]);
}

template<typename T, typename A>
void doParsimUnpacking(omnetpp::cCommBuffer *buffer, std::vector<T,A>& v)
{
    int n;
    doParsimUnpacking(buffer, n);
    v.resize(n);
    for (int i = 0; i < n; i++)
        doParsimUnpacking(buffer, v[i]);
}

// Packing/unpacking an std::list
template<typename T, typename A>
void doParsimPacking(omnetpp::cCommBuffer *buffer, const std::list<T,A>& l)
{
    doParsimPacking(buffer, (int)l.size());
    for (typename std::list<T,A>::const_iterator it = l.begin(); it != l.end(); ++it)
        doParsimPacking(buffer, (T&)*it);
}

template<typename T, typename A>
void doParsimUnpacking(omnetpp::cCommBuffer *buffer, std::list<T,A>& l)
{
    int n;
    doParsimUnpacking(buffer, n);
    for (int i=0; i<n; i++) {
        l.push_back(T());
        doParsimUnpacking(buffer, l.back());
    }
}

// Packing/unpacking an std::set
template<typename T, typename Tr, typename A>
void doParsimPacking(omnetpp::cCommBuffer *buffer, const std::set<T,Tr,A>& s)
{
    doParsimPacking(buffer, (int)s.size());
    for (typename std::set<T,Tr,A>::const_iterator it = s.begin(); it != s.end(); ++it)
        doParsimPacking(buffer, *it);
}

template<typename T, typename Tr, typename A>
void doParsimUnpacking(omnetpp::cCommBuffer *buffer, std::set<T,Tr,A>& s)
{
    int n;
    doParsimUnpacking(buffer, n);
    for (int i=0; i<n; i++) {
        T x;
        doParsimUnpacking(buffer, x);
        s.insert(x);
    }
}

// Packing/unpacking an std::map
template<typename K, typename V, typename Tr, typename A>
void doParsimPacking(omnetpp::cCommBuffer *buffer, const std::map<K,V,Tr,A>& m)
{
    doParsimPacking(buffer, (int)m.size());
    for (typename std::map<K,V,Tr,A>::const_iterator it = m.begin(); it != m.end(); ++it) {
        doParsimPacking(buffer, it->first);
        doParsimPacking(buffer, it->second);
    }
}

template<typename K, typename V, typename Tr, typename A>
void doParsimUnpacking(omnetpp::cCommBuffer *buffer, std::map<K,V,Tr,A>& m)
{
    int n;
    doParsimUnpacking(buffer, n);
    for (int i=0; i<n; i++) {
        K k; V v;
        doParsimUnpacking(buffer, k);
        doParsimUnpacking(buffer, v);
        m[k] = v;
    }
}

// Default pack/unpack function for arrays
template<typename T>
void doParsimArrayPacking(omnetpp::cCommBuffer *b, const T *t, int n)
{
    for (int i = 0; i < n; i++)
        doParsimPacking(b, t[i]);
}

template<typename T>
void doParsimArrayUnpacking(omnetpp::cCommBuffer *b, T *t, int n)
{
    for (int i = 0; i < n; i++)
        doParsimUnpacking(b, t[i]);
}

// Default rule to prevent compiler from choosing base class' doParsimPacking() function
template<typename T>
void doParsimPacking(omnetpp::cCommBuffer *, const T& t)
{
    throw omnetpp::cRuntimeError("Parsim error: No doParsimPacking() function for type %s", omnetpp::opp_typename(typeid(t)));
}

template<typename T>
void doParsimUnpacking(omnetpp::cCommBuffer *, T& t)
{
    throw omnetpp::cRuntimeError("Parsim error: No doParsimUnpacking() function for type %s", omnetpp::opp_typename(typeid(t)));
}

}  // namespace omnetpp


// forward
template<typename T, typename A>
std::ostream& operator<<(std::ostream& out, const std::vector<T,A>& vec);

// Template rule which fires if a struct or class doesn't have operator<<
template<typename T>
inline std::ostream& operator<<(std::ostream& out,const T&) {return out;}

// operator<< for std::vector<T>
template<typename T, typename A>
inline std::ostream& operator<<(std::ostream& out, const std::vector<T,A>& vec)
{
    out.put('{');
    for(typename std::vector<T,A>::const_iterator it = vec.begin(); it != vec.end(); ++it)
    {
        if (it != vec.begin()) {
            out.put(','); out.put(' ');
        }
        out << *it;
    }
    out.put('}');
    
    char buf[32];
    sprintf(buf, " (size=%u)", (unsigned int)vec.size());
    out.write(buf, strlen(buf));
    return out;
}

Register_Class(ackMsg)

ackMsg::ackMsg(const char *name, short kind) : ::omnetpp::cPacket(name,kind)
{
    this->transactionId = 0;
    this->receiver = 0;
    this->htlcIndex = 0;
    this->pathIndex = 0;
    this->timeSent = 0;
    this->isSuccess = false;
    this->failedHopNum = 0;
    this->secret = "";
    this->amount = 0;
    this->hasTimeOut = false;
}

ackMsg::ackMsg(const ackMsg& other) : ::omnetpp::cPacket(other)
{
    copy(other);
}

ackMsg::~ackMsg()
{
}

ackMsg& ackMsg::operator=(const ackMsg& other)
{
    if (this==&other) return *this;
    ::omnetpp::cPacket::operator=(other);
    copy(other);
    return *this;
}

void ackMsg::copy(const ackMsg& other)
{
    this->transactionId = other.transactionId;
    this->receiver = other.receiver;
    this->htlcIndex = other.htlcIndex;
    this->pathIndex = other.pathIndex;
    this->timeSent = other.timeSent;
    this->isSuccess = other.isSuccess;
    this->failedHopNum = other.failedHopNum;
    this->secret = other.secret;
    this->amount = other.amount;
    this->hasTimeOut = other.hasTimeOut;
}

void ackMsg::parsimPack(omnetpp::cCommBuffer *b) const
{
    ::omnetpp::cPacket::parsimPack(b);
    doParsimPacking(b,this->transactionId);
    doParsimPacking(b,this->receiver);
    doParsimPacking(b,this->htlcIndex);
    doParsimPacking(b,this->pathIndex);
    doParsimPacking(b,this->timeSent);
    doParsimPacking(b,this->isSuccess);
    doParsimPacking(b,this->failedHopNum);
    doParsimPacking(b,this->secret);
    doParsimPacking(b,this->amount);
    doParsimPacking(b,this->hasTimeOut);
}

void ackMsg::parsimUnpack(omnetpp::cCommBuffer *b)
{
    ::omnetpp::cPacket::parsimUnpack(b);
    doParsimUnpacking(b,this->transactionId);
    doParsimUnpacking(b,this->receiver);
    doParsimUnpacking(b,this->htlcIndex);
    doParsimUnpacking(b,this->pathIndex);
    doParsimUnpacking(b,this->timeSent);
    doParsimUnpacking(b,this->isSuccess);
    doParsimUnpacking(b,this->failedHopNum);
    doParsimUnpacking(b,this->secret);
    doParsimUnpacking(b,this->amount);
    doParsimUnpacking(b,this->hasTimeOut);
}

int ackMsg::getTransactionId() const
{
    return this->transactionId;
}

void ackMsg::setTransactionId(int transactionId)
{
    this->transactionId = transactionId;
}

int ackMsg::getReceiver() const
{
    return this->receiver;
}

void ackMsg::setReceiver(int receiver)
{
    this->receiver = receiver;
}

int ackMsg::getHtlcIndex() const
{
    return this->htlcIndex;
}

void ackMsg::setHtlcIndex(int htlcIndex)
{
    this->htlcIndex = htlcIndex;
}

int ackMsg::getPathIndex() const
{
    return this->pathIndex;
}

void ackMsg::setPathIndex(int pathIndex)
{
    this->pathIndex = pathIndex;
}

double ackMsg::getTimeSent() const
{
    return this->timeSent;
}

void ackMsg::setTimeSent(double timeSent)
{
    this->timeSent = timeSent;
}

bool ackMsg::getIsSuccess() const
{
    return this->isSuccess;
}

void ackMsg::setIsSuccess(bool isSuccess)
{
    this->isSuccess = isSuccess;
}

int ackMsg::getFailedHopNum() const
{
    return this->failedHopNum;
}

void ackMsg::setFailedHopNum(int failedHopNum)
{
    this->failedHopNum = failedHopNum;
}

const char * ackMsg::getSecret() const
{
    return this->secret.c_str();
}

void ackMsg::setSecret(const char * secret)
{
    this->secret = secret;
}

double ackMsg::getAmount() const
{
    return this->amount;
}

void ackMsg::setAmount(double amount)
{
    this->amount = amount;
}

bool ackMsg::getHasTimeOut() const
{
    return this->hasTimeOut;
}

void ackMsg::setHasTimeOut(bool hasTimeOut)
{
    this->hasTimeOut = hasTimeOut;
}

class ackMsgDescriptor : public omnetpp::cClassDescriptor
{
  private:
    mutable const char **propertynames;
  public:
    ackMsgDescriptor();
    virtual ~ackMsgDescriptor();

    virtual bool doesSupport(omnetpp::cObject *obj) const override;
    virtual const char **getPropertyNames() const override;
    virtual const char *getProperty(const char *propertyname) const override;
    virtual int getFieldCount() const override;
    virtual const char *getFieldName(int field) const override;
    virtual int findField(const char *fieldName) const override;
    virtual unsigned int getFieldTypeFlags(int field) const override;
    virtual const char *getFieldTypeString(int field) const override;
    virtual const char **getFieldPropertyNames(int field) const override;
    virtual const char *getFieldProperty(int field, const char *propertyname) const override;
    virtual int getFieldArraySize(void *object, int field) const override;

    virtual const char *getFieldDynamicTypeString(void *object, int field, int i) const override;
    virtual std::string getFieldValueAsString(void *object, int field, int i) const override;
    virtual bool setFieldValueAsString(void *object, int field, int i, const char *value) const override;

    virtual const char *getFieldStructName(int field) const override;
    virtual void *getFieldStructValuePointer(void *object, int field, int i) const override;
};

Register_ClassDescriptor(ackMsgDescriptor)

ackMsgDescriptor::ackMsgDescriptor() : omnetpp::cClassDescriptor("ackMsg", "omnetpp::cPacket")
{
    propertynames = nullptr;
}

ackMsgDescriptor::~ackMsgDescriptor()
{
    delete[] propertynames;
}

bool ackMsgDescriptor::doesSupport(omnetpp::cObject *obj) const
{
    return dynamic_cast<ackMsg *>(obj)!=nullptr;
}

const char **ackMsgDescriptor::getPropertyNames() const
{
    if (!propertynames) {
        static const char *names[] = {  nullptr };
        omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
        const char **basenames = basedesc ? basedesc->getPropertyNames() : nullptr;
        propertynames = mergeLists(basenames, names);
    }
    return propertynames;
}

const char *ackMsgDescriptor::getProperty(const char *propertyname) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    return basedesc ? basedesc->getProperty(propertyname) : nullptr;
}

int ackMsgDescriptor::getFieldCount() const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    return basedesc ? 10+basedesc->getFieldCount() : 10;
}

unsigned int ackMsgDescriptor::getFieldTypeFlags(int field) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldTypeFlags(field);
        field -= basedesc->getFieldCount();
    }
    static unsigned int fieldTypeFlags[] = {
        FD_ISEDITABLE,
        FD_ISEDITABLE,
        FD_ISEDITABLE,
        FD_ISEDITABLE,
        FD_ISEDITABLE,
        FD_ISEDITABLE,
        FD_ISEDITABLE,
        FD_ISEDITABLE,
        FD_ISEDITABLE,
        FD_ISEDITABLE,
    };
    return (field>=0 && field<10) ? fieldTypeFlags[field] : 0;
}

const char *ackMsgDescriptor::getFieldName(int field) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldName(field);
        field -= basedesc->getFieldCount();
    }
    static const char *fieldNames[] = {
        "transactionId",
        "receiver",
        "htlcIndex",
        "pathIndex",
        "timeSent",
        "isSuccess",
        "failedHopNum",
        "secret",
        "amount",
        "hasTimeOut",
    };
    return (field>=0 && field<10) ? fieldNames[field] : nullptr;
}

int ackMsgDescriptor::findField(const char *fieldName) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    int base = basedesc ? basedesc->getFieldCount() : 0;
    if (fieldName[0]=='t' && strcmp(fieldName, "transactionId")==0) return base+0;
    if (fieldName[0]=='r' && strcmp(fieldName, "receiver")==0) return base+1;
    if (fieldName[0]=='h' && strcmp(fieldName, "htlcIndex")==0) return base+2;
    if (fieldName[0]=='p' && strcmp(fieldName, "pathIndex")==0) return base+3;
    if (fieldName[0]=='t' && strcmp(fieldName, "timeSent")==0) return base+4;
    if (fieldName[0]=='i' && strcmp(fieldName, "isSuccess")==0) return base+5;
    if (fieldName[0]=='f' && strcmp(fieldName, "failedHopNum")==0) return base+6;
    if (fieldName[0]=='s' && strcmp(fieldName, "secret")==0) return base+7;
    if (fieldName[0]=='a' && strcmp(fieldName, "amount")==0) return base+8;
    if (fieldName[0]=='h' && strcmp(fieldName, "hasTimeOut")==0) return base+9;
    return basedesc ? basedesc->findField(fieldName) : -1;
}

const char *ackMsgDescriptor::getFieldTypeString(int field) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldTypeString(field);
        field -= basedesc->getFieldCount();
    }
    static const char *fieldTypeStrings[] = {
        "int",
        "int",
        "int",
        "int",
        "double",
        "bool",
        "int",
        "string",
        "double",
        "bool",
    };
    return (field>=0 && field<10) ? fieldTypeStrings[field] : nullptr;
}

const char **ackMsgDescriptor::getFieldPropertyNames(int field) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldPropertyNames(field);
        field -= basedesc->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    }
}

const char *ackMsgDescriptor::getFieldProperty(int field, const char *propertyname) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldProperty(field, propertyname);
        field -= basedesc->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    }
}

int ackMsgDescriptor::getFieldArraySize(void *object, int field) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldArraySize(object, field);
        field -= basedesc->getFieldCount();
    }
    ackMsg *pp = (ackMsg *)object; (void)pp;
    switch (field) {
        default: return 0;
    }
}

const char *ackMsgDescriptor::getFieldDynamicTypeString(void *object, int field, int i) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldDynamicTypeString(object,field,i);
        field -= basedesc->getFieldCount();
    }
    ackMsg *pp = (ackMsg *)object; (void)pp;
    switch (field) {
        default: return nullptr;
    }
}

std::string ackMsgDescriptor::getFieldValueAsString(void *object, int field, int i) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldValueAsString(object,field,i);
        field -= basedesc->getFieldCount();
    }
    ackMsg *pp = (ackMsg *)object; (void)pp;
    switch (field) {
        case 0: return long2string(pp->getTransactionId());
        case 1: return long2string(pp->getReceiver());
        case 2: return long2string(pp->getHtlcIndex());
        case 3: return long2string(pp->getPathIndex());
        case 4: return double2string(pp->getTimeSent());
        case 5: return bool2string(pp->getIsSuccess());
        case 6: return long2string(pp->getFailedHopNum());
        case 7: return oppstring2string(pp->getSecret());
        case 8: return double2string(pp->getAmount());
        case 9: return bool2string(pp->getHasTimeOut());
        default: return "";
    }
}

bool ackMsgDescriptor::setFieldValueAsString(void *object, int field, int i, const char *value) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->setFieldValueAsString(object,field,i,value);
        field -= basedesc->getFieldCount();
    }
    ackMsg *pp = (ackMsg *)object; (void)pp;
    switch (field) {
        case 0: pp->setTransactionId(string2long(value)); return true;
        case 1: pp->setReceiver(string2long(value)); return true;
        case 2: pp->setHtlcIndex(string2long(value)); return true;
        case 3: pp->setPathIndex(string2long(value)); return true;
        case 4: pp->setTimeSent(string2double(value)); return true;
        case 5: pp->setIsSuccess(string2bool(value)); return true;
        case 6: pp->setFailedHopNum(string2long(value)); return true;
        case 7: pp->setSecret((value)); return true;
        case 8: pp->setAmount(string2double(value)); return true;
        case 9: pp->setHasTimeOut(string2bool(value)); return true;
        default: return false;
    }
}

const char *ackMsgDescriptor::getFieldStructName(int field) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldStructName(field);
        field -= basedesc->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    };
}

void *ackMsgDescriptor::getFieldStructValuePointer(void *object, int field, int i) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldStructValuePointer(object, field, i);
        field -= basedesc->getFieldCount();
    }
    ackMsg *pp = (ackMsg *)object; (void)pp;
    switch (field) {
        default: return nullptr;
    }
}


