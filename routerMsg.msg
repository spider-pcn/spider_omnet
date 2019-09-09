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
#define TRANSACTION_MSG 0
#define ACK_MSG 1
#define UPDATE_MSG 2
#define STAT_MSG 3
#define TIME_OUT_MSG 4
#define PROBE_MSG 5
#define CLEAR_STATE_MSG 6

//new msg types for price scheme
#define TRIGGER_PRICE_UPDATE_MSG 7
#define TRIGGER_PRICE_QUERY_MSG 8
#define PRICE_UPDATE_MSG 9
#define PRICE_QUERY_MSG 10
#define TRIGGER_TRANSACTION_SEND_MSG 11
#define COMPUTE_DEMAND_MSG 12
#define TRIGGER_RATE_DECREASE_MSG 13

#define MONITOR_PATHS_MSG 14
#define TRIGGER_REBALANCING_MSG 15
#define ADD_FUNDS_MSG 16
#define COMPUTE_BALANCE_MSG 17


typedef std::vector<int> IntVector;
}};

class noncobject IntVector;

packet routerMsg 
//if encapIsAck == false needs to look in encapsulated transactionMsg for priority class and amount
{
    IntVector route;
    int hopCount = 0;
    int messageType;  //0 for transaction, 1 for ack, 2 for balance update
   
}

