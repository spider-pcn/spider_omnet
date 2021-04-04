""" algorithm to compute k shortest paths from given source to destination
in a graph using Yen's algorithm: https://en.wikipedia.org/wiki/Yen%27s_algorithm """

import argparse
import copy 

import cPickle as pickle 
import networkx as nx 
import operator
import numpy as np

import sys
sys.path.insert(1, '../paths')
import parse

def ksp_yen(graph, node_start, node_end, max_k=2):

    graph = copy.deepcopy(graph)

    A = []
    B = []

    try:
        path = nx.shortest_path(graph, source=node_start, target=node_end)
    except:
        print "No path found!"
        return None 

    A.append(path)    
    
    for k in range(1, max_k):
        for i in range(0, len(A[-1])-1):

            node_spur = A[-1][i]
            path_root = A[-1][:i+1]

            edges_removed = []
            for path_k in A:
                curr_path = path_k
                if len(curr_path) > i and path_root == curr_path[:i+1]:
                    if (curr_path[i], curr_path[i+1]) in graph.edges() or \
                        (curr_path[i+1], curr_path[i]) in graph.edges():
                        graph.remove_edge(curr_path[i], curr_path[i+1])
                        edges_removed.append([curr_path[i], curr_path[i+1]])
            
            nodes_removed = []
            for rootpathnode in path_root:
                if rootpathnode != node_spur:
                    graph.remove_node(rootpathnode)
                    nodes_removed.append(rootpathnode)

            try:
                path_spur = nx.shortest_path(graph, source=node_spur, target=node_end)
            except:
                path_spur = None
            
            if path_spur:
                path_total = path_root[:-1] + path_spur            
                potential_k = path_total            
                if not (potential_k in B):
                    B.append(potential_k)
            
            for node in nodes_removed:
                graph.add_node(node)

            for edge in edges_removed:
                graph.add_edge(edge[0], edge[1])
        
        if len(B):
            B.sort(key=len)
            A.append(B[0])
            B.pop(0)
        else:
            break
    
    return A

def ksp_edge_disjoint(graph, node_start, node_end, max_k=2):
    """ compute k edge disjoint shortest paths """
    graph = copy.deepcopy(graph)

    A = []

    try:
        path = nx.shortest_path(graph, source=node_start, target=node_end)
    except:
        print "No path found!"
        return None 

    A.append(path)    
    
    for k in range(1, max_k):
        prev_path = A[-1]
        for i, j in zip(prev_path[:-1], prev_path[1:]):
            if (i, j) in graph.edges() or (j, i) in graph.edges():
                graph.remove_edge(i, j)

        try:
            path = nx.shortest_path(graph, source=node_start, target=node_end)
        except:
            path = None

        if path:
            A.append(path)
                
    return A    

def kwp_edge_disjoint(graph, node_start, node_end, max_k, credit_mat):
    """ compute k edge disjoint widest paths """
    """ using http://www.cs.cmu.edu/~avrim/451f08/lectures/lect1007.pdf """

    graph = copy.deepcopy(graph)
    capacity_mat = credit_mat
    A = []

    try:
        path = nx.shortest_path(graph, source=node_start, target=node_end)
    except:
        print "No path found!"
        return None 

    for k in range(max_k):
        widthto = {}
        pathto = {}
        tree_nodes = []
        tree_neighbors = []
        tree_nodes_membership_indicator = {v: False for v in graph.nodes()}
        tree_neighbors_membership_indicator = {v: False for v in graph.nodes()}
        
        widthto[node_end] = np.inf
        pathto[node_end] = None
        tree_nodes.append(node_end)
        tree_nodes_membership_indicator[node_end] = True
        tree_neighbors = [v for v in graph.neighbors(node_end)]
        for v in graph.neighbors(node_end):
            tree_neighbors_membership_indicator[v] = True

        while tree_neighbors and (tree_nodes_membership_indicator[node_start] is False):
            x = tree_neighbors.pop(0)
            tree_neighbors_membership_indicator[x] = False
            
            max_width = -1.
            max_width_neighbor = None            
            for v in graph.neighbors(x):
                if tree_nodes_membership_indicator[v] is True:
                    if np.min([widthto[v], capacity_mat[x, v]]) > max_width:
                        max_width = np.min([widthto[v], capacity_mat[x, v]])
                        max_width_neighbor = v
                else:
                    if tree_neighbors_membership_indicator[v] is False:
                        tree_neighbors.append(v)
                        tree_neighbors_membership_indicator[v] = True

            widthto[x] = max_width
            pathto[x] = max_width_neighbor
            tree_nodes.append(x)
            tree_nodes_membership_indicator[x] = True

        if tree_nodes_membership_indicator[node_start] is True:
            node = node_start
            path = [node]
            while node != node_end:
                node = pathto[node]
                path.append(node)
            A.append(path)

        prev_path = A[-1]
        for i, j in zip(prev_path[:-1], prev_path[1:]):
            if (i, j) in graph.edges() or (j, i) in graph.edges():
                graph.remove_edge(i, j)            

    return A 

