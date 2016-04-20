#ifndef TESTRESULT_H_
#define TESTRESULT_H_

#include <string>
#include <string.h>
#include "ns3/ipv4-address.h"

using namespace ns3;


void openResultFile(std::string filename);

void closeResultFile();

void startDumpNeighbor();

void endDumpNeighbor();

void startDumpRoute();

void endDumpRoute();

void startDumpTrafficTrace();

void endDumpTrafficTrace();

void checkNeighborTableEntry(uint32_t neighborNum, Ipv4Address neighborAddr, Ipv4Address ifAddr);

void checkNeighborTableEntry(std::string neighborNum, Ipv4Address neighborAddr, Ipv4Address ifAddr);

void checkRouteTableEntry(uint32_t dstNum, Ipv4Address dstAddr, uint32_t nextHopNum,
		Ipv4Address nextHopAddr, Ipv4Address ifAddr, uint32_t cost);

void checkRouteTableEntry(std::string dstNum, Ipv4Address dstAddr, uint32_t nextHopNum,
		Ipv4Address nextHopAddr, Ipv4Address ifAddr, uint32_t cost);

void checkTrafficTrace(std::string g_nodeId, std::string g_moduleName);


#endif /* TESTRESULT_H_ */
