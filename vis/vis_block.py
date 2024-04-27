import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import re

# Set Configs
case_name = "exp1/exp1_multicast_450_t16n"
nmax = 4
font_size = 18
logdata = []
BlueList = ['#92B4F4', '#7772CA', '#5E7CE2', '#C6CDFF', '#248BD6', '#0F6BAE']
RedList  = ['#FFB3A7', '#8C4356', '#9D2933']
vis_callback_only = False

# Load File
all_data=[]
bgn_data=[]
end_data=[]
latency_data=[]

for i in range(nmax):
    
    all_data.append([])
    bgn_data.append([])
    end_data.append([])
    latency_data.append([])

    fileHandler = open("./logs/" + case_name + str(i+1) + ".log", "r")
    listOfLines = fileHandler.readlines()
    fileHandler.close()
    
    # Process Raw Data
    for line in listOfLines:
        number = re.findall("\d+", line)
        if len(re.findall("Blocking", line)) > 0:
            time = float(number[-2]) + float(number[-1]) / 1000000
            all_data[-1].append(time)
        if len(re.findall("Regular_callback26", line)) > 0:
            time = float(number[-2]) + float(number[-1]) / 1000000
            end_data[-1].append(time)
        if len(re.findall("Timer_callback1", line)) > 0:
            time = float(number[-2]) + float(number[-1]) / 1000000
            bgn_data[-1].append(time)
    
    # Get Latency
    k = 0
    while k + 1 < len(end_data[-1]):
        # print(i, k, bgn_data[-1][k], end_data[-1][k+1])
        latency_data[-1].append(end_data[-1][k+1] - bgn_data[-1][k])
        k += 2
            

fig,axes=plt.subplots(nrows=1, ncols=2, figsize=(9,4))
bplot0 = axes[0].boxplot(all_data, vert=True, patch_artist=True)
bplot1 = axes[1].boxplot(latency_data, vert=True, patch_artist=True)

# Save File
plt.savefig("./figures/" + case_name + ".png")
for i in range(len(all_data)):
    print("n = ", i+1, "block avg", np.mean(all_data[i]), "sum", np.sum(all_data[i]))
    print("     latency avg", np.mean(latency_data[i]), "num", len(latency_data[i]))
