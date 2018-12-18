import sys
import textwrap


# workload file of form
# [amount] [timeSent] [sender] [receiver] [priorityClass]

outfile = open(sys.argv[1], "w")

totalTime = 10

#edge a->b appears in index i as rateListStart[i]=a, and rateListEnd[i]=b
rateListStart = [1,0,2]; 
rateListEnd = [2,2,0]; 
rateListAmtTemp = [2,1,2];
rateListAmt = [50*x for x in rateListAmtTemp]
timeOut = 5.0

for i in range(totalTime):
   for k in range(len(rateListStart)):
      for l in range(rateListAmt[k]):
         rate = rateListAmt[k]
         timeStart = i*1.0 + (l*1.0)/(1.0*rate)
         outfile.write("1 "+str(timeStart)+" "+str(rateListStart[k])+" "+str(rateListEnd[k])+" 0\n") # " + str(timeOut) + " \n");


