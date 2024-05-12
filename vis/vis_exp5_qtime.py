import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import re
from queue import Queue

# Set Configs
pgo_case_name = "co_t_mexe_f_wcet"
nmax = 1
font_size = 28
logdata = []
BlueList = ['#92B4F4', '#7772CA', '#5E7CE2', '#C6CDFF', '#248BD6', '#0F6BAE']
RedList  = ['#FFB3A7', '#8C4356', '#9D2933']
vis_callback_only = False

# Load File
time_list = []
label_list = []
base_data = 0

fileHandler = open("./logs/exp1/" + pgo_case_name + ".log", "r")
listOfLines = fileHandler.readlines()
fileHandler.close()
logdata = []
queue = Queue()

for line in listOfLines:
    
    number = re.findall("\d+", line)
    addr = re.findall(r'0x[0-9A-F]+', line, re.I)
    
    reg_cb_bool = True if len(re.findall("Regular_callback", line)) > 0 else False
    push_bool = True if len(re.findall("Push", line)) > 0 else False
    pop_bool = True if len(re.findall("Pop", line)) > 0 else False
    coroutine_queue_bool = True if len(re.findall("coroutine_queue", line)) > 0 else False
    
    if coroutine_queue_bool and push_bool:
        # Create the item
        ptr = addr[0]
        time = float(number[-2]) + float(number[-1]) / 1000000
        item = {
            "ptr": addr[0],
            "tid": -1,
            "cbk": -1,
            "bgn": time,
            "end": -1
        }
        logdata.append(item)
    elif coroutine_queue_bool and pop_bool:
        # Match the item
        time = float(number[-2]) + float(number[-1]) / 1000000
        for i in range(len(logdata)):
            item = logdata[i]
            if item["ptr"] == addr[0] and item["end"] == -1:
                item["end"] = time
                time_list.append(item["end"] - item["bgn"])
                item["tid"] = int(number[0])
                queue.put([item["tid"], i]) # Push
    elif reg_cb_bool:
        if queue.empty() == False:
            if queue.queue[0][0] == int(number[1]) and logdata[queue.queue[0][1]]["cbk"] < 0:
                logdata[queue.queue[0][1]]["cbk"] = int(number[0])
                k = queue.get() # Pop

coroutine_len = []
base_data = 0

# Process Raw Data
fileHandler = open("./logs/exp1/" + pgo_case_name + ".log", "r")
listOfLines = fileHandler.readlines()
fileHandler.close()
for line in listOfLines:
    number = re.findall("\d+", line)
    if len(re.findall("get_rest_coroutine", line)) > 0:
        coroutine_len.append(float(number[-1]))

for item in logdata:
    print(item)

fig, axes = plt.subplots(1, 1, figsize=(10,8))
iters=list(range(min(len(coroutine_len), len(time_list))))

star = []
star_pos = []

data = [[], [], []]

for item in logdata:
    if item['cbk'] == 11:
        data[0].append(item['end'] - item['bgn'])
    if item['cbk'] == 22:
        data[1].append(item['end'] - item['bgn'])
    if item['cbk'] == 31:
        data[2].append(item['end'] - item['bgn'])
        
for i in iters:
    if coroutine_len[i] > 1:
        star_pos.append(i)
        star.append(time_list[i])

plt.grid()
plt.plot(list(range(len(data[0]))), data[0], 'o', color=BlueList[0], label="$cb\_11$", linewidth=3.5)
plt.plot(list(range(len(data[0]), len(data[1])+len(data[0]))), data[1], 'o', color=BlueList[1], label="$cb\_22$", linewidth=3.5)
plt.plot(list(range(len(data[1])+len(data[0]), len(data[1])+len(data[0])+len(data[2]))), data[2], 'o', color=BlueList[2], label="$cb\_33$", linewidth=3.5)

# plt.plot(star_pos, star, '*', color=RedList[1])

plt.legend(fontsize=18)
plt.xticks(fontsize=font_size)
plt.yticks(fontsize=font_size)

plt.ylabel("$Q^{i}$ / s", fontsize=font_size)
plt.savefig("./figures/ana/exp5_queue.png", bbox_inches='tight')