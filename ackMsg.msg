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
 	int htlcIndex;
 	double timeSent;
 	bool isSuccess; //status, true for success, false for failure
    string secret=""; 
    double amount;
    bool hasTimeOut;
 }

