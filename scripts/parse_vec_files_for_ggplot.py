import os
import sys
from config import *
from parse_vec_files import *
import argparse
import numpy as np
import math

parser = argparse.ArgumentParser(prog='timeseries or relevant stats in ggplot')
parser.add_argument('--filename',
        type=str,
        help='Single vector file for a particular run using the omnet simulator')
parser.add_argument('--output-prefix',
        type=str,
        required=True,
        help='output prefix')
parser.add_argument('--scheme-list',
        nargs="*",
        help='set of schemes to aggregate results for', default=["priceSchemeWindow"])
parser.add_argument('--demand',
        type=int,
        help='Demand amount used for this experiment', default=30)
parser.add_argument('--start-time',
        type=int,
        required=True,
        help='Consider points after this second')
parser.add_argument('--credit-list',
        nargs="+",
        required=True,
        help='Credits to collect stats for')
parser.add_argument('--end-time',
        type=int,
        required=True,
        help='Consider points until this second')
parser.add_argument('--num-max',
        type=int,
        help='Single number denoting the maximum number of runs to aggregate data over', default="5")
parser.add_argument('--attr-list',
        nargs='+',
        help='List of attributes to plot')
parser.add_argument('--summary',
        action='store_true',
        help='whether to produce summary across all src dest')
parser.add_argument('--topo',
        type=str, 
        help='what topology to generate size summary for')

args = parser.parse_args()
num_paths = 4

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
                                and ((tv_val[0] >= start_time) and (tv_val[0]<= end_time))):
                                    if (src == dest or 
                                            (vec_id_map[vec_id][0] == src and vec_id_map[vec_id][3] == dest)):
                                        dataPoints.append((tv_val[0], tv_val[1], path_id))
    return dataPoints


# check if this field is actually one of the fields we want to process
def is_relevant(vec_id, vec_id_to_info_map, relevant_signal):
    vec_info = vec_id_to_info_map[vec_id]

    if vec_info[2].startswith(relevant_signal):
        return True
    return False


""" collect time inflight data across schemes and edges at different credit list"""
def aggregate_across_files(scheme_list, credit_list, topo):
    out_file = open(GGPLOT_DATA_DIR + args.output_prefix + "_token_time_inflight_data", 'w+')
    out_file.write("scheme,credit,values\n")
    for credit in credit_list:
        for scheme in scheme_list:
            path_type = "shortest" #if scheme in ["waterfilling", "DCTCPQ"] else "shortest"
            scheduling_alg = "FIFO" if scheme in ["celer"] else "LIFO"

            for run_num in range(0, args.num_max  + 1):
                file_name = topo + "_circ_net_" + str(credit) + "_" + str(scheme) + "_circ"
                file_name += str(run_num) + "_demand" + str(demand/10) + "_" + path_type
                if scheme != "shortestPath":
                    file_name += "_" + str(num_paths) 
                file_name += "_" + scheduling_alg + "-#0.vec"
                                
                try: 
                    all_timeseries, vec_id_to_info_map, parameters = parse_vec_file(RESULT_DIR + file_name, \
                            "timeInFlight")
                    
                    for vec_id, timeseries in list(all_timeseries.items()):
                        vector_details = vec_id_to_info_map[vec_id]
                        src_node = vector_details[0]
                        src_node_type = vector_details[1]
                        dest_node_type = vector_details[4]
                        dest_node = vector_details[3]
                        
                        if ("timeInFlight" not in vector_details[2]):
                            continue
                        if (src_node_type != "router" or dest_node_type != "router"):
                            continue

                        values = [t[1] for t in timeseries if t[0] > args.start_time and 
                                t[0] < args.end_time and not math.isnan(t[1])]
                        if len(values) > 0:
                            out_file.write(scheme + "," + str(credit) + "," + str(np.average(values)) + "\n")
                except IOError:
                    print("error with " , file_name)
                    continue
    out_file.close()


if __name__ == "__main__":
    demand = args.demand
    start_time = args.start_time
    end_time = args.end_time
    
    """ parse single vector file """
    if args.filename is not None:
        vec_filename = args.filename
        scheme = args.scheme_list[0]
        new_list = []
        attr_list = []
        attr_set = set()
        summary = args.summary
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
            if summary:
                out_file.write("scheme,demand,")
            else:
                out_file.write("scheme,demand,src,dest,")
            if e != "queue":
                out_file.write("path,time,value\n")
            else:
                out_file.write("time,value\n")

        if summary:
            for e in attr_set:
                parameter = e
                print(signal_map[parameter])
                node_type = "router" if parameter == "queue" else "host"
                data_points = parse_for_timeseries(vec_filename, start_time, end_time, node_type, 0, 0, \
                        signal_map[parameter])
                out_file = open(args.output_prefix + "_" + parameter + "_data", 'a+')
                for x in data_points:
                    path = x[2]
                    out_file.write(scheme + "," + str(demand) + ",")
                    if node_type == "host":
                        out_file.write("P" + str(path) + "," + str(x[0]) + "," + str(x[1]) + "\n")
                    else:
                        out_file.write(str(x[0]) + "," + str(x[1]) + "\n")
                out_file.close()
                print(max(data_points))
        else:
            for entry in attr_list:
                parameter = entry[0]
                src, dest = int(entry[1]), int(entry[2])
                print(signal_map[parameter])
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
                print(max(data_points))
                
    else:
        """ aggregate stats for a bunch of files with a common prefix and for a set of schemes """
        aggregate_across_files(args.scheme_list, args.credit_list, args.topo)
