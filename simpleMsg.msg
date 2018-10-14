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

message simpleMsg
{

    double amount;
    double timeSent;  //time after start time that job is active
    int sender;
    int receiver;
    IntVector route;
    int priorityClass;
    int hopCount = 0;

}