# get the best paths amongst the intermediary paths passed in based
# on bottleneck capacity / total rtt
def heuristic(intermediary_paths, capacity_mat, prop_mat, max_k=2):
    final_paths = []
    path_metric_dict = {}
    for i, path in enumerate(intermediary_paths):
        sum_rtt = 0.0
        min_capacity = capacity_mat[path[0], path[1]]
        for u,v in zip(path[:-1], path[1:]):
            sum_rtt += prop_mat[u,v]
            if capacity_mat[u,v] < min_capacity:
                min_capacity = capacity_mat[u,v]
        path_metric_dict[i] =  min_capacity / sum_rtt

    for key, _ in sorted(path_metric_dict.items(), key=operator.itemgetter(1), reverse=True):
        final_paths.append(intermediary_paths[key])
        if len(final_paths) == max_k:
            return final_paths
    return final_paths


def raeke(node_start, node_end):
    with open('./lnd_oblivious.pkl', 'rb') as input:
        paths = pickle.load(input)

    """ change node index """
    new_paths = []
    for path in paths[node_start, node_end]:
        new_path = [i-102 for i in path[1:-1]]
        new_paths.append(new_path)

    return new_paths

def main():

    parser = argparse.ArgumentParser()
    parser.add_argument('--credit_type', help='uniform or random or lnd credit on links')
    parser.add_argument('--graph_type', help='small_world or scale_free or txt or lnd or edgelist')
    parser.add_argument('--path_type', help='ksp_yen or ksp_edge_disjoint or kwp_edge_disjoint or heuristic')
    parser.add_argument('--topo_txt_file', help='filename to parse topology from', \
            default="../topology/sf_50_routers_lndCap_topo2750.txt")
    parser.add_argument('--max_num_paths', help='number of paths to consider (integer > 0)')
    args = parser.parse_args()

    n = 50
    CREDIT_AMT = 100.0
    RAND_SEED = 23
    delay = 1

    """ construct graph """
    if args.graph_type == 'scale_free':
        graph = nx.barabasi_albert_graph(n, 8, seed=23)
        graph = nx.Graph(graph)
        graph.remove_edges_from(graph.selfloop_edges())

    elif args.graph_type == 'small_world':
        graph = nx.watts_strogatz_graph(n, k=8, p=0.25, seed=23)
        graph = nx.Graph(graph)
        graph.remove_edges_from(graph.selfloop_edges())

    elif args.graph_type == 'edgelist':
        graph = nx.read_edgelist("../oblivious_routing/lnd_dec4_2018_reducedsize.edgelist")
        rename_dict = {v: int(str(v)) for v in graph.nodes()}
        graph = nx.relabel_nodes(graph, rename_dict)
        for e in graph.edges():
            graph.edges[e]['capacity'] = int(str(graph.edges[e]['capacity']))
        graph = nx.Graph(graph)
        graph.remove_edges_from(graph.selfloop_edges())
        n = nx.number_of_nodes(graph)        

    elif args.graph_type == 'txt':
        graph = parse.parse_txt_topology_file(args.topo_txt_file)
        n = nx.number_of_nodes(graph)

    else:
        print "Error! Graph type invalid."

    assert nx.is_connected(graph)

    """ construct credit matrix """
    if args.credit_type == 'uniform':
        credit_mat = np.ones([n, n])*CREDIT_AMT
        prop_mat = np.ones([n, n])*delay

    elif args.credit_type == 'random':
        np.random.seed(RAND_SEED)
        credit_mat = np.triu(np.random.rand(n, n), 1) * 2 * CREDIT_AMT
        credit_mat += credit_mat.transpose()
        credit_mat = credit_mat.astype(int)
        prop_mat = np.ones([n, n])*delay

    elif args.credit_type == 'txt' or args.credit_type == 'edgelist':
        credit_mat = np.zeros([n, n])
        prop_mat = np.zeros([n, n])
        for e in graph.edges():
            credit_mat[e[0], e[1]] = graph[e[0]][e[1]]['capacity'] 
            credit_mat[e[1], e[0]] = graph[e[1]][e[0]]['capacity']
            prop_mat[e[0], e[1]] = graph[e[0]][e[1]]['hop_delay'] 
            prop_mat[e[1], e[0]] = graph[e[1]][e[0]]['hop_delay']

    else:
        print "Error! Credit matrix type invalid."

    """ get paths and store in dict """
    paths = {}
    for i in range(n):
        for j in range(n):
            if i != j:
                if args.path_type == 'ksp_yen':
                    ret_paths = ksp_yen(graph, i, j, int(args.max_num_paths))
                elif args.path_type == 'ksp_edge_disjoint':
                    ret_paths = ksp_edge_disjoint(graph, i, j, int(args.max_num_paths))
                elif args.path_type == 'kwp_edge_disjoint':
                    ret_paths = kwp_edge_disjoint(graph, i, j, int(args.max_num_paths), credit_mat)
                elif args.path_type == 'heuristic':
                    intermediary_paths = ksp_yen(graph, i, j, 10000)
                    print "found", len(intermediary_paths), "between", i, "and", j
                    ret_paths = heuristic(intermediary_paths, credit_mat, prop_mat, int(args.max_num_paths))
                else:
                    print "Error! Path type invalid."

                new_paths = []
                for ret_path in ret_paths: 
                    new_path = []
                    new_path.append(i)
                    new_path = new_path + [u + n for u in ret_path]
                    new_path.append(j)
                    new_paths.append(new_path)

                paths[i, j] = new_paths

    print paths

    with open('./paths/' + args.graph_type + '_' + args.path_type + '.pkl', 'wb') as output:
        pickle.dump(paths, output, pickle.HIGHEST_PROTOCOL)

if __name__=='__main__':
    main()
