//
// Generated file, do not edit! Created by nedtool 5.4 from transactionMsg.msg.
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
#include "transactionMsg_m.h"

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

Register_Class(transactionMsg)

transactionMsg::transactionMsg(const char *name, short kind) : ::omnetpp::cPacket(name,kind)
{
    this->amount = 0;
    this->timeSent = 0;
    this->sender = 0;
    this->receiver = 0;
    this->priorityClass = 0;
    this->transactionId = 0;
    this->hasTimeOut = false;
    this->timeOut = 0;
    this->htlcIndex = 0;
    this->routeIndex = 0;
}

transactionMsg::transactionMsg(const transactionMsg& other) : ::omnetpp::cPacket(other)
{
    copy(other);
}

transactionMsg::~transactionMsg()
{
}

transactionMsg& transactionMsg::operator=(const transactionMsg& other)
{
    if (this==&other) return *this;
    ::omnetpp::cPacket::operator=(other);
    copy(other);
    return *this;
}

void transactionMsg::copy(const transactionMsg& other)
{
    this->amount = other.amount;
    this->timeSent = other.timeSent;
    this->sender = other.sender;
    this->receiver = other.receiver;
    this->priorityClass = other.priorityClass;
    this->transactionId = other.transactionId;
    this->hasTimeOut = other.hasTimeOut;
    this->timeOut = other.timeOut;
    this->htlcIndex = other.htlcIndex;
    this->routeIndex = other.routeIndex;
}

void transactionMsg::parsimPack(omnetpp::cCommBuffer *b) const
{
    ::omnetpp::cPacket::parsimPack(b);
    doParsimPacking(b,this->amount);
    doParsimPacking(b,this->timeSent);
    doParsimPacking(b,this->sender);
    doParsimPacking(b,this->receiver);
    doParsimPacking(b,this->priorityClass);
    doParsimPacking(b,this->transactionId);
    doParsimPacking(b,this->hasTimeOut);
    doParsimPacking(b,this->timeOut);
    doParsimPacking(b,this->htlcIndex);
    doParsimPacking(b,this->routeIndex);
}

void transactionMsg::parsimUnpack(omnetpp::cCommBuffer *b)
{
    ::omnetpp::cPacket::parsimUnpack(b);
    doParsimUnpacking(b,this->amount);
    doParsimUnpacking(b,this->timeSent);
    doParsimUnpacking(b,this->sender);
    doParsimUnpacking(b,this->receiver);
    doParsimUnpacking(b,this->priorityClass);
    doParsimUnpacking(b,this->transactionId);
    doParsimUnpacking(b,this->hasTimeOut);
    doParsimUnpacking(b,this->timeOut);
    doParsimUnpacking(b,this->htlcIndex);
    doParsimUnpacking(b,this->routeIndex);
}

double transactionMsg::getAmount() const
{
    return this->amount;
}

void transactionMsg::setAmount(double amount)
{
    this->amount = amount;
}

double transactionMsg::getTimeSent() const
{
    return this->timeSent;
}

void transactionMsg::setTimeSent(double timeSent)
{
    this->timeSent = timeSent;
}

int transactionMsg::getSender() const
{
    return this->sender;
}

void transactionMsg::setSender(int sender)
{
    this->sender = sender;
}

int transactionMsg::getReceiver() const
{
    return this->receiver;
}

void transactionMsg::setReceiver(int receiver)
{
    this->receiver = receiver;
}

int transactionMsg::getPriorityClass() const
{
    return this->priorityClass;
}

void transactionMsg::setPriorityClass(int priorityClass)
{
    this->priorityClass = priorityClass;
}

int transactionMsg::getTransactionId() const
{
    return this->transactionId;
}

void transactionMsg::setTransactionId(int transactionId)
{
    this->transactionId = transactionId;
}

bool transactionMsg::getHasTimeOut() const
{
    return this->hasTimeOut;
}

void transactionMsg::setHasTimeOut(bool hasTimeOut)
{
    this->hasTimeOut = hasTimeOut;
}

double transactionMsg::getTimeOut() const
{
    return this->timeOut;
}

void transactionMsg::setTimeOut(double timeOut)
{
    this->timeOut = timeOut;
}

int transactionMsg::getHtlcIndex() const
{
    return this->htlcIndex;
}

void transactionMsg::setHtlcIndex(int htlcIndex)
{
    this->htlcIndex = htlcIndex;
}

int transactionMsg::getRouteIndex() const
{
    return this->routeIndex;
}

void transactionMsg::setRouteIndex(int routeIndex)
{
    this->routeIndex = routeIndex;
}

class transactionMsgDescriptor : public omnetpp::cClassDescriptor
{
  private:
    mutable const char **propertynames;
  public:
    transactionMsgDescriptor();
    virtual ~transactionMsgDescriptor();

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

Register_ClassDescriptor(transactionMsgDescriptor)

transactionMsgDescriptor::transactionMsgDescriptor() : omnetpp::cClassDescriptor("transactionMsg", "omnetpp::cPacket")
{
    propertynames = nullptr;
}

transactionMsgDescriptor::~transactionMsgDescriptor()
{
    delete[] propertynames;
}

bool transactionMsgDescriptor::doesSupport(omnetpp::cObject *obj) const
{
    return dynamic_cast<transactionMsg *>(obj)!=nullptr;
}

const char **transactionMsgDescriptor::getPropertyNames() const
{
    if (!propertynames) {
        static const char *names[] = {  nullptr };
        omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
        const char **basenames = basedesc ? basedesc->getPropertyNames() : nullptr;
        propertynames = mergeLists(basenames, names);
    }
    return propertynames;
}

const char *transactionMsgDescriptor::getProperty(const char *propertyname) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    return basedesc ? basedesc->getProperty(propertyname) : nullptr;
}

