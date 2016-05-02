#include "ns3/test-result.h"
#include <stdio.h>
#include <fstream>
#include <iostream>
#include <map>

using namespace std;

struct RoutingTableEntry{
  Ipv4Address destAddr;
  uint32_t nextHopNum;
  Ipv4Address nextHopAddr;
  Ipv4Address interfaceAddr;
  uint32_t cost;
};

struct NeighborTableEntry
{
  Ipv4Address neighborAddr;
  Ipv4Address interfaceAddr;
};


std::map<uint32_t, NeighborTableEntry> m_neighborTable;
std::map<uint32_t, RoutingTableEntry> m_routingTable;

ifstream ifs;
int entry_total;
int entry_count;
bool is_enabled;
std::string test_name;
char error[100][100];
int error_count;

void openResultFile(std::string fileName)
{
	if(fileName == "")
		is_enabled = false;
	else
		is_enabled = true;
	if(!is_enabled)
		return;
	ifs.open(fileName.c_str(), ifstream::in);
}

void closeResultFile()
{
	if(!is_enabled)
		return;
	if(ifs.is_open())
		ifs.close();
}

void startDumpNeighbor()
{
	if(!is_enabled)
		return;
	test_name = "";
	while(test_name.length() == 0)
		std::getline(ifs, test_name);
	ifs >> entry_total;
	entry_count = 0;
	m_neighborTable.clear();
	for(int i=0; i<entry_total; i++)
	{
		uint32_t num;
		std::string addr1, addr2;
		ifs >> num >> addr1 >> addr2;
		Ipv4Address ipv4addr1(addr1.c_str()), ipv4addr2(addr2.c_str());
		NeighborTableEntry entry;
		entry.neighborAddr = ipv4addr1;
		entry.interfaceAddr = ipv4addr2;
		m_neighborTable[num] = entry;
	}
	error_count = 0;
}


void startDumpTrafficTrace()
{
	if(!is_enabled)
		return;
	test_name = "";
	while(test_name.length() == 0)
		std::getline(ifs, test_name);
	ifs >> entry_total;
	entry_count = 0;
	error_count = 0;
}


void startDumpRoute()
{
	if(!is_enabled)
		return;
	test_name = "";
	while(test_name.length() == 0)
		std::getline(ifs, test_name);
	ifs >> entry_total;
	entry_count = 0;
	for(int i=0; i<entry_total; i++)
	{
		uint32_t num1, num2, cost1;
		std::string addr1, addr2, addr3;
		ifs >> num1 >> addr1 >> num2 >> addr2 >> addr3 >> cost1;
		Ipv4Address ipv4addr1(addr1.c_str()), ipv4addr2(addr2.c_str()), ipv4addr3(addr3.c_str());
		RoutingTableEntry entry;
		entry.destAddr = ipv4addr1;
		entry.nextHopNum = num2;
		entry.nextHopAddr = ipv4addr2;
		entry.interfaceAddr = ipv4addr3;
		entry.cost = cost1;
		m_routingTable[num1] = entry;
	}
	error_count = 0;
}


void endDump()
{
	if(!is_enabled)
		return;
	if(entry_count < entry_total)
	{
		sprintf(error[error_count++], "entry_count %i is less than entry_total %i\n", entry_count, entry_total);
	}
	printf("\x1b[33m%s is %s\x1b[0m\n\n", test_name.c_str(), error_count==0?"correct":"wrong");
	for(int i=0; i<error_count; i++)
		printf("\x1b[33merror %i: %s\x1b[0m\n", i+1, error[i]);
}

void endDumpNeighbor()
{
	endDump();
}

void endDumpRoute()
{
	endDump();
}


void endDumpTrafficTrace()
{
	if(!is_enabled)
		return;
	if(entry_count < entry_total)
	{
		error_count = 1;
		sprintf(error[0], "there is less traffic trace than expected in standard answer (%i)\n", entry_total);
	}
	while(entry_count < entry_total)
	{
		std::string line = "";
		while(line.length() == 0)
			std::getline(ifs, line);
		entry_count++;
	}
	printf("\x1b[33m\n%s is %s\x1b[0m\n\n", test_name.c_str(), error_count==0?"correct":"wrong");
	for(int i=0; i<error_count; i++)
		printf("\x1b[33merror %i: %s\x1b[0m\n", i+1, error[i]);
}

