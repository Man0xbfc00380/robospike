import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import re

# Set Configs
pgo_case_name = "exp2/gpu_pgo_10min"

nmax = 1
font_size = 28
logdata = []
BlueList = ['#92B4F4', '#7772CA', '#5E7CE2', '#C6CDFF', '#248BD6', '#0F6BAE']
RedList  = ['#FFB3A7', '#8C4356', '#9D2933']
vis_callback_only = False

# Load File
sync_data=[]
await_data=[]
latency_data=[]
eval_data=[]

for i in range(nmax):
    # Process Raw Data
    fileHandler = open("./logs/" + pgo_case_name + ".log", "r")
    listOfLines = fileHandler.readlines()
    fileHandler.close()
    for line in listOfLines:
        number = re.findall("\d+", line)
        if len(re.findall("Base:", line)) > 0:
            sync_data.append(float(number[-1]))
            await_data.append(float(number[-2]))
            latency_data.append(float(number[-3]))
            if len(re.findall("Score: -", line)) > 0:
                eval_data.append(-1 * float(number[-4]))
            else:
                eval_data.append(float(number[-4]))

fig, axes = plt.subplots(1, 1, figsize=(10,8))
iters=list(range(len(sync_data)))

print(iters)
plt.plot(iters, await_data, color=BlueList[0], label="Await Time", linewidth=3.5)
plt.plot(iters, latency_data, color=BlueList[1], label="Latency", linewidth=3.5)
plt.plot(iters, sync_data, color=RedList[1], label="Sync Overhead", linewidth=3.5)
# plt.plot(iters, eval_data, color=RedList[0], label="Eval Function", linewidth=3.5)

plt.grid()

plt.legend(fontsize=font_size-2)

plt.xticks(fontsize=font_size)
plt.yticks(fontsize=font_size)
# Save File
plt.savefig("./figures/" + pgo_case_name + ".png", bbox_inches='tight')