import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import re

# Set Configs
pgo_case_name = "co_t_mexe_f_utilize"
nmax = 1
font_size = 28
logdata = []
BlueList = ['#92B4F4', '#7772CA', '#5E7CE2', '#C6CDFF', '#248BD6', '#0F6BAE']
RedList  = ['#FFB3A7', '#8C4356', '#9D2933']
vis_callback_only = False

# Load File
sync_data=[]
coroutine_len = []
base_data = 0

for i in range(nmax):
    # Process Raw Data
    fileHandler = open("./logs/exp1/" + pgo_case_name + ".log", "r")
    listOfLines = fileHandler.readlines()
    fileHandler.close()
    for line in listOfLines:
        number = re.findall("\d+", line)
        if len(re.findall("get_rest_coroutine", line)) > 0:
            coroutine_len.append(float(number[-1]))

fig, axes = plt.subplots(1, 1, figsize=(10,8))
iters=list(range(len(coroutine_len)))

plt.plot(iters, coroutine_len, color=BlueList[2], label="$coroutine\_queue$ length", linewidth=3.5)

plt.grid()

plt.legend(fontsize=18)

plt.xticks(fontsize=font_size)
plt.yticks(fontsize=font_size)
# Save File
plt.savefig("./figures/ana/exp5_len.png", bbox_inches='tight')