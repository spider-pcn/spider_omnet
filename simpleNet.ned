//
// This file is part of an OMNeT++/OMNEST simulation example.
//
// Copyright (C) 2003-2015 Andras Varga, Rudolf Hornig
//
// This file is distributed WITHOUT ANY WARRANTY. See the file
// `license' for details on this and other legal matters.
//

simple endpointNode
{
    parameters:
        @signal[arrival](type="long");
        @statistic[hopCount](title="hop count"; source="arrival"; record=vector,stats; interpolationmode=none);

        @display("i=block/routing");
        
    gates:
        input in[];
        output out[];
}

simple routerNode
{
    gates:
        input in[];
        output out[];
}



network simpleNet
{
    parameters:
        @figure[description](type=text; pos=5,20; font=,,bold; text="Random routing example - displaying last hop count");
        @figure[lasthopcount](type=text; pos=5,35; text="last hopCount: N/A");
        @display("bgb=340,261");
    types:
        channel Channel extends ned.DelayChannel
        {
            delay = 100ms;
        }
    submodules:

        end[2]: endpointNode {}
       // end1: endpointNode {}
        router[2]: routerNode {}
    connections:



        router[0].out++ --> {  delay = 100ms; } --> router[1].in++;
        router[0].in++ <-- {  delay = 100ms; } <-- router[1].out++;


        end[0].out++ --> {  delay = 100ms; } --> router[0].in++;
        end[0].in++ <-- {  delay = 100ms; } <-- router[0].out++;

      //  router[0].out++ --> {  delay = 100ms; } --> router[1].in++;
       // router[0].in++ <-- {  delay = 100ms; } <-- router[1].out++;

        router[1].out++ --> {  delay = 100ms; } --> end[1].in++;
        router[1].in++ <-- {  delay = 100ms; } <-- end[1].out++;

}
