import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import re

# Set Configs
pgo_case_name = "co_t_mexe_f_wcet"
nmax = 1
font_size = 28
logdata = []
BlueList = ['#92B4F4', '#7772CA', '#5E7CE2', '#C6CDFF', '#248BD6', '#0F6BAE']
RedList  = ['#FFB3A7', '#8C4356', '#9D2933']
vis_callback_only = False

# Load File
sync_data=[]
coroutine_len = [0,0,0]
base_data = 0

for i in range(nmax):
    # Process Raw Data
    fileHandler = open("./logs/exp1/" + pgo_case_name + ".log", "r")
    listOfLines = fileHandler.readlines()
    fileHandler.close()
    for line in listOfLines:
        number = re.findall("\d+", line)
        if len(re.findall("get_rest_coroutine", line)) > 0:
            coroutine_len[int(number[-1]) - 1] += 1

fig, axes = plt.subplots(1, 1, figsize=(5,8))
sum = (coroutine_len[0]+coroutine_len[1]+coroutine_len[2])
coroutine_len[0] = coroutine_len[0] / sum
coroutine_len[1] = coroutine_len[1] / sum
coroutine_len[2] = coroutine_len[2] / sum

bar1 = axes.bar([1,2,3], coroutine_len, edgecolor='black', color=BlueList[0])

for i in range(3):
    plt.text(i+1, coroutine_len[i], '{:.3f}'.format(coroutine_len[i]), va="bottom", ha="center", fontsize=font_size)

plt.grid()
axes.spines['top'].set_visible(False)
axes.spines['right'].set_visible(False)
plt.xticks(fontsize=font_size)
plt.yticks(fontsize=font_size)
plt.ylabel("$coroutine\_queue$ length", fontsize=font_size)
# Save File
plt.savefig("./figures/ana/exp5_len.png", bbox_inches='tight')