# CS533: Link State and Distance Vector Routing #

#### Luciano Dyballa, Sean Niu, Michael McMillan

In this project, we implemented two routing protocols in the ns-3 framework.
Some of the most important and essential design decisions we made include 
the following:

### Neighbor Discovery ###
1) Each node periodically sends out a hello packet using the SendHello()
   function.  Upon receiving the hello message, a ProcessHelloRequest() event
   is triggered, then the program checks if we have seen this message, and if not,
   inserts the neighbor into the neighbor table, runs the Dijkstra() function,
   and broadcasts an LSTableMessage that contains the table information
   the node needs to share with the network.

   This hello message has no reply because we can gather all
   the information we need about our neighbors from that single packet.
   We followed RFC 2328 on OSPF, which indicates that Hello replies are
   not necessary in general.

2) The neighbor table is a map from Node numbers (std::string) to 
   NeighborTableEntry structs, which consist of address and interface information.
   The routing table is a map from Node numbers (std::string) to
   RoutingTableEntry structs, which contain all of the interface and destination
   info.

3) The AuditHellos() function automatically sends a new Hello message, checks 
   that the node received a hello packet from all of its neighbors recently and,
   if a hello was not received, removes the associated node from the Neighbor table.
   In this case, it also calls Dijkstra() and sends an LSTableMessage() with the
   new table info.

### Link State ###
1) We monitored the sequence numbers in the LSTableEntry struct in order to
   keep track of the the most recent message seen so we could prevent messages
   from needlessly flooding the network or getting caught in infinite loops.
   This way, we keep the number of packets in the network at any one time to a minimum.
   Each node broadcasts a LinkState packet each time its neighbor list
   changes---upon receiving a Hello message or when auditing lost Hello messages
   (AuditHellos()) that indicate a down link. 

2) Our primary data structures were a neighbor table (the address and interface info
   about each neighbor), a routing table (with forwarding info gathered with Dijsktra's
   algorithm), and the LSTable, which contained the neighbor costs and sequence numbers
   also used in conjunction with Dijsktra.

3) Forwarding required a relatively small amount of code, but team members spent
   a lot of time investigating the details of the fields that needed to be set.
   For RouteInput and RouteOutput, we had to create an ipv4Route object with
   several fields set to the values we collected from Dijsktra's algorithm.

### Distance Vector ###
1) The team tried two approaches to the Count to Infinity problem.  In the 
   first, we impose an upper bound on the number of hops to a destination.  For this 
   solution (recommended in the assignment specification), if the cost exceeds 16,
   the program does not call SendDVTableMessage() and does not change the information
   an the routing table.  See the details in the Testing section below for a specific
   example.

   In the second, we implemented a partially working version of Poisoned Reverse that
   can be found in a separate branch on the Github repository. For each DV message sent 
   out, the code checks that certain values are set to infinity to prevent improper 
   routing. As part of our effort, we wrote a SendPacket() function that would allow 
   us to handle each neighbor separately.

2) To implement the distance vector protocol, we added three data structures:
   a map with neighbor costs (m_costs), a map with the estimated cost for all known
   nodes (m_dv), and each neighbor's distance vector (added to the neighbor table).
   We initialize m_dv with the <NodeSelf, Cost=0>.  Then, the nodes begin to send
   out Hello packets periodically, which are used for neighbor discovery (same as
   above).  Based on the neighbor info that is collected in ProcessHelloReq(),
   if the neighbor table changes, then the node runs the Bellman-Ford algorithm
   and sends out a DV message with its distance vector.

   When a node subsequently receives that DV message, ProcessDVTableMessage()
   stores that neighbor's DV vector in the Neighbor Table and calls
   BellmanFord().

   The BellmanFord() function is the heart of the Distance Vector class,
   It first searches the neighbor's new distance vector for unknown nodes
   in the network.  It then iterates through all the known nodes to find
   the minimum distance and the next hop to reach that node. Based on this
   information, it also updates the routing table.

   This function posed a number of subtle problems.  Although the algorithm
   is straightforward on the page when all nodes in the network are known,
   we found that it was challenging to deal with all of the various cases
   when nodes enter or leave the network.  For example, it took use some time
   to realize that in any call to BellmanFord(), the algorithm will always 
   find a minCost, even if that value is infinity.  At the same time, it 
   is necessary to identify whether or not that minCost changed from the previous
   value, which determines if we need to send a new DV message.  Finally,
   if the minCost is less than infinity, then there is route to reach that
   destination and so in this case, we also update the routing table.
   This logic somewhat tricky to work out in the code because we had to 
   compare values to account for many different possible outcomes.
   Furthermore, the various combinations of messages that can be sent
   or received made it necessary to think through the calls to BellmanFord()
   and SendDTableMessage() very carefully, though we believe we were generally
   successful in managing and forwarding packets.

### Testing ###
We wrote three test files for our code.  The main scenario/topology (14-ls.sce)  
breaks the shortest path multiple times and confirms that the routes are 
adjusted correctly.  The first route should be 0-1-4-12-13.  Then, we break
the link between 4 and 12, and the new shortest path should be down the middle.
Then, we break 8 and 11, and the shortest path between 0 and 13 moves up the
left side.  Then, when disconnecting 6, it takes the only remaining path.
Finally, when 7 is removed, the network is partitioned into two halves,
and the ping from 0 to 13 fails.

We believe that this test works successfully in both LS and DV.

             13
          /  |  \
        10   11   12
        |    |    |
        9    |    |
      / |    |    |
    6   7    8    |
      \ |    |    |
        5    |    |
        |    |    |
        2    3    4
          \  |  /
             1
             |
             0


### Extra credit ###
Apart from the reverse poisoning, our implementation is
able to incorporate arbitrary cost information for each link. Even though we
couldn't modify the simulator-main.cc code to allow for the reading of the link
weights in the topology file, if that is properly set up our program should be
able to run without practically any modifications.
