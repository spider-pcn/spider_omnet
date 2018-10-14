/*
 * simpleNet.h
 *
 *  Created on: Oct 14, 2018
 *      Author: kathleenruan
 */

#ifndef SIMPLENET_H_
#define SIMPLENET_H_
class Job{

    double amount;
    double timeSent;  //time after start time that job is active
    int sender;
    int receiver;
    vector<int> route;
    int priorityClass;

};




#endif /* SIMPLENET_H_ */
