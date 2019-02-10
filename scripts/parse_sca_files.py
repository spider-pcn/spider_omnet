import sys
import shlex

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
    for src_dst_pair, stat in data.items():
        for s in stat:
            stat_type = s[0]
            value = float(s[1])

            if stat_type == "attempted":
                sum_attempted += value
            elif stat_type == "completed":
                sum_completed += value
            else:
                sum_arrived += value

    return parameters, sum_completed/sum_arrived, sum_completed/sum_attempted






