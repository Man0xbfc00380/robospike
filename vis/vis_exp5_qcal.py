import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import re
from queue import Queue
import copy

# Set Configs
pgo_case_name = "co_t_mexe_f_wcet"
nmax = 1
font_size = 28
logdata = []
BlueList = ['#92B4F4', '#7772CA', '#5E7CE2', '#C6CDFF', '#248BD6', '#0F6BAE']
RedList  = ['#FFB3A7', '#8C4356', '#9D2933']

# # Chain  1--   2--   3--
# timer = [-1, -1, -1] 
# # Cb    1------------------------------------------------------------  2-------------------  3-------
# cback = [[[-1, -1], [-1, -1], [-1, -1], [-1, -1], [-1, -1], [-1, -1], [-1, -1]], [[-1, -1], [-1, -1]], [[-1, -1]]]

# # case a
# fileHandler = open("./logs/exp1/" + pgo_case_name + ".log", "r")
# listOfLines = fileHandler.readlines()
# fileHandler.close()
# logdata = []

# # Process Raw Data
# for line in listOfLines:
#     number = re.findall("\d+", line)
#     bgn_bool = True if len(re.findall("Bgn", line)) > 0 else False
#     end_bool = True if len(re.findall("End", line)) > 0 else False
#     is_timer = True if len(re.findall("Timer_callback", line)) > 0 else False
#     is_subcalback = True if len(re.findall("[*]", line)) > 0 else False
#     if bgn_bool:
#         # Create the item
#         name = "Timer" + number[0] if is_timer else "Cb_" + number[0]
#         color_mark = -1 if is_timer else (int(number[0]) % 10) - 1
#         time = float(number[-2]) + float(number[-1]) / 1000000
#         item = {
#             "name":name,
#             "is_timer": is_timer,
#             "color": color_mark,
#             "thread":number[-3],
#             "is_sub":is_subcalback,
#             "bgn":time,
#             "end":-1
#         }
#         logdata.append(item)
#     elif end_bool:
#         # Match the item
#         name = "Timer" + number[0] if is_timer else "Cb_" + number[0]
#         time = float(number[-2]) + float(number[-1]) / 1000000
#         for item in logdata:
#             if item["name"] == name and item["thread"] == number[-3] and item["end"] == -1:
#                 item["end"] = time

# for item in logdata:
#     if item["end"] > 0:
#         name = item["name"]
#         if item["is_timer"]:
#             if item["end"] - item["bgn"] > timer[int(name[-1]) - 1]:
#                 timer[int(name[-1]) - 1] = item["end"] - item["bgn"]
#         elif item["is_sub"]:
#             if item["end"] - item["bgn"] > cback[int(name[-2]) - 1][int(name[-1]) - 1][1]:
#                 cback[int(name[-2]) - 1][int(name[-1]) - 1][1] = item["end"] - item["bgn"]
#         else:
#             if item["end"] - item["bgn"] > cback[int(name[-2]) - 1][int(name[-1]) - 1][0]:
#                 cback[int(name[-2]) - 1][int(name[-1]) - 1][0] = item["end"] - item["bgn"]

# print(timer)
# print(cback)

# Certain Case
q0_cb11 = 0.7848690000000147
q0_cb22 = 0.8166330000000004
q0_cb31 = 0.7848690000000147

qi_cb11 = 0.8166330000000004
qi_cb22 = 3.032578000000001
qi_cb31 = 0.8166330000000004

fig, axes = plt.subplots(1, 1, figsize=(5,8))

plt.grid()

pos1 = [1,4,7]
pos2 = [2,5,8]
data1 = [q0_cb11, q0_cb22, q0_cb31]
data2 = [qi_cb11, qi_cb22, qi_cb31]
bar1 = axes.bar(pos1, data1, edgecolor='black', color=BlueList[0], label='$Q^{(0)}$')
bar2 = axes.bar(pos2, data2, edgecolor='black', color=BlueList[1], label='$Q^{(i)}$')

for i in range(3):
    plt.text(pos1[i], data1[i], ' {:.3f}'.format(data1[i]), va="bottom", ha="center", fontsize=font_size-4, rotation=90)
    plt.text(pos2[i], data2[i], ' {:.3f}'.format(data2[i]), va="bottom", ha="center", fontsize=font_size-4, rotation=90)

axes.spines['top'].set_visible(False)
axes.spines['right'].set_visible(False)
plt.xticks(fontsize=font_size)
plt.yticks(fontsize=font_size)
plt.ylabel("Time / s", fontsize=font_size+4)
labels = ["Cb_11", "Cb_22", "Cb_31"]
plt.xticks([1.5, 4.5, 7.5], labels=labels, size=font_size-8)

plt.legend(loc=2, prop={'size': font_size-8})
plt.savefig("./figures/ana/exp5_cal.png", bbox_inches='tight')