void checkTrafficTrace(std::string g_nodeId, std::string g_moduleName)
{
	if(!is_enabled)
		return;
//	printf("\x1b[33m check traffic trace\x1b[0m\n");
	if(entry_count >= entry_total)
	{
		error_count = 1;
		sprintf(error[0], "there is more traffic trace than expected in standard answer (%i)\n", entry_total);
		return;
	}
	entry_count++;
	std::string line = "";
	char answer[50];
	while(line.length() == 0)
		std::getline(ifs, line);
	sprintf(answer, "*TRAFFIC* Node: %s, Module: %s", g_nodeId.c_str(), g_moduleName.c_str());
	if(strncmp(line.c_str(), answer, strlen(answer)) != 0)
	{
		printf("\x1b[33mIncorrect trace. The correct answer is:\n%s\x1b[0m\n", line.c_str());
		error_count = 1;
		sprintf(error[0], "some trace doesn't match with standard answer.\n");
		return;
	}
}


void checkNeighborTableEntry(uint32_t neighborNum, Ipv4Address neighborAddr, Ipv4Address ifAddr)
{
	if(!is_enabled)
		return;
	entry_count++;
	std::map<uint32_t, NeighborTableEntry>::iterator iter = m_neighborTable.find(neighborNum);
	if(iter == m_neighborTable.end())
	{
		sprintf(error[error_count++], "neighbor %i is nout found in standard answer", neighborNum);
		return;
	}
	NeighborTableEntry entry = iter->second;
	if(!neighborAddr.IsEqual(entry.neighborAddr))
	{
		sprintf(error[error_count++], "for neighbor %i, neighbor address doesn't match\n", neighborNum);
		return;
	}
	if(!ifAddr.IsEqual(entry.interfaceAddr))
	{
		sprintf(error[error_count++], "for neighbor %i, interface address doesn't match\n", neighborNum);
		return;
	}
	m_neighborTable.erase(iter);
}

void checkNeighborTableEntry(std::string neighborNum, Ipv4Address neighborAddr, Ipv4Address ifAddr)
{
	uint32_t num;
	std::istringstream sin(neighborNum);
	sin >> num;
	checkNeighborTableEntry(num, neighborAddr, ifAddr);
}

void checkRouteTableEntry(std::string dstNum, Ipv4Address dstAddr, uint32_t nextHopNum,
		Ipv4Address nextHopAddr, Ipv4Address ifAddr, uint32_t cost)
{
	uint32_t num;
	std::istringstream sin(dstNum);
	sin >> num;
	checkRouteTableEntry(num, dstAddr, nextHopNum, nextHopAddr, ifAddr, cost);
}


//Note: It is OK to output route to destination that is different from standard answer,
//as long as it gives the minimum cost.

void checkRouteTableEntry(uint32_t dstNum, Ipv4Address dstAddr, uint32_t nextHopNum,
		Ipv4Address nextHopAddr, Ipv4Address ifAddr, uint32_t cost)
{
	if(!is_enabled)
		return;
	entry_count++;
	std::map<uint32_t, RoutingTableEntry>::iterator iter = m_routingTable.find(dstNum);
	if(iter == m_routingTable.end())
	{
		sprintf(error[error_count++], "route to %i is nout found in standard answer", dstNum);
		return;
	}
	RoutingTableEntry entry = iter->second;
	if(!dstAddr.IsEqual(entry.destAddr))
	{
		sprintf(error[error_count++], "for route to %i, destination address doesn't match\n", dstNum);
		return;
	}
	if(cost != entry.cost)
	{
		sprintf(error[error_count++], "for route to %i, cost doesn't match\n", entry_count);
		return;
	}
	if(nextHopNum != entry.nextHopNum)
	{
		sprintf(error[error_count++], "for route to %i, next hop number doesn't match. But it gives the same minimum cost\n", dstNum);
		return;
	}
	if(!nextHopAddr.IsEqual(entry.nextHopAddr))
	{
		sprintf(error[error_count++], "for route to %i, next hop address doesn't match\n", dstNum);
		return;
	}
	if(!ifAddr.IsEqual(entry.interfaceAddr))
	{
		sprintf(error[error_count++], "for route to %i, interface address doesn't match\n", dstNum);
		return;
	}
	m_routingTable.erase(iter);
}



