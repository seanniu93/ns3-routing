#this is the network shown in our textbook (Kurose & Ross), page 365
#(you can check that the print out of this program matches the table at page 368)
#the only difference is that we keep track of the next hop from the beginning, which
#is better than keeping track of the previous hop like they do in the book)

#PS: "next_hop" for node i means: to which of my immediate neighbor should I forward a packet
#that has i as target?

neighbor_cost_map = {
    'u': {'v':2, 'x':1, 'w':5}, 
    'v': {'u':2, 'x':2, 'w':3}, 
    'x': {'u':1, 'v':2, 'w':3, 'y':1}, 
    'w': {'u':5, 'v':3, 'x':3, 'y':1, 'z':5},
    'y': {'x':1, 'w':1, 'z':2},
    'z': {'w':5, 'y':2}
}

neighbor_cost_map2 = {
    'A': {'B':5, 'C':10}, 
    'B': {'A':5, 'C':3, 'D':11}, 
    'C': {'A':10, 'B':3, 'D':2}, 
    'D': {'B':11, 'C':2}
}



N = len(neighbor_cost_map)

def get_neighbors(node):
    return neighbor_cost_map[node].keys()

def distance(node1, node2):
    if node1 == node2:
        return 0
    if node2 not in neighbor_cost_map[node1]:
        return 99999
    return neighbor_cost_map[node1][node2]


me = 'u'

current = me

least_costs_and_nexthop = {me: (0, None)}

print "Current node:",current

for neighbor in get_neighbors(current):
    least_costs_and_nexthop[neighbor] = (distance(current,neighbor), neighbor)
print "Least costs table:",least_costs_and_nexthop
result = {}
#add it to final_result
result[current] = least_costs_and_nexthop[current]
del least_costs_and_nexthop[current]
print "result:", result
print

for i in range(N-1):
    #pick node with least cost so far
    current = min(least_costs_and_nexthop, key=lambda x: least_costs_and_nexthop[x][0])
    print "Current node:",current
    print "Least costs table:",least_costs_and_nexthop


    #add each neighbor of current node that is not already in result to the least_costs map
    for neighbor in get_neighbors(current):
        if neighbor in result:
            continue
        if neighbor in least_costs_and_nexthop:
            old_cost = least_costs_and_nexthop[neighbor][0]
        else:
            old_cost = 9999
        new_cost = least_costs_and_nexthop[current][0] + distance(current,neighbor)
        if new_cost < old_cost:
            print "Updating cost to",neighbor,":",new_cost
            #since we are using current to reach neighbor, the next_hop should be 
            #the same one store in the entry for current
            next_hop = least_costs_and_nexthop[current][1]
            #update best cost and next hop to reach neighbor
            least_costs_and_nexthop[neighbor] = (new_cost, next_hop)

    #add it to result
    result[current] = least_costs_and_nexthop[current]
    del least_costs_and_nexthop[current]
    print "result:", result
    print



