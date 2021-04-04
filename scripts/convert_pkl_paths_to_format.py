import pickle
from config import *
import collections

OP_FILE_PATH="../benchmarks/circulations/"

topo_filelist = ['sw_50_routers_lndCap_topo4000', 'sf_50_routers_lndCap_topo4000', 'lnd_july15_2019_topo40'] 
file_prefix = ['sw_50_routers_lndCap_topo4000', 'sf_50_routers_lndCap_topo4000', 'lnd_july15_2019_topo40'] 
path_type_list = ['ksp_yen', 'oblivious', 'ksp_edge_disjoint', 'heuristic', 'kwp_edge_disjoint']
op_suffix_list = ['_kspYenPaths', '_obliviousPaths', '_edgeDisjointPaths', '_heuristicPaths', '_widestPaths']


for topo, op_prefix in zip(topo_filelist, file_prefix):
    for path_type, op_suffix in zip(path_type_list, op_suffix_list):
        num_paths = []
        filename = topo + "_" + path_type
        pkl_file = open(PATH_PKL_DATA + filename + ".pkl", 'rb')
        paths = pickle.load(pkl_file)
        pkl_file.close()

        output_filename = OP_FILE_PATH + op_prefix + ".txt" + op_suffix
        output_file = open(output_filename, "w+")

        for (sender, receiver) in list(paths.keys()):
            output_file.write("pair " + str(sender) + " " + str(receiver) + "\n")
            num_paths.append(len(paths[(sender, receiver)]))
            for path in paths[(sender, receiver)]:
                path_str = [str(p) for p in path]
                output_file.write(" ".join(path_str) + "\n")

        ctr = collections.Counter(num_paths)
        print(filename, ctr)
        output_file.close()
