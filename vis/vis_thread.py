import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import re

# Set Configs
case_name = "exp1/exp1_multicast_450_t16n4" # Just change it
font_size = 18
logdata = []
BlueList = ['#92B4F4', '#7772CA', '#5E7CE2', '#C6CDFF', '#248BD6', '#0F6BAE']
RedList  = ['#FFB3A7', '#8C4356', '#9D2933']
vis_callback_only = False

# Load File
fileHandler = open("./logs/" + case_name + ".log", "r")
listOfLines = fileHandler.readlines()
fileHandler.close()

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
                break
print(logdata)
# Draw Figures
if vis_callback_only:
    fig, axes = plt.subplots(1, 1, figsize=(16,12))
else:
    fig, axes = plt.subplots(2, 1, figsize=(16,12))

plt.xticks(fontsize=font_size)
plt.yticks(fontsize=font_size)

for item in logdata:
    if item["end"] > 0 and item["end"] < 10:
        if vis_callback_only:
            # y = callback
            axes.barh(y=item["name"], width=item["end"]-item["bgn"], left=item["bgn"], edgecolor='grey', color=BlueList[item["color"]] if item["color"] >= 0 else RedList[-1 * item["color"]])
        else:
            # y = thread
            axes[0].barh(y=item["thread"], width=item["end"]-item["bgn"], left=item["bgn"], edgecolor='grey', color=BlueList[item["color"]] if item["color"] >= 0 else RedList[-1 * item["color"]])
            # y = callback
            axes[1].barh(y=item["name"], width=item["end"]-item["bgn"], left=item["bgn"], edgecolor='grey', color=BlueList[item["color"]] if item["color"] >= 0 else RedList[-1 * item["color"]])

# Save File
plt.savefig("./figures/" + case_name + ".png")