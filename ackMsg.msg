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


packet ackMsg
{
 	int transactionId;
 	int receiver;
 	int htlcIndex;
 	int pathIndex; 
 	double timeSent;
 	bool isSuccess; //status, true for success, false for failure
        int failedHopNum;
    string secret=""; 
    double amount;
    int priorityClass;
    bool hasTimeOut;
    double timeOut;
    double largerTxnId;
    bool isMarked;
    double timeAttempted;
 }

