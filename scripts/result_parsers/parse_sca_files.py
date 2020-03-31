import sys
import shlex
import numpy as np

# parse a line in the scalar file that starts with scalar
def parse_sca_parameter_line(line):
    data = shlex.split(line)
    scalar = data[len(data) - 2]
    value = data[len(data) - 1]
    return scalar + " " + value + "\n"



# parse one line of statistic and update the 
def parse_sca_stat_line(line):
    info = shlex.split(line)
    sender = int(info[1].split('[')[1].split(']')[0])

    signal_type = info[2]
    if "rateArrivedPerDest_Total" in signal_type:
        stat_type = "arrived"
    elif "rateAttemptedPerDest_Total" in signal_type:
        stat_type = "attempted"
    elif "rateCompletedPerDest_Total" in signal_type:
        stat_type = "completed"
    else:
        stat_type = "irrelevant"
        return sender, 0, stat_type

    receiver = int(signal_type.split(" ")[2].split(")")[0])
    return sender, receiver, stat_type

# parse the scalar file in its entirety
# and return the overall success rate among the total arrived and the total attempted
def parse_sca_files(filename):
    parameters = ""
    data = dict()
    with open(filename) as f:
        line = f.readline()
        while line:
            if line.startswith("scalar"):
                if not("queueSize" in line or "completionTime" in line or "amt" in line or "rate" in line or \
                        "Rebalancing" in line or "Amt" in line or "size" in line or "time " in line):
                    parameters += parse_sca_parameter_line(line)

            elif line.startswith("statistic"):
                sender, receiver, stat_type = parse_sca_stat_line(line)
                for i in range(7):
                    line = f.readline()
                    relevant = (stat_type != "irrelevant")
                    if i == 5 and relevant and line.startswith("field"):
                        # field line with sum, remove new line part
                        sum_value = line.split(" ")[2][:-1]
                        cur_data = data.get((sender, receiver), [])
                        cur_data.append((stat_type, sum_value))
                        data[(sender, receiver)] = cur_data    
                    else:
                        continue
            
            line = f.readline()

    # compute completion as a fraction of arrival and attempte
    sum_attempted = 0.0
    sum_arrived = 0.0
    sum_completed = 0.0
    for src_dst_pair, stat in list(data.items()):
        for s in stat:
            stat_type = s[0]
            value = float(s[1])

            if stat_type == "attempted":
                sum_attempted += value
            elif stat_type == "completed":
                sum_completed += value
            else:
                sum_arrived += value

    return parameters, sum_completed/max(sum_arrived, 1.0), sum_completed/max(sum_attempted, 1.0)

def parse_overall_stat_line(line):
    data = shlex.split(line)
    scalar_name = data[len(data) - 2]
    parts = scalar_name.split()
    stat_type = parts[0]
    sender = int(parts[1])
    receiver = int(parts[3])
    value = data[len(data) - 1]
    return sender, receiver, stat_type, value

def parse_simple_stat_line(line):
    data = shlex.split(line)
    scalar_name = data[len(data) - 2]
    parts = scalar_name.split()
    stat_type = parts[0]
    sender = int(parts[-1])
    value = data[len(data) - 1]
    return sender, stat_type, value


def parse_sca_files_overall(filename):
    parameters = ""
    stats = dict()
    stats_3000 = dict()
    amt_added, num_rebalancing = 0, 0
    num_retries, comp_times = [], []
    num_rebalancing_list, amt_rebalanced_list = [], []
    
    with open(filename) as f:
        line = f.readline()
        flag = False
        while line:
            if line.startswith("scalar") and "->" in line:
                #print line, flag
                sender, receiver, stat_name, value = parse_overall_stat_line(line)
                if flag:
                    temp = stats_3000.get((sender, receiver), [])
                    temp.append((stat_name, value))
                    stats_3000[sender, receiver] = temp
                else:
                    temp = stats.get((sender, receiver), [])
                    temp.append((stat_name, value))
                    stats[sender, receiver] = temp
                if "completionTime" in line:
                    flag = False
            elif line.startswith("scalar") and "totalNumRebalancingEvents" in line:
                sender, receiver, stat_name, value = parse_overall_stat_line(line)
                num_rebalancing_list.append(float(value))
                num_rebalancing += float(value)
            elif line.startswith("scalar") and "retries" in line:
                sender, stat_name, value = parse_simple_stat_line(line)
                num_retries.append(float(value))
            elif line.startswith("scalar") and "completion times" in line:
                sender, stat_name, value = parse_simple_stat_line(line)
                comp_times.append(float(value))
            elif line.startswith("scalar") and "totalAmtAdded" in line:
                sender, receiver, stat_name, value = parse_overall_stat_line(line)
                amt_rebalanced_list.append(float(value))
                amt_added += float(value)

            elif "time 3000" in line:
                flag = True
            else:
                flag = False
            line = f.readline()

    # compute completion as a fraction of arrival and attempte
    vol_attempted, num_attempted = 0.0, 0.0
    vol_arrived, num_arrived = 0.0, 0.0
    vol_completed, num_completed = 0.0, 0.0
    completion_time = 0.0

    # clean tail compl times and tries
    comp_times = np.array(comp_times)
    comp_times = comp_times[comp_times != 0]
    num_retries = np.array(num_retries)
    num_retries = num_retries[num_retries != 0]

    for src_dst_pair, stat in list(stats_3000.items()):
        for s in stat:
            stat_type = s[0]
            value = float(s[1])

            if stat_type == "rateAttempted":
                num_attempted += value
            elif stat_type == "amtAttempted":
                vol_attempted += value
            elif stat_type == "rateCompleted":
                num_completed += value
            elif stat_type == "amtArrived":
                vol_arrived += value
            elif stat_type == "amtCompleted":
                vol_completed += value
            elif stat_type == "rateArrived":
                num_arrived += value
            else:
                completion_time += value

    
    if num_arrived > 0 and vol_arrived > 0:
        print("Stats for first 3000 seconds of  ", filename)
        print(" Success ratio over arrived: ", num_completed/num_arrived, " over attempted", \
                num_completed/num_attempted)
        print(" Success volume  over arrived: ", vol_completed/vol_arrived, \
                " over attempted", vol_completed/vol_attempted)
        print(" Avg completion time ", completion_time/num_completed)
        print("Success Rate " + str(num_completed/1000.0))


    vol_attempted, num_attempted = 0.0, 0.0
    vol_arrived, num_arrived = 0.0, 0.0
    vol_completed, num_completed = 0.0, 0.0
    completion_time = 0.0
    str_to_add = ""
    
    for src_dst_pair, stat in list(stats.items()):
        for s in stat:
            stat_type = s[0]
            value = float(s[1])

            if stat_type == "rateAttempted":
                num_attempted += value
            elif stat_type == "amtAttempted":
                vol_attempted += value
            elif stat_type == "rateCompleted":
                num_completed += value
            elif stat_type == "amtArrived":
                vol_arrived += value
            elif stat_type == "amtCompleted":
                vol_completed += value
            elif stat_type == "rateArrived":
                num_arrived += value
            else:
                completion_time += value

    print("Stats for last part of", filename)
    print(" Success ratio over arrived: ", num_completed/num_arrived, " over attempted", num_completed/num_attempted)
    print(" Success volume  over arrived: ", vol_completed/vol_arrived, \
            " over attempted", vol_completed/vol_attempted)
    print(" Avg completion time ", completion_time/num_completed)
    print("Success Rate " + str(num_completed/1000.0))
    print("Amt rebalanced " + str(amt_added)) 
    print("Num rebalanced " + str(num_rebalancing))
    print("Num arrived " + str(num_arrived)) 
    print("Num completed " + str(num_completed)) 

    if len(num_retries) > 0:
        str_to_add = "\nNum retries percentile (99.9) " + str(np.percentile(num_retries, 90))
        print(str_to_add)


    stats_str = "Stats for " + filename + "\nSuccess ratio over arrived: " + str(num_completed/num_arrived) +\
            " over attempted" + str(num_completed/num_attempted) + \
            "\nSuccess volume  over arrived: " + str(vol_completed/vol_arrived) + \
            " over attempted" + str(vol_completed/vol_attempted) + \
            "\nAvg completion time " + str(completion_time/max(num_completed, 1.0)) + \
            "\nSuccess Rate " + str(vol_completed/1000.0) + \
            "\nAmt rebalanced " + str(amt_added) + \
            "\nNum rebalanced " + str(num_rebalancing) + \
            "\nNum arrived " + str(num_arrived) + \
            "\nNum completed " + str(num_completed) + \
            str_to_add 

    return (num_rebalancing_list, amt_rebalanced_list, stats_str) 