int transactionMsgDescriptor::getFieldCount() const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    return basedesc ? 10+basedesc->getFieldCount() : 10;
}

unsigned int transactionMsgDescriptor::getFieldTypeFlags(int field) const
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

const char *transactionMsgDescriptor::getFieldName(int field) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldName(field);
        field -= basedesc->getFieldCount();
    }
    static const char *fieldNames[] = {
        "amount",
        "timeSent",
        "sender",
        "receiver",
        "priorityClass",
        "transactionId",
        "hasTimeOut",
        "timeOut",
        "htlcIndex",
        "routeIndex",
    };
    return (field>=0 && field<10) ? fieldNames[field] : nullptr;
}

int transactionMsgDescriptor::findField(const char *fieldName) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    int base = basedesc ? basedesc->getFieldCount() : 0;
    if (fieldName[0]=='a' && strcmp(fieldName, "amount")==0) return base+0;
    if (fieldName[0]=='t' && strcmp(fieldName, "timeSent")==0) return base+1;
    if (fieldName[0]=='s' && strcmp(fieldName, "sender")==0) return base+2;
    if (fieldName[0]=='r' && strcmp(fieldName, "receiver")==0) return base+3;
    if (fieldName[0]=='p' && strcmp(fieldName, "priorityClass")==0) return base+4;
    if (fieldName[0]=='t' && strcmp(fieldName, "transactionId")==0) return base+5;
    if (fieldName[0]=='h' && strcmp(fieldName, "hasTimeOut")==0) return base+6;
    if (fieldName[0]=='t' && strcmp(fieldName, "timeOut")==0) return base+7;
    if (fieldName[0]=='h' && strcmp(fieldName, "htlcIndex")==0) return base+8;
    if (fieldName[0]=='r' && strcmp(fieldName, "routeIndex")==0) return base+9;
    return basedesc ? basedesc->findField(fieldName) : -1;
}

const char *transactionMsgDescriptor::getFieldTypeString(int field) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldTypeString(field);
        field -= basedesc->getFieldCount();
    }
    static const char *fieldTypeStrings[] = {
        "double",
        "double",
        "int",
        "int",
        "int",
        "int",
        "bool",
        "double",
        "int",
        "int",
    };
    return (field>=0 && field<10) ? fieldTypeStrings[field] : nullptr;
}

const char **transactionMsgDescriptor::getFieldPropertyNames(int field) const
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

const char *transactionMsgDescriptor::getFieldProperty(int field, const char *propertyname) const
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

int transactionMsgDescriptor::getFieldArraySize(void *object, int field) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldArraySize(object, field);
        field -= basedesc->getFieldCount();
    }
    transactionMsg *pp = (transactionMsg *)object; (void)pp;
    switch (field) {
        default: return 0;
    }
}

const char *transactionMsgDescriptor::getFieldDynamicTypeString(void *object, int field, int i) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldDynamicTypeString(object,field,i);
        field -= basedesc->getFieldCount();
    }
    transactionMsg *pp = (transactionMsg *)object; (void)pp;
    switch (field) {
        default: return nullptr;
    }
}

std::string transactionMsgDescriptor::getFieldValueAsString(void *object, int field, int i) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldValueAsString(object,field,i);
        field -= basedesc->getFieldCount();
    }
    transactionMsg *pp = (transactionMsg *)object; (void)pp;
    switch (field) {
        case 0: return double2string(pp->getAmount());
        case 1: return double2string(pp->getTimeSent());
        case 2: return long2string(pp->getSender());
        case 3: return long2string(pp->getReceiver());
        case 4: return long2string(pp->getPriorityClass());
        case 5: return long2string(pp->getTransactionId());
        case 6: return bool2string(pp->getHasTimeOut());
        case 7: return double2string(pp->getTimeOut());
        case 8: return long2string(pp->getHtlcIndex());
        case 9: return long2string(pp->getRouteIndex());
        default: return "";
    }
}

bool transactionMsgDescriptor::setFieldValueAsString(void *object, int field, int i, const char *value) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->setFieldValueAsString(object,field,i,value);
        field -= basedesc->getFieldCount();
    }
    transactionMsg *pp = (transactionMsg *)object; (void)pp;
    switch (field) {
        case 0: pp->setAmount(string2double(value)); return true;
        case 1: pp->setTimeSent(string2double(value)); return true;
        case 2: pp->setSender(string2long(value)); return true;
        case 3: pp->setReceiver(string2long(value)); return true;
        case 4: pp->setPriorityClass(string2long(value)); return true;
        case 5: pp->setTransactionId(string2long(value)); return true;
        case 6: pp->setHasTimeOut(string2bool(value)); return true;
        case 7: pp->setTimeOut(string2double(value)); return true;
        case 8: pp->setHtlcIndex(string2long(value)); return true;
        case 9: pp->setRouteIndex(string2long(value)); return true;
        default: return false;
    }
}

const char *transactionMsgDescriptor::getFieldStructName(int field) const
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

void *transactionMsgDescriptor::getFieldStructValuePointer(void *object, int field, int i) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldStructValuePointer(object, field, i);
        field -= basedesc->getFieldCount();
    }
    transactionMsg *pp = (transactionMsg *)object; (void)pp;
    switch (field) {
        default: return nullptr;
    }
}


