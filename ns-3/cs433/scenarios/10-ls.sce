* LS VERBOSE ALL OFF
* DV VERBOSE ALL OFF
* APP VERBOSE ALL OFF
* LS VERBOSE STATUS ON
* LS VERBOSE ERROR ON
* DV VERBOSE STATUS ON
* DV VERBOSE ERROR ON
* APP VERBOSE STATUS ON
* APP VERBOSE ERROR ON
* LS VERBOSE TRAFFIC ON
* APP VERBOSE TRAFFIC ON

# Advance Time pointer by 60 seconds. Allow the routing protocol to stabilize.
TIME 60000

# Bring down Link Number 6.
LINK DOWN 6
TIME 10

# Bring up Link Number 6.
LINK UP 6
TIME 10

# Bring down all links of node 1
NODELINKS DOWN 1
TIME 10

# Bring up all links of node 1
NODELINKS UP 1
TIME 10

# Bring down link(s) between nodes 1 and 8
LINK DOWN 1 8
TIME 10

# Bring up link(s) between nodes 1 and 8
LINK UP 1 8
TIME 10

# Dump Link State Neighbor Table.
1 LS DUMP NEIGHBORS

# Dump Link State Routing Table.
1 LS DUMP ROUTES

# Send application level PING from node 1 to node 8. Note that Application level PINGs are unicast packets and supposed to traverse multiple hops.
1 LS PING 8 Hello

TIME 4000

# Quit the simulator. Commented for now.
#QUIT
