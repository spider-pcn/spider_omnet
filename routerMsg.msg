//
// This file is part of an OMNeT++/OMNEST simulation example.
//
// Copyright (C) 2003 Ahmet Sekercioglu
// Copyright (C) 2003-2015 Andras Varga
//
// This file is distributed WITHOUT ANY WARRANTY. See the file
// `license' for details on this and other legal matters.
//


//
// Same as TictocMsg11
//
cplusplus {{
#include <vector>
typedef std::vector<int> IntVector;
}};

class noncobject IntVector;

packet routerMsg 
//if encapIsAck == false needs to look in encapsulated transactionMsg for priority class and amount
{
    IntVector route;
    int hopCount = 0;
    bool encapIsAck = false;  //false for transaction, true for ack
    int amount;
    int priorityClass;
    
}

