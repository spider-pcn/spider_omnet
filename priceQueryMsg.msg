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
#include <map>
typedef std::vector<int> IntVector;
typedef std::vector<double> DoubleVector;
typedef std::map<int, double> IntMap;
}};

class noncobject IntVector;
class noncobject IntMap;
class noncobject DoubleVector;

packet priceQueryMsg
//if encapIsAck == false needs to look in encapsulated transactionMsg for priority class and amount
{
    double zValue = 0;
    int pathIndex = 0;
    bool isReversed = false;
}