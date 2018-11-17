import sys
import textwrap


# workload file of form
# [amount] [timeSent] [sender] [receiver] [priorityClass]

outfile = open(sys.argv[1], "w")

totalTime = 100

#edge a->b appears in index i as rateListStart[i]=a, and rateListEnd[i]=b
rateListStart = [0,3,0,1,2,3,2,4]; 
rateListEnd = [4,0,1,3,1,2,4,2]; 
rateListAmt = [1,2,1,2,1,2,2,2];

for i in range(totalTime):
   for k in range(len(rateListStart)):
      for l in range(rateListAmt[k]):
         outfile.write("5 "+str(i)+" "+str(rateListStart[k])+" "+str(rateListEnd[k])+" 0\n");


