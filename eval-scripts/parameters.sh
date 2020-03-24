#!/bin/bash

# generally fixed parameters
delay="30"
demand_scale="3"

#general parameters that do not affect config names
simulationLength=1010
statCollectionRate=10
timeoutClearRate=1
timeoutEnabled=true
signalsEnabled=true
loggingEnabled=false
transStatStart=800
transStatEnd=1000
mtu=1.0

# scheme specific parameters
eta=0.025
alpha=0.2
kappa=0.025
updateQueryTime=1.5
minPriceRate=0.25
zeta=0.01
rho=0.04
tau=10
normalizer=100
xi=1
routerQueueDrainTime=5
serviceArrivalWindow=300

#DCTCP parameters
windowBeta=0.1
windowAlpha=10
queueThreshold=160
queueDelayThreshold=300
balanceThreshold=0.1
minDCTCPWindow=1
rateDecreaseFrequency=3.0
