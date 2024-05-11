import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import re

# Set Configs
pgo_case_name = "co_t_mexe_f"  + "_2"#+ "_auto_f" + "_half"
targ = "11"
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
base_data = 0

for i in range(nmax):
    # Process Raw Data
    fileHandler = open("./logs/exp1/" + pgo_case_name + ".log", "r")
    listOfLines = fileHandler.readlines()
    fileHandler.close()
    for line in listOfLines:
        is_11 = True if len(re.findall("Regular_callback" + targ, line)) > 0 else False
        number = re.findall("\d+", line)
        if len(re.findall("Base:", line)) > 0 and is_11:
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

plt.plot(iters, sync_data, color=BlueList[1], label="$T_{sync}$", linewidth=3.5)
plt.plot(iters, latency_data, color=RedList[1], label="$T_{all}$", linewidth=3.5)
plt.plot(iters, await_data, color=BlueList[0], label="$T_{await}$", linewidth=3.5)
plt.fill_between(iters, await_data, color=BlueList[0], alpha=0.3)

plt.grid()

plt.legend(fontsize=18)

plt.xticks(fontsize=font_size)
plt.yticks(fontsize=font_size)
# Save File
plt.savefig("./figures/ana/" + pgo_case_name + "_" + targ + ".png", bbox_inches='tight')

print(pgo_case_name, np.sum(latency_data),  ((float)(np.sum(await_data) / np.sum(latency_data))))