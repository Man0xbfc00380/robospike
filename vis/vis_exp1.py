import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import re

# Set Configs
case_name_a = "exp1/co_t_mexe_f"
case_name_b = "exp1/co_f_mexe_f"

nmax = 10
font_size = 18
logdata = []
BlueList = ['#92B4F4', '#7772CA', '#5E7CE2', '#C6CDFF', '#248BD6', '#0F6BAE']
RedList  = ['#FFB3A7', '#8C4356', '#9D2933']
vis_callback_only = False

# Load File
# Chain    1-  2-  3-
timer_a = [[], [], []] 
timer_b = [[], [], []]
# Cb    1---------------------------  2-------  3---
cb_a = [[[], [], [], [], [], [], []], [[], []], [[]]]
cb_b = [[[], [], [], [], [], [], []], [[], []], [[]]]

# case a
fileHandler = open("./logs/" + case_name_a + ".log", "r")
listOfLines = fileHandler.readlines()
fileHandler.close()
logdata = []

# Process Raw Data
for line in listOfLines:
    number = re.findall("\d+", line)
    bgn_bool = True if len(re.findall("Bgn", line)) > 0 else False
    end_bool = True if len(re.findall("End", line)) > 0 else False
    is_timer = True if len(re.findall("Timer_callback", line)) > 0 else False
    is_final = True if len(re.findall("[*]", line)) > 0 else False
    if bgn_bool:
        # Create the item
        name = "Timer" + number[0] if is_timer else "Cb_" + number[0]
        color_mark = -1 if is_timer else (int(number[0]) % 10) - 1
        time = float(number[-2]) + float(number[-1]) / 1000000
        item = {
            "name":name,
            "color": color_mark,
            "thread":number[-3],
            "bgn":time,
            "end":-1
        }
        logdata.append(item)
    elif end_bool:
        # Match the item
        name = "Timer" + number[0] if is_timer else "Cb_" + number[0]
        time = float(number[-2]) + float(number[-1]) / 1000000
        for item in logdata:
            if item["name"] == name and item["thread"] == number[-3] and item["end"] == -1:
                item["end"] = time
                if is_timer: 
                    timer_a[int(number[0]) - 1].append(item["bgn"])
                else:
                    if is_final:
                        cb_a[int(int(number[0]) / 10) - 1][(int(number[0]) % 10 - 1)].append(item["end"])

# case b
fileHandler = open("./logs/" + case_name_b + ".log", "r")
listOfLines = fileHandler.readlines()
fileHandler.close()
logdata = []

# Process Raw Data
for line in listOfLines:
    number = re.findall("\d+", line)
    bgn_bool = True if len(re.findall("Bgn", line)) > 0 else False
    end_bool = True if len(re.findall("End", line)) > 0 else False
    is_timer = True if len(re.findall("Timer_callback", line)) > 0 else False
    is_final = True if len(re.findall("[*]", line)) > 0 else False
    if bgn_bool:
        # Create the item
        name = "Timer" + number[0] if is_timer else "Cb_" + number[0]
        color_mark = -1 if is_timer else (int(number[0]) % 10) - 1
        time = float(number[-2]) + float(number[-1]) / 1000000
        item = {
            "name":name,
            "color": color_mark,
            "thread":number[-3],
            "bgn":time,
            "end":-1
        }
        logdata.append(item)
    elif end_bool:
        # Match the item
        name = "Timer" + number[0] if is_timer else "Cb_" + number[0]
        time = float(number[-2]) + float(number[-1]) / 1000000
        for item in logdata:
            if item["name"] == name and item["thread"] == number[-3] and item["end"] == -1:
                item["end"] = time
                if is_timer: 
                    timer_b[int(number[0]) - 1].append(item["bgn"])
                else:
                    if is_final:
                        cb_b[int(int(number[0]) / 10) - 1][(int(number[0]) % 10 - 1)].append(item["end"])

for t in range(len(timer_a)):
    print("Timer_a", t+1, ":", len(timer_a[t]))
    for i in range(len(cb_a[t])):
        print("Cb_a", t+1, i+1, ":", len(cb_a[t][i]))
        for j in range(len(cb_a[t][i])):
            # print("[a]", t, i, j)
            cb_a[t][i][j] -= timer_a[t][j]

for t in range(len(timer_b)):
    print("Timer_b", t+1, ":", len(timer_b[t]))
    for i in range(len(cb_b[t])):
        print("Cb_b", t+1, i+1, ":", len(cb_b[t][i]))
        for j in range(len(cb_b[t][i])):
            # print("[b]", t, i, j)
            cb_b[t][i][j] -= timer_b[t][j]



data = [cb_a[0][0], cb_b[0][0], [], \
        cb_a[0][1], cb_b[0][1], [], \
        cb_a[0][2], cb_b[0][2], [], \
        cb_a[0][3], cb_b[0][3], [], \
        cb_a[0][4], cb_b[0][4], [], \
        cb_a[0][5], cb_b[0][5], [], \
        cb_a[0][6], cb_b[0][6], [], \
        cb_a[1][0], cb_b[1][0], [], \
        cb_a[1][1], cb_b[1][1], [], \
        cb_a[2][0], cb_b[2][0]]

fig,axes=plt.subplots(nrows=1, ncols=1, figsize=(20, 8))

plt.xticks(fontsize=font_size)
plt.yticks(fontsize=font_size)
plt.grid()

bplot1 = axes.boxplot(data, vert=True, patch_artist=True)
x = 0
for patch in bplot1['boxes']:
    patch.set_facecolor(BlueList[x])
    x = (x + 1) % 3

plt.ylabel("Latency / s", fontsize=font_size)
plt.savefig("./figures/ana/exp1.png", bbox_inches='tight')
