import os
import sys
from config import *
from parse_vec_files import *
import argparse

parser = argparse.ArgumentParser(prog='timeseries plot')
parser.add_argument('--filename',
        type=str,
        required=True,
        help='Single vector file for a particular run using the omnet simulator')
parser.add_argument('--output-prefix',
        type=str,
        required=True,
        help='output prefix')
parser.add_argument('--scheme',
        type=str,
        required=True,
        help='Scheme that you ran with')
parser.add_argument('--demand',
        type=int,
        required=True,
        help='Demand amount used for this experiment')
parser.add_argument('--start-time',
        type=int,
        required=True,
        help='Consider points after this second')
parser.add_argument('--end-time',
        type=int,
        required=True,
        help='Consider points until this second')
parser.add_argument('--attr-list',
        nargs='+',
        help='List of attributes to plot')
args = parser.parse_args()

signal_map={"queue": "numInQueue", "inflight": "sumOfTransUnitsInFlight", "rate": "rateToSendTransPerDest"}


# parse the entire file starting from the declarations making a map of them
# and then parsing the data lines one by one
# everytime store the data in a map, so its easy to extract all the useful signals at the end in one shot
def parse_for_timeseries(filename, start_time, end_time, node_type, src, dest, relevant_signal):
    vec_id_map = dict()
    last_vec_id = -1 
    dataPoints = list()
    parameters = ""

    with open(filename) as f:
        for line in f:
            if line.startswith("param"):
                parameters += (line.split("**")[1])
            if line.startswith("vector"):
                vec_id, vec_info = parse_vec_declaration(line)
                vec_id_map[vec_id] = vec_info

            if line[0].isdigit():
                columns = line.split("\t")
                if len(columns) == 4:
                    cur_vec_id = columns[0]
                    signal_name = vec_id_map[int(columns[0])][2]
                    if (node_type == "host" and "_" in signal_name and "Total" not in signal_name):
                        path_id = int(signal_name.split("_")[1])
                    else: 
                        path_id = 0
                    if (cur_vec_id != last_vec_id and is_relevant(int(columns[0]), vec_id_map, relevant_signal)):
                        cur_vec_id = last_vec_id
                    if cur_vec_id == last_vec_id:
                        data = parse_vec_data_line(columns)
                        if data[0] != None:
                            tv_val = data[1]
                            vec_id = data[0]
                            if((vec_id_map[vec_id][1] == node_type) and (vec_id_map[vec_id][4] == node_type)
                                    and (vec_id_map[vec_id][0] == src and vec_id_map[vec_id][3] == dest)):
                                if ((tv_val[0] >= start_time) and (tv_val[0]<= end_time)):
                                    dataPoints.append((tv_val[0], tv_val[1], path_id))
    return dataPoints


# check if this field is actually one of the fields we want to process
def is_relevant(vec_id, vec_id_to_info_map, relevant_signal):
    vec_info = vec_id_to_info_map[vec_id]

    if vec_info[2].startswith(relevant_signal):
        return True
    return False

if __name__ == "__main__":
    vec_filename = args.filename
    demand = args.demand
    scheme = args.scheme
    start_time = args.start_time
    end_time = args.end_time
    new_list = []
    attr_list = []
    attr_set = set()
    for i, a in enumerate(args.attr_list):
        new_list.append(a)
        if (i + 1)  % 3 == 0:
            attr_list.append(new_list)
            new_list = []
        if (i % 3) == 0:
            attr_set.add(a)

    for e in attr_set:
        os.system('rm ' + args.output_prefix + '_' + e + "_data")
        out_file = open(args.output_prefix + "_" + e + "_data", 'w+')
        out_file.write("scheme,demand,src,dest,")
        if e != "queue":
            out_file.write("path,time,value\n")
        else:
            out_file.write("time,value\n")

    for entry in attr_list:
        parameter = entry[0]
        src, dest = int(entry[1]), int(entry[2])
        print signal_map[parameter]
        node_type = "router" if parameter == "queue" else "host"
        data_points = parse_for_timeseries(vec_filename, start_time, end_time, node_type, src, dest, \
                signal_map[parameter])
        out_file = open(args.output_prefix + "_" + parameter + "_data", 'a+')
        for x in data_points:
            path = x[2]
            out_file.write(scheme + "," + str(demand) + "," + str(src) + "," + str(dest) + ",")
            if node_type == "host":
                out_file.write("P" + str(path) + "," + str(x[0]) + "," + str(x[1]) + "\n")
            else:
                out_file.write(str(x[0]) + "," + str(x[1]) + "\n")
        out_file.close()
        print max(data_points)
