import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import re

BlueList = ['#92B4F4', '#7772CA', '#5E7CE2', '#C6CDFF', '#248BD6', '#0F6BAE']

def get_time(case_name):
    logdata = []
    

    thread_id = []
    time_list = [0,0,0,0,0]

    # Load File
    fileHandler = open("./logs/" + case_name + ".log", "r")
    listOfLines = fileHandler.readlines()
    fileHandler.close()

    # Process Raw Data
    for line in listOfLines:
        number = re.findall("\d+", line)
        bgn_bool = True if len(re.findall("Bgn", line)) > 0 else False
        end_bool = True if len(re.findall("End", line)) > 0 else False
        is_timer = True if len(re.findall("Timer_callback", line)) > 0 else False
        if bgn_bool:
            # Create the item
            name = "Timer" + number[0] if is_timer else "Cb_" + number[0]
            color_mark = -1 if is_timer else (int(number[0]) % 10) - 1
            time = float(number[-2]) + float(number[-1]) / 1000000
            item = {
                "name":name,
                "color": color_mark,
                "thread":int(number[-3]),
                "bgn":time,
                "end":-1
            }
            logdata.append(item)
            flag = True
            for tid in thread_id:
                if tid == int(number[-3]):
                    flag = False
                    break
            if (flag):
                thread_id.append(int(number[-3]))
        elif end_bool:
            # Match the item
            name = "Timer" + number[0] if is_timer else "Cb_" + number[0]
            time = float(number[-2]) + float(number[-1]) / 1000000
            for item in logdata:
                if item["name"] == name and item["thread"] == int(number[-3]) and item["end"] == -1:
                    item["end"] = time
                    break
    for item in logdata:
        targ_tid = item["thread"]
        id = 0
        for tid in thread_id:
            if targ_tid == tid: break
            else: id += 1
        if item["end"] > 0:
            time_list[id] += item["end"] - item["bgn"]
    return time_list

time_1 = get_time("exp1/co_t_mexe_f_utilize")
time_2 = get_time("exp1/co_f_mexe_f_utilize")

new_time_1 = [x/200 for x in time_1]
new_time_2 = [x/200 for x in time_2]

fig, axes = plt.subplots(1, 1, figsize=(20,8))
font_size = 28
plt.xticks(fontsize=font_size)
plt.yticks(fontsize=font_size)
plt.grid()
plt.ylabel("Utilization", fontsize=font_size+4)

labels = ["Thread 1", "Thread 2", "Thread 3", "Thread 4", "Thread 5"]
plt.xticks([1.5, 4.5, 7.5, 10.5, 13.5], labels=labels, size=font_size)

pos1 = [1,4,7,10,13]
pos2 = [2,5,8,11,14]

bar1 = axes.bar(pos1, new_time_1, edgecolor='black', color=BlueList[0], label='RoboSpike')
bar2 = axes.bar(pos2, new_time_2, edgecolor='black', color=BlueList[1], label='ROS 2')

for i in range(5):
    plt.text(pos1[i], new_time_1[i], '{:.3f}'.format(new_time_1[i]), va="bottom", ha="center", fontsize=font_size)
    plt.text(pos2[i], new_time_2[i], '{:.3f}'.format(new_time_2[i]), va="bottom", ha="center", fontsize=font_size)

axes.spines['top'].set_visible(False)
axes.spines['right'].set_visible(False)

plt.legend(loc=4, prop={'size': font_size})
plt.savefig("./figures/ana/exp2.png", bbox_inches='tight')