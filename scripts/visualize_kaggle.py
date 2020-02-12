import numpy as np
import matplotlib.pyplot as plt
import math

amt_dist = np.load('data/amt_dist.npy')
sizes = amt_dist.item().get('bins')
prob = amt_dist.item().get('p')
index = -1

cdf = []
cdf.append(prob[0])
for i in range(1, len(prob)):
    cdf.append(prob[i] + cdf[i - 1])
    if sizes[i] > 250 and index == -1:
        index = i

#plt.plot(sizes, cdf)
#plt.show()

cut_off_sizes = sizes[:index + 1]
cut_off_pdfs = prob[:index + 1]
new_pdf = [x/sum(cut_off_pdfs) for x in cut_off_pdfs]
mean = np.average(cut_off_sizes, weights=new_pdf)

np.save('data/amt_dist_cutoff.npy', np.array({'p':np.array(new_pdf), 'bins':np.array(cut_off_sizes)}))



new_cdf = []
new_cdf.append(new_pdf[0])
exp_x2 = ((sizes[0] - mean) ** 2) * new_pdf[0]
for i in range(1, len(new_pdf)):
    new_cdf.append(new_pdf[i] + new_cdf[i - 1])
    exp_x2 += ((sizes[i] - mean) ** 2) * new_pdf[i]

#plt.plot(cut_off_sizes, new_cdf)
#plt.show()

print(math.sqrt(exp_x2))
print(mean)



