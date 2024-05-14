import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import re

# Set Configs
case_name = "exp0/exp0_n"
nmax = 10
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
        if len(re.findall("Regular_callback11", line)) > 0:
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
            
sum_data = []
for i in range(len(all_data)):
    sum_data.append(np.sum(all_data[i]))

avg_data = []
for i in range(len(latency_data)):
    avg_data.append(np.mean(latency_data[i]))

color_select = [BlueList[0], BlueList[0], BlueList[1], BlueList[1], BlueList[2], BlueList[2], \
                BlueList[3], BlueList[3], BlueList[3], BlueList[3], BlueList[4], BlueList[4], BlueList[4], BlueList[4]]
fig,axes=plt.subplots(nrows=2, ncols=2, figsize=(10, 8))
bplot0 = axes[0][0].boxplot(all_data, vert=True, patch_artist=True)
for patch, color in zip(bplot0['boxes'], color_select):
    patch.set_facecolor(color)
axes[0][0].set_title("Block Latency")

bplot1 = axes[0][1].boxplot(latency_data, vert=True, patch_artist=True)
for patch, color in zip(bplot1['boxes'], color_select):
    patch.set_facecolor(color)
axes[0][1].set_title("E2E Latency")

bar1 = axes[1][0].bar(range(1, len(avg_data)+1), avg_data, edgecolor='black', color=color_select)
axes[1][0].set_title("Average E2E Latency")

bar2 = axes[1][1].bar(range(1, len(sum_data)+1), sum_data, edgecolor='black', color=color_select)
axes[1][1].set_title("Sum Block Latency")

# Save File
plt.savefig("./figures/" + case_name + ".png", bbox_inches='tight')
for i in range(len(all_data)):
    print("n = ", i+1, "latency avg", np.mean(latency_data[i]), "block sum", np.sum(all_data[i]))