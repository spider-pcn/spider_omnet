import sys
import textwrap


# workload file of form
# [amount] [timeSent] [sender] [receiver] [priorityClass]

outfile = open(sys.argv[1], "w")

totalTime = 10

#edge a->b appears in index i as rateListStart[i]=a, and rateListEnd[i]=b
rateListStart = [0,3,0,1,2,3,2,4]; 
rateListEnd = [4,0,1,3,1,2,4,2]; 
rateListAmtTemp = [1,2,1,2,1,2,2,2];
rateListAmt = [50*x for x in rateListAmtTemp]


for i in range(totalTime):
   for k in range(len(rateListStart)):
      for l in range(rateListAmt[k]):
         rate = rateListAmt[k]
         timeStart = i*1.0 + (l*1.0)/(1.0*rate)
         outfile.write("1 "+str(timeStart)+" "+str(rateListStart[k])+" "+str(rateListEnd[k])+" 0\n");


