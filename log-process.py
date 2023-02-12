#!/usr/bin/env python

import re
import sys
import argparse


keywords = ['FALSE_NEGATIVE', 'Terminating']

def main():
    parser = get_parser()
    args = parser.parse_args()

    test_period = args.test_period
    echo = args.echo
    max_time = args.max_time
    termination_delay = {}

    lines = (line.strip('\n') for line in sys.stdin)
    lines = list(filter(is_not_noise, lines))

    if echo:
        for line in lines:
            print(line)

    for i, line in enumerate(lines):
        if 'FALSE_NEGATIVE' in line:
            pid = get_suspected_process(line)
            if pid not in termination_delay:
                (time_delta, terminated) = find_termination_time(pid, i, lines, max_time)
                rounds = time_delta / test_period
                termination_delay[pid] = (rounds, terminated)

    terminated = {pid: count for pid, count in termination_delay.items()}
    for (delay, _) in terminated.values():
        print(delay)



def get_parser():
    parser = argparse.ArgumentParser(
                    prog = 'VCube Log Prcessor',
                    description = 'Analyzes output logs from VCube simulator and counts the number of rounds from when a process is first suspected until it gets terminated')
    parser.add_argument('test_period', help='Period at which test rounds are run', type=float)
    parser.add_argument('max_time', help='Time for which simmulation was run', type=float)
    parser.add_argument('-e', '--echo', action='store_true', help='Echo stdin')
    return parser


def is_not_noise(log):
    for keyword in keywords:
        if keyword in log:
            return True
    return False

def get_suspected_process(line):
    match = re.search(r'\d+ -> (\d+)', line)
    return int(match.group(1))

def get_time(line):
    return float(line.split(':')[0])


def find_termination_time(pid, idx, lines, max_time) -> (float, bool):
    """
    Find delta between when a process was first suspected and when it was terminated.
    Return tuple with time delta and whether process was terminated before the simmulation ended or not.

    pid: process id
    idx: idx in log
    lines: log lines
    max_time: termination time of simmulation
    """
    start = get_time(lines[idx])
    for line in lines[idx+1:]:
        if f'Process {pid} suspected' in line:
            time = get_time(line)
            return (time - start, True)
    return (max_time - start, False)
        

if __name__ == '__main__':
    main()
