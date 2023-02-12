#!/usr/bin/env python

import sys

import matplotlib.pyplot as plt


counter = {}

for line in sys.stdin:
    line = line[:-1] # strip \n
    rounds = int(float(line))
    count = counter.get(rounds, 0)
    count += 1
    counter[rounds] = count

fig, ax = plt.subplots()

p1 = ax.bar(counter.keys(), counter.values(), width=1, edgecolor="white", linewidth=0.7)
ax.set_ylabel('Occurences')
ax.set_xlabel('Delay (in test rounds)')

ax.bar_label(p1, label_type='center')

plt.show()
