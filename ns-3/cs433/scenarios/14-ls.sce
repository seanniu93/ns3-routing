* LS VERBOSE ALL OFF
* DV VERBOSE ALL OFF
* APP VERBOSE ALL OFF
* LS VERBOSE STATUS ON
* LS VERBOSE ERROR ON
* DV VERBOSE STATUS ON
* DV VERBOSE ERROR ON
* APP VERBOSE STATUS ON
* APP VERBOSE ERROR ON

* APP VERBOSE TRAFFIC ON
#* LS VERBOSE TRAFFIC ON
#* LS VERBOSE DEBUG ON

TIME 60000


#### Cut shortest path until all paths are cut ####

# Should be 0 -> 1 -> 4 -> 12 -> 13
0 APP PING 13 Hello
TIME 1000

# Cut shortest path
LINK DOWN 4 12
TIME 60000

# Should be 0 -> 1 -> 3 -> 8 -> 11 -> 13
0 APP PING 13 Hello
TIME 1000

# Cut shortest path
LINK DOWN 8 11
TIME 60000

# Should be 0 -> 1 -> 2 -> 5 -> [6,7] -> 9 -> 10 -> 13
0 APP PING 13 Hello
TIME 1000

NODELINKS DOWN 6
TIME 60000

0 APP PING 13 Hello
TIME 1000

NODELINKS DOWN 7
TIME 60000

0 APP PING 13 Hello
TIME 1000

#### Now that there is no path, bring up 4-12 link ####

# Uncut shortest path
LINK UP 4 12
TIME 60000

0 APP PING 13 Hello
TIME 1000

QUIT
