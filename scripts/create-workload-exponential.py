import sys
import textwrap
import numpy as np

# workload file of form
# [amount] [timeSent] [sender] [receiver] [priorityClass]

outfile = open(sys.argv[1], "w")

totalTime = 30

#edge a->b appears in index i as rateListStart[i]=a, and rateListEnd[i]=b
#rateListStart = [0,3,0,1,2,3,2,4]; 
#rateListEnd = [4,0,1,3,1,2,4,2]; 
#rateListAmt = [1,2,1,2,1,2,2,2];

rateListStart = [0,3]; 
rateListEnd = [3,0]; 
rateListAmt = [400, 400];



for i in range(totalTime):
   for k in range(len(rateListStart)):
      #for l in range(rateListAmt[k]):
      currentTime = 0.0
      rate = rateListAmt[k]*1.0
      beta = (1.0)/(1.0*rate)
      while (currentTime<1.0):  
         timeStart = i*1.0 + currentTime
         outfile.write("1 "+str(timeStart)+" "+str(rateListStart[k])+" "+str(rateListEnd[k])+" 0\n");
         timeIncr = np.random.exponential(beta)
         currentTime = currentTime+timeIncr

