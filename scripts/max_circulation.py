""" function to compute the maximum circulation value in a 
given demand matrix """

from gurobipy import *

def max_circulation(demand_dict):

	m = Model()
	m.setParam('OutputFlag', 0)
	m.setParam('TimeLimit', TIME_LIMIT)

	xvar = {}

	""" create variables """
	for i, j in list(demand_dict.keys()):
		xvar[i, j] = m.addVar(vtype=GRB.CONTINUOUS, lb=0.0, ub=demand_dict[i, j], obj=1.0)

	""" add circulation constraints """
	nonzero_demand_nodes = set()
	nonzero_demand_nodes = nonzero_demand_nodes | {i for i, j in list(demand_dict.keys())}
	nonzero_demand_nodes = nonzero_demand_nodes | {j for i, j in list(demand_dict.keys())}
	nonzero_demand_nodes = list(nonzero_demand_nodes)

	for k in nonzero_demand_nodes:
		incoming = list({i for i, j in list(demand_dict.keys()) if j == k})
		outgoing = list({j for i, j in list(demand_dict.keys()) if i == k})
		expr = 0.0
		for i in incoming:
			expr += xvar[i, k]
		for j in outgoing:
			expr -= xvar[k, j]
		m.addConstr(expr == 0.0)

	""" optimize """
	m.update()
	m.setAttr("ModelSense", -1)
	m.optimize()

	""" return computed solution """
	obj = 0.0
	for i, j in list(demand_dict.keys()):
		obj += xvar[i, j].X 

	return obj

TIME_LIMIT = 120

def main():
	demand_dict = {}
	demand_dict[0, 1] = 1.0
	demand_dict[1, 2] = 1.0
	demand_dict[2, 3] = 1.0
	demand_dict[3, 0] = 1.0
	demand_dict[3, 1] = 1.0
	demand_dict[2, 0] = 1.0
	print(max_circulation(demand_dict))

if __name__=='__main__':
	main()