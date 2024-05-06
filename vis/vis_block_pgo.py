import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import re

# Set Configs
case_name_a = "exp2/gpu_pgo_4"
case_name_b = "exp2/gpu_pgo_3"

nmax = 10
font_size = 18
logdata = []
BlueList = ['#92B4F4', '#7772CA', '#5E7CE2', '#C6CDFF', '#248BD6', '#0F6BAE']
RedList  = ['#FFB3A7', '#8C4356', '#9D2933']
vis_callback_only = False

# Load File
timer_a = []
cb_a = [[], [], [], [], [], [], []]
timer_b = []
cb_b = [[], [], [], [], [], [], []]

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
    is_second = True if len(re.findall("[*]", line)) > 0 else False
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
                    timer_a.append(item["bgn"])
                else:
                    if int(number[0]) % 10 == 1 and is_second: 
                        cb_a[(int(number[0]) % 10 - 1)].append(item["end"])
                    elif int(number[0]) % 10 != 1:
                        cb_a[(int(number[0]) % 10 - 1)].append(item["end"])

# case b
fileHandler = open("./logs/" + case_name_b + ".log", "r")
listOfLines = fileHandler.readlines()
fileHandler.close()
logdata = []

# Process Raw Data
for line in listOfLines:
    number = re.findall("\d+", line)
    bgn_bool = True if len(re.findall("Bgn", line)) > 0 else False
    is_timer = True if len(re.findall("Timer_callback", line)) > 0 else False
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
    elif len(number) >= 2:
        # Match the item
        name = "Timer" + number[0] if is_timer else "Cb_" + number[0]
        time = float(number[-2]) + float(number[-1]) / 1000000
        for item in logdata:
            if item["name"] == name and item["thread"] == number[-3] and item["end"] == -1:
                item["end"] = time
                if is_timer: timer_b.append(item["bgn"])
                else: cb_b[(int(number[0]) % 10 - 1)].append(item["end"])

print("[timer_a]", len(timer_a))
print("[timer_b]", len(timer_b))

print("[cb_a] num", len(cb_a[0]), len(cb_a[1]), len(cb_a[2]), len(cb_a[3]), len(cb_a[4]), len(cb_a[5]), len(cb_a[6]))
print("[cb_b] num", len(cb_b[0]), len(cb_b[1]), len(cb_b[2]), len(cb_b[3]), len(cb_b[4]), len(cb_b[5]), len(cb_b[6]))

for i in range(len(cb_a)):
    for j in range(len(cb_a[i])):
        cb_a[i][j] -= timer_a[j]

for i in range(len(cb_b)):
    for j in range(len(cb_b[i])):
        cb_b[i][j] -= timer_b[j]

print("[cb_a] mean", np.mean(cb_a[0]), np.mean(cb_a[1]), np.mean(cb_a[2]), np.mean(cb_a[3]), np.mean(cb_a[4]), np.mean(cb_a[5]), np.mean(cb_a[6]))
print("[cb_b] mean", np.mean(cb_b[0]), np.mean(cb_b[1]), np.mean(cb_b[2]), np.mean(cb_b[3]), np.mean(cb_b[4]), np.mean(cb_b[5]), np.mean(cb_b[6]))

print("[cb_a] max", np.max(cb_a[0]), np.max(cb_a[1]), np.max(cb_a[2]), np.max(cb_a[3]), np.max(cb_a[4]), np.max(cb_a[5]), np.max(cb_a[6]))
print("[cb_b] max", np.max(cb_b[0]), np.max(cb_b[1]), np.max(cb_b[2]), np.max(cb_b[3]), np.max(cb_b[4]), np.max(cb_b[5]), np.max(cb_b[6]))

data = [cb_a[0], cb_b[0], [], [], \
        cb_a[1], cb_b[1], [], [], \
        cb_a[2], cb_b[2], [], [], \
        cb_a[3], cb_b[3], [], [], \
        cb_a[4], cb_b[4], [], [], \
        cb_a[5], cb_b[5], [], [], \
        cb_a[6], cb_b[6]]
fig,axes=plt.subplots(nrows=1, ncols=1, figsize=(20, 8))

plt.xticks(fontsize=font_size)
plt.yticks(fontsize=font_size)
plt.grid()

bplot1 = axes.boxplot(data, vert=True, patch_artist=True)
x = 0
for patch in bplot1['boxes']:
    patch.set_facecolor(BlueList[x])
    x = 1 - x
plt.savefig("./figures/" + case_name_a + "block.png", bbox_inches='tight')
