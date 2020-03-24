#!/bin/bash
demand_scale="35"
delay="30"

#general parameters that do not affect config names
simulationLength=8000
statCollectionRate=1
timeoutClearRate=1
timeoutEnabled=true
signalsEnabled=true
loggingEnabled=false
transStatStart=0
transStatEnd=8000
mtu=1.0

# price-scheme specific best parameters
eta=0.07
alpha=0.1
kappa=0.07
updateQueryTime=1.5
minPriceRate=0.25
zeta=0.01
rho=0.04
tau=10
normalizer=100
xi=5
routerQueueDrainTime=5
serviceArrivalWindow=300
