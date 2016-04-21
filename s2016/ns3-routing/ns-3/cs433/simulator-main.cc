/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2010 University of Pennsylvania
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*  -----cs433 SIMULATOR-----
 *  Students need to implement RoutingModule for Project 1 and Application for Project 2 
 *  This code:
 *  1. Reads Topology (Inet format)
 *  2. Reads Simulation Script
 *  3. Spawns Runtime Command Handler Thread
 */


/* Includes */
#include <fstream>
#include <stdlib.h>
#include <stdint.h>
#include <iostream>
#include <string.h>
#include <vector>
#include <map>
#include "ns3/core-module.h"
#include "ns3/common-module.h"
#include "ns3/node-module.h"
#include "ns3/simulator-module.h"
#include "ns3/helper-module.h"
#include "ns3/object.h"
#include "ns3/nstime.h"
#include "ns3/ipv4-list-routing.h"
#include "ns3/test-result.h"
#include "ns3/ls-routing-helper.h"
#include "ns3/dv-routing-helper.h"
#include "ns3/test-app-helper.h"
#include "ns3/l4-platform-helper.h"
#include "ns3/l4-device.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

/* Defines */
#define MAX_SCAN_DELAY 256
#define MIN_SCAN_DELAY 1
#define MAX_NOT_READY 3

#define LS_MODULE_NAME "LS"
#define DV_MODULE_NAME "DV"
#define APP_MODULE_NAME "TESTAPP"
#define SIM_MODULE_NAME "SIM"

using namespace ns3;

/* Log component for this file */
NS_LOG_COMPONENT_DEFINE("SimulatorMain");

struct CommandHandlerArgument
{
  std::string scriptFile;
  NodeContainer nodeContainer;
  void *simulatorMain;
};

void Tokenize(const std::string& str, std::vector<std::string>& tokens, const std::string& delimiters);
void UpperCase (std::string &str);

class SimulatorMain
{
  public:
    void Start (std::string scriptFile, NodeContainer nodeContainer, NetDeviceContainer* ndc, uint32_t totalNodes, uint32_t totalLinks);
    void Stop ();

    // Command Handler
    static void *CommandHandler (void *arg);
    void ProcessCommandTokens (std::vector<std::string> tokens, Time time);
    void ProcessNodeCommandTokens (uint32_t nodeNum, std::vector<std::string> tokens, Time time);
    void ReadCommandTokens (void);

    void LinkOperation (uint32_t linkNumber, bool isUp);
    void P2POperation (uint32_t nodeANum, uint32_t nodeBNum, bool isUp);
    void AllInterfacesOperation (uint32_t nodeNumber, bool isUp);

    void SetRoutingVerbose (Ptr<CommRoutingProtocol> routingProtocol, std::vector<std::string> tokens);
    void SetApplicationVerbose (Ptr<CommApplication> application, std::vector<std::string> tokens);

    std::map<uint32_t, Ipv4Address> g_nodeAddressMap;
    std::map<Ipv4Address, uint32_t> g_addressNodeMap;
  
  private:
    std::string m_scriptFile;
    NodeContainer m_nodeContainer;
    NetDeviceContainer* m_ndc;
    std::vector<std::string> m_tokens;
    bool m_readyToRead;
    uint32_t m_scanDelay;
    uint32_t m_notReadyCount;
    uint32_t m_totalNodes, m_totalLinks;

    pthread_t m_commandHandlerThreadId;
    struct CommandHandlerArgument m_thArgument;

    // Print
    void PrintCharArray (uint8_t*, uint32_t, std::ostream&);
    void PrintHexArray (uint8_t*, uint32_t, std::ostream&);
};

void
SimulatorMain::Start (std::string scriptFile, NodeContainer nodeContainer, NetDeviceContainer* ndc, uint32_t totalNodes, uint32_t totalLinks)
{
  NS_LOG_FUNCTION_NOARGS ();
  m_thArgument.scriptFile = scriptFile;
  m_thArgument.nodeContainer = nodeContainer;
  m_thArgument.simulatorMain = (void *) this;

  m_nodeContainer = nodeContainer;
  m_ndc = ndc;
  m_totalNodes = totalNodes;
  m_totalLinks = totalLinks;
  m_scriptFile = scriptFile;
  m_readyToRead = false;
  m_scanDelay = MIN_SCAN_DELAY;
  m_notReadyCount = 0;

  // Process script file
  if (scriptFile != "")
    {
      std::ifstream file;
      file.open (scriptFile.c_str ());
      if (file.is_open ())
        {
          NS_LOG_INFO ("Reading script file: " << scriptFile);
          Time time = MilliSeconds (0.0);
          std::string commandLine;
          do
            {
              std::getline (file, commandLine, '\n');
              if (commandLine == "")
                continue;
              //UpperCase (commandLine);
              NS_LOG_INFO ("Adding command: " << commandLine << std::endl);
              Tokenize (commandLine, m_tokens, " ");
              if (m_tokens.size () == 0)
                {
                  NS_LOG_ERROR ("Failed to Tokenize: " << commandLine);
                  continue;
                }
                // Check for time command
              std::vector<std::string>::iterator iterator = m_tokens.begin ();
              UpperCase (*iterator);
              if (*iterator == "TIME")
                {
                  if (m_tokens.size () != 2)
                    {
                      NS_LOG_ERROR ("Invalid number of arguments with time command!");
                      continue;
                    }
                  iterator++;
                  std::istringstream sin (*iterator);
                  uint64_t delta;
                  sin >> delta;
                  time = MilliSeconds ( time.GetMilliSeconds() + delta );
                  std::cout << "Time Pointer: " << time.GetMilliSeconds() << std::endl;
                  m_tokens.clear ();
                  continue;
                }
              ProcessCommandTokens (m_tokens, MilliSeconds(time.GetMilliSeconds()));
              m_tokens.clear ();

            } while (!file.eof ());
        }
    }
  // Schedule processing
  Simulator::Schedule (MilliSeconds (m_scanDelay), &SimulatorMain::ReadCommandTokens, this);
  if (pthread_create (&m_commandHandlerThreadId, NULL, SimulatorMain::CommandHandler, &m_thArgument) != 0)
    {
      perror ("New thread creation failed, exiting...");
      exit (1);
    }
}

void
SimulatorMain::Stop ()
{
  NS_LOG_FUNCTION_NOARGS ();
  pthread_cancel (m_commandHandlerThreadId);
  pthread_join (m_commandHandlerThreadId, NULL);
}

void*
SimulatorMain::CommandHandler (void* arg)
{
  struct CommandHandlerArgument thArgument = *((struct CommandHandlerArgument*) arg);
  std::string scriptFile = thArgument.scriptFile;
  NodeContainer nodeContainer = thArgument.nodeContainer;
  SimulatorMain* simulatorMain = (SimulatorMain*) thArgument.simulatorMain;
  
  while (1)
    {
      std::string commandLine;
      std::cout << "\nCommand > ";
      std::getline (std::cin, commandLine, '\n');
      if (commandLine == "")
        continue;
      if (simulatorMain->m_readyToRead == true)
        {
          std::cout << "Simulator busy, please try again...\n";
          continue;    
        }
      Tokenize (commandLine, simulatorMain->m_tokens, " ");
      std::vector<std::string>::iterator iterator = simulatorMain->m_tokens.begin ();
      if (simulatorMain -> m_tokens.size() == 0)
        {
          continue;
        }
      simulatorMain -> m_readyToRead = true;
    }
  Simulator::Stop ();
  pthread_exit (NULL);
}

void
SimulatorMain::ReadCommandTokens (void)
{
  if (m_readyToRead == true)
    {
      m_scanDelay = MIN_SCAN_DELAY;
      if (m_tokens.size() > 0)
        {
          ProcessCommandTokens (m_tokens, MilliSeconds (0.0));
        }
      m_tokens.clear();
      m_readyToRead = false;
    }
  else
    {
      m_notReadyCount++;
      if (m_notReadyCount == MAX_NOT_READY)
        {
          m_notReadyCount = 0;
          m_scanDelay = m_scanDelay * 2;
          if (m_scanDelay > MAX_SCAN_DELAY)
            {
              m_scanDelay = MAX_SCAN_DELAY;
            }
        }
    }
  Simulator::Schedule (MilliSeconds (m_scanDelay), &SimulatorMain::ReadCommandTokens, this);
}

void
SimulatorMain::ProcessCommandTokens (std::vector<std::string> tokens, Time time)
{
  //Process tokens
  std::vector<std::string>::iterator iterator = tokens.begin();
  if ((*iterator).at (0) == '#')
    return;
  UpperCase (*iterator);
  std::string command = *iterator;
  if (command == "QUIT")
    {
      NS_LOG_INFO ("Scheduling command: quit...");
      Simulator::Stop (MilliSeconds (time.GetMilliSeconds()));
      return;
    }
  else if (*iterator == "TIME")
	  {
				std::cout << "Current Time: " <<  Simulator::Now().GetMilliSeconds() << " ms" << std::endl;
	  }
  else if (tokens.size() < 2)
    {
      return;
    }

  // Check for Link Command
  else if (command == "LINK")
    {
      tokens.erase (iterator);
      if (tokens.size() == 2)
        {
          iterator = tokens.begin ();
          UpperCase (*iterator);
          std::string operation = *iterator;
          tokens.erase (iterator);
          iterator = tokens.begin ();
          std::istringstream sin (*iterator);
          uint32_t linkNumber;
          sin >> linkNumber;             
          if (operation == "DOWN")
            {
              Simulator::Schedule (MilliSeconds (time.GetMilliSeconds ()), &SimulatorMain::LinkOperation, this, linkNumber, false);
              return;
            }
          else if (operation == "UP")
            {
              Simulator::Schedule (MilliSeconds (time.GetMilliSeconds ()), &SimulatorMain::LinkOperation, this, linkNumber, true);
              return;
            }
        }
      else if (tokens.size () == 3)
        {
          iterator = tokens.begin ();
          UpperCase (*iterator);
          std::string operation = *iterator;
          tokens.erase (iterator);
          iterator = tokens.begin ();
          std::istringstream sinA (*iterator);
          uint32_t nodeANum;
          sinA >> nodeANum;    
          tokens.erase (iterator);
          iterator = tokens.begin ();
          std::istringstream sinB (*iterator);
          uint32_t nodeBNum;
          sinB >> nodeBNum;      
          if (operation == "DOWN")
            {
              Simulator::Schedule (MilliSeconds (time.GetMilliSeconds ()), &SimulatorMain::P2POperation, this, nodeANum, nodeBNum, false);
              return;
            }
          else if (operation == "UP")
            {
              Simulator::Schedule (MilliSeconds (time.GetMilliSeconds ()), &SimulatorMain::P2POperation, this, nodeANum, nodeBNum, true);
              return;
            }


        }
    }
  else if (*iterator == "NODELINKS")
    {
      tokens.erase (iterator);
      if (tokens.size() != 2)
        return;
      iterator = tokens.begin ();
      UpperCase (*iterator);
      std::string operation = *iterator;
      tokens.erase (iterator);
      iterator = tokens.begin ();
      std::istringstream sin (*iterator);
      uint32_t nodeNumber;
      sin >> nodeNumber;             
      if (operation == "DOWN")
        {
          Simulator::Schedule (MilliSeconds (time.GetMilliSeconds ()), &SimulatorMain::AllInterfacesOperation, this, nodeNumber, false);
          return;
        }
      else if (operation == "UP")
        {
          Simulator::Schedule (MilliSeconds (time.GetMilliSeconds ()), &SimulatorMain::AllInterfacesOperation, this, nodeNumber, true);
          return;
        }
    }
  else if (*iterator == "*")    
    {
      // Send command to all nodes 
      tokens.erase (iterator);
      for (uint32_t i = 0 ; i < m_totalNodes ; i++)
        {
          ProcessNodeCommandTokens (i, tokens, time);
        }
    }
  else
    {
      // Get node number
      std::istringstream sin (*iterator);
      uint32_t nodeNumber;
      sin >> nodeNumber;
      if (nodeNumber >= m_totalNodes)
        { 
          NS_LOG_ERROR ("Invalid node number!");
          return;
        }
      tokens.erase (iterator);
      ProcessNodeCommandTokens (nodeNumber, tokens, time);
    }
}

void
SimulatorMain::ProcessNodeCommandTokens (uint32_t nodeNumber, std::vector<std::string> tokens, Time time)
{
  if (nodeNumber >= m_totalNodes)
    { 
      NS_LOG_ERROR ("Invalid node number!");
      return;
    }

  if (tokens.size () == 0)
    return;
  std::vector<std::string>::iterator iterator = tokens.begin();
  UpperCase (*iterator);
  std::string module = *iterator;    
  tokens.erase (iterator);
  if (tokens.size () == 0)
    return;
  iterator = tokens.begin();
  // module name
  if (module == APP_MODULE_NAME || module == "APP")
    {
      // Get application pointer
      Ptr<TestApp> application = m_nodeContainer.Get(nodeNumber)->GetApplication(0)->GetObject<TestApp> ();
      std::string command = std::string ((*iterator).c_str(), (*iterator).length());
      UpperCase (command);
      if (command == "VERBOSITY" || command == "VERBOSE")
        {
          tokens.erase (iterator);
          if (tokens.size () == 0)
            return;
          Simulator::Schedule (MilliSeconds (time.GetMilliSeconds ()), &SimulatorMain::SetApplicationVerbose, this, application, tokens);
          return;
        }
      else
        {
    	  if(tokens.size() == 3 && strcmp(tokens[0].c_str(), "PING") == 0)
    	  {
    		  Simulator::Schedule (MilliSeconds (time.GetMilliSeconds()), &startDumpTrafficTrace);
    		  Simulator::Schedule (MilliSeconds (time.GetMilliSeconds()), &TestApp::ProcessCommand, application, tokens);
    		  Simulator::Schedule (MilliSeconds (time.GetMilliSeconds()+1000), &endDumpTrafficTrace);
    	  }
    	  else
    	  {
    		  Simulator::Schedule (MilliSeconds (time.GetMilliSeconds()), &TestApp::ProcessCommand, application, tokens);
    	  }
          return;
        }
    }
  else if (module == LS_MODULE_NAME || module == DV_MODULE_NAME)
    {
      Ptr<Ipv4> ipv4 = m_nodeContainer.Get(nodeNumber)->GetObject<Ipv4> ();
      if (!ipv4)
        return;
      Ptr<Ipv4ListRouting> listRouting = DynamicCast<Ipv4ListRouting> (ipv4->GetRoutingProtocol ());
      if (!listRouting)
        {
          return;
        }
      uint32_t protocols = listRouting->GetNRoutingProtocols ();
      for (uint32_t i = 0 ; i < protocols ; i++)
        {
          std::string command = std::string ((*iterator).c_str(), (*iterator).length());
          UpperCase (command);
          int16_t priority;
          if (module == LS_MODULE_NAME)
            {
              Ptr<LSRoutingProtocol> lsRouting = DynamicCast<LSRoutingProtocol> (listRouting->GetRoutingProtocol (i, priority));
              if (lsRouting)
                {
                  if (command == "VERBOSITY" || command == "VERBOSE")
                    {
                      tokens.erase (iterator);
                      if (tokens.size () == 0)
                        return;
                      Simulator::Schedule (MilliSeconds (time.GetMilliSeconds ()), &SimulatorMain::SetRoutingVerbose, this, lsRouting, tokens);
                      return;
                    }
                  else
                    {
                	  if(tokens.size() == 2 && strcmp(tokens[0].c_str(), "DUMP")==0 && strcmp(tokens[1].c_str(),"NEIGHBORS") == 0)
                	  {
                          Simulator::Schedule (MilliSeconds (time.GetMilliSeconds()), &startDumpNeighbor);
                		  Simulator::Schedule (MilliSeconds (time.GetMilliSeconds()), &LSRoutingProtocol::ProcessCommand, lsRouting, tokens);
                          Simulator::Schedule (MilliSeconds (time.GetMilliSeconds()), &endDumpNeighbor);
                	  }
                	  else if(tokens.size() == 2 && strcmp(tokens[0].c_str(), "DUMP")==0 && strcmp(tokens[1].c_str(),"ROUTES") == 0)
                	  {
                          Simulator::Schedule (MilliSeconds (time.GetMilliSeconds()), &startDumpRoute);
                		  Simulator::Schedule (MilliSeconds (time.GetMilliSeconds()), &LSRoutingProtocol::ProcessCommand, lsRouting, tokens);
                          Simulator::Schedule (MilliSeconds (time.GetMilliSeconds()), &endDumpRoute);
                	  }
                	  else
                	  {
                		  Simulator::Schedule (MilliSeconds (time.GetMilliSeconds()), &LSRoutingProtocol::ProcessCommand, lsRouting, tokens);
                	  }
                      return;
                    }
                }
            }
          else if (module == DV_MODULE_NAME)
            {
              Ptr<DVRoutingProtocol> dvRouting = DynamicCast<DVRoutingProtocol> (listRouting->GetRoutingProtocol (i, priority));
              if (dvRouting)
                {
                  if (command == "VERBOSITY" || command == "VERBOSE")
                    {
                      tokens.erase (iterator);
                      if (tokens.size () == 0)
                        return;
                      Simulator::Schedule (MilliSeconds (time.GetMilliSeconds ()), &SimulatorMain::SetRoutingVerbose, this, dvRouting, tokens);
                      return;
                    }
                  else
                    {
                	  if(tokens.size() == 2 && strcmp(tokens[0].c_str(), "DUMP")==0 && strcmp(tokens[1].c_str(),"NEIGHBORS") == 0)
                	  {
                          Simulator::Schedule (MilliSeconds (time.GetMilliSeconds()), &startDumpNeighbor);
                		  Simulator::Schedule (MilliSeconds (time.GetMilliSeconds()), &DVRoutingProtocol::ProcessCommand, dvRouting, tokens);
                          Simulator::Schedule (MilliSeconds (time.GetMilliSeconds()), &endDumpNeighbor);
                	  }
                	  else if(tokens.size() == 2 && strcmp(tokens[0].c_str(), "DUMP")==0 && strcmp(tokens[1].c_str(),"ROUTES") == 0)
                	  {
                          Simulator::Schedule (MilliSeconds (time.GetMilliSeconds()), &startDumpRoute);
                		  Simulator::Schedule (MilliSeconds (time.GetMilliSeconds()), &DVRoutingProtocol::ProcessCommand, dvRouting, tokens);
                          Simulator::Schedule (MilliSeconds (time.GetMilliSeconds()), &endDumpRoute);
                	  }
                	  else
                	  {
                		  Simulator::Schedule (MilliSeconds (time.GetMilliSeconds()), &DVRoutingProtocol::ProcessCommand, dvRouting, tokens);
                	  }
                      return;
                    }
                }

            }
        }
    }
  else if (module == SIM_MODULE_NAME)
    {
      if (tokens.size () == 0)
        return;
      UpperCase (*iterator);
      std::string command = *iterator;
      tokens.erase (iterator);
      iterator = tokens.begin ();
      return;
    }

}

void
SimulatorMain::SetApplicationVerbose (Ptr<CommApplication> application, std::vector<std::string> tokens)
{
  if (tokens.size() != 2)
    return;
  std::vector<std::string>::iterator iterator = tokens.begin();
  UpperCase (*iterator);
  std::string type = (*iterator);
  iterator++;
  UpperCase (*iterator);
  std::string state = (*iterator);
  bool on;
  if (state == "ON")
    {
      on = true;
    }
  else if (state == "OFF")
    {
      on = false;
    }
  else
    {
      NS_LOG_ERROR ("Invalid Verbose setting!");
      return;
    }

  if (type == "TRAFFIC")
    {
      application->SetTrafficVerbose (on);
    }
  else if (type == "ERROR")
    { 
      application->SetErrorVerbose (on);
    }
  else if (type == "DEBUG")
    { 
      application->SetDebugVerbose (on);
    }
  else if (type == "STATUS")
    {
      application->SetStatusVerbose (on);
    }
  else if (type == "ALL")
    {
      application->SetTrafficVerbose (on);
      application->SetErrorVerbose (on);
      application->SetDebugVerbose (on);
      application->SetStatusVerbose (on);
    }
}

void
SimulatorMain::SetRoutingVerbose (Ptr<CommRoutingProtocol> routingProtocol, std::vector<std::string> tokens)
{
  if (tokens.size() != 2)
    return;
  std::vector<std::string>::iterator iterator = tokens.begin();
  UpperCase (*iterator);
  std::string type = (*iterator);
  iterator++;
  UpperCase (*iterator);
  std::string state = (*iterator);
  bool on;
  if (state == "ON")
    {
      on = true;
    }
  else if (state == "OFF")
    {
      on = false;
    }
  else
    {
      NS_LOG_ERROR ("Invalid Verbose setting!");
      return;
    }

  if (type == "TRAFFIC")
    {
      routingProtocol->SetTrafficVerbose (on);
    }
  else if (type == "ERROR")
    { 
      routingProtocol->SetErrorVerbose (on);
    }
  else if (type == "DEBUG")
    { 
      routingProtocol->SetDebugVerbose (on);
    }
  else if (type == "STATUS")
    {
      routingProtocol->SetStatusVerbose (on);
    }
  else if (type == "ALL")
    {
      routingProtocol->SetTrafficVerbose (on);
      routingProtocol->SetErrorVerbose (on);
      routingProtocol->SetDebugVerbose (on);
      routingProtocol->SetStatusVerbose (on);
    }
}

void
SimulatorMain::AllInterfacesOperation (uint32_t nodeNumber, bool isUp)
{
  if (nodeNumber >= m_totalNodes)
    return;
  Ptr<Node> node = m_nodeContainer.Get(nodeNumber);
  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
  if (!ipv4)
    return;
  uint32_t totalDevices = node->GetNDevices ();
  if (!isUp)
    {
      NS_LOG_INFO ("Bringing down interfaces of node: " << nodeNumber);
    }
  else
    {
      NS_LOG_INFO ("Bringing up interfaces of node: " << nodeNumber);
    }
  for (uint32_t i = 0 ; i < totalDevices ; i++)
    {
      Ptr<NetDevice> netDevice = node->GetDevice (i);
      if (!isUp)
        {
          ipv4->SetDown (i);
        }
      else
        {
          ipv4->SetUp (i);
        }
    }
}

void
SimulatorMain::P2POperation (uint32_t nodeANum, uint32_t nodeBNum, bool isUp)
{
  if (nodeANum >= m_totalNodes || nodeBNum >= m_totalNodes)
    return;
  Ptr<Node> nodeA = m_nodeContainer.Get(nodeANum);
  Ptr<Ipv4> ipv4A = nodeA->GetObject<Ipv4> ();

  Ptr<Node> nodeB = m_nodeContainer.Get(nodeBNum);
  Ptr<Ipv4> ipv4B = nodeB->GetObject<Ipv4> ();
  if (!ipv4A || !ipv4B)
    return;

  uint32_t numDevicesA = nodeA->GetNDevices ();
  for (uint32_t i = 0 ; i < numDevicesA ; i++)
    {
      Ptr<NetDevice> device = nodeA->GetDevice (i);
      Ptr<Channel> channel = device->GetChannel ();
      if (!channel)
        continue;
      uint32_t chDevices = channel->GetNDevices ();
      for (uint32_t j = 0 ; j < chDevices ; j++)
        {
          Ptr<NetDevice> nodeDevice = channel->GetDevice (j);
          Ptr<Node> node = nodeDevice->GetNode ();
          if (node == nodeB)
            {
              // Operate on this link
              uint32_t ifIndexA = device->GetIfIndex ();
              uint32_t ifIndexB = nodeDevice->GetIfIndex ();
              if (!isUp)
                {
                  NS_LOG_INFO ("Bringing down link between nodes: " << nodeANum << " and " << nodeBNum);
                  ipv4A->SetDown (ifIndexA);
                  ipv4B->SetDown (ifIndexB);
                }
              else
                {
                  NS_LOG_INFO ("Bringing up link between nodes: " << nodeANum << " and " << nodeBNum);
                  ipv4A->SetUp (ifIndexA);
                  ipv4B->SetUp (ifIndexB);
                }

            }
        }
    }
  

}

void
SimulatorMain::LinkOperation (uint32_t linkNumber, bool isUp)
{
  if (linkNumber > m_totalLinks && !m_ndc)
    {
      NS_LOG_ERROR ("Invalid link number!");
      return;
    }
  Ptr<NetDevice> netDeviceA = m_ndc[linkNumber].Get(0);
  Ptr<NetDevice> netDeviceB = m_ndc[linkNumber].Get(1);
  if (!netDeviceB)
    return;
  Ptr<Node> nodeA = netDeviceA->GetNode ();
  Ptr<Node> nodeB = netDeviceB->GetNode ();
  uint32_t ifIndexA = netDeviceA->GetIfIndex ();
  uint32_t ifIndexB = netDeviceB->GetIfIndex ();
  Ptr<Ipv4> ipv4A = nodeA->GetObject<Ipv4> ();
  Ptr<Ipv4> ipv4B = nodeB->GetObject<Ipv4> ();
  if (!isUp)
    {
      NS_LOG_INFO ("Bringing down link: " << linkNumber);
      ipv4A->SetDown (ifIndexA);
      ipv4B->SetDown (ifIndexB);
    }
  else
    {
      NS_LOG_INFO ("Bringing up link: " << linkNumber);
      ipv4A->SetUp (ifIndexA);
      ipv4B->SetUp (ifIndexB);
    }
}

void
UpperCase (std::string &str)
{
  std::string::iterator iter;
  for (iter = str.begin () ; iter != str.end(); iter++)
    {
      *iter = std::toupper ((unsigned char)*iter);
    }
}

/* Method Tokenize, Credits:  http://oopweb.com/CPP/Documents/CPPHOWTO/Volume/C++Programming-HOWTO-7.html */

void 
Tokenize(const std::string& str,
    std::vector<std::string>& tokens,
    const std::string& delimiters)
{
  // Skip delimiters at beginning.
  std::string::size_type lastPos = str.find_first_not_of(delimiters, 0);
  // Find first "non-delimiter".
  std::string::size_type pos = str.find_first_of(delimiters, lastPos);

  while (std::string::npos != pos || std::string::npos != lastPos)
  {
    // Found a token, add it to the vector.
    tokens.push_back(str.substr(lastPos, pos - lastPos));
    // Skip delimiters.  Note the "not_of"
    lastPos = str.find_first_not_of(delimiters, pos);
    // Find next "non-delimiter"
    pos = str.find_first_of(delimiters, lastPos);
  }
}

int
main (int argc, char *argv[])
{
  std::string scriptFile = "";
  std::string topologyFile = "";
  std::string routingProtocol = LS_MODULE_NAME;
  std::string animFile = "";
  std::string realStack = "";
  std::string realTime = "";
  std::string resultFile = "";
  std::string localAddress = "";

  // Command Line parameters
  CommandLine cmd;
  cmd.AddValue ("inet-topo", "Name of the Topology file in Inet format", topologyFile);
  cmd.AddValue ("scenario", "Name of the scenario file", scriptFile);
  cmd.AddValue ("routing", "Routing Protocol: LS/DV/ANY/NS3", routingProtocol);
  cmd.AddValue ("result-check", "Name of the result checking file", resultFile);
  cmd.AddValue ("anim-file",  "File Name for Animation Logs", animFile);
  cmd.AddValue ("real-stack", "Use real IP stack/sockets: <yes/no>", realStack);
  cmd.AddValue ("real-time", "Run simulation in wall clock mode (real-time): <yes/no>", realTime);
  cmd.AddValue ("local-address", "Local Address if real stack is used (optional)", localAddress);

  cmd.Parse (argc, argv);
  
  UpperCase (realStack);
  UpperCase (realTime);

  openResultFile(resultFile);

  if (realStack == "YES" || realTime == "YES")
    {
       GlobalValue::Bind ("SimulatorImplementationType",
                         StringValue ("ns3::RealtimeSimulatorImpl"));
    }

  // Enable Logs
  LogComponentEnable ("SimulatorMain", LOG_LEVEL_ALL);
  LogComponentEnable ("LSRoutingProtocol", LOG_LEVEL_ALL);
  LogComponentEnable ("UdpTransportSocketImpl", LOG_LEVEL_ERROR);
  LogComponentEnable ("TcpTransportSocketImpl", LOG_LEVEL_ERROR);
  LogComponentEnable ("Ipv4GlobalRouting", LOG_LEVEL_ALL);
    
  // Create SimulatorMain
  SimulatorMain simulatorMain;

  // Read topology
  Ptr<TopologyReader> topologyReader = 0;
  TopologyReaderHelper topoHelper;

  NodeContainer nodeContainer, realNodeContainer;
  NetDeviceContainer* ndc = NULL;
  uint32_t totalLinks = 0, totalNodes = 0;
  Ipv4InterfaceContainer* ipic = NULL;
  NodeContainer* nc = NULL;

  if (realStack != "YES")
    {  
      std::map<uint32_t, Ptr<Node> > nodeMap;

      topoHelper.SetFileName (topologyFile);
      topoHelper.SetFileType ("Inet");
      topologyReader = topoHelper.GetTopologyReader ();

      if (topologyReader != 0)
        {
          nodeContainer = topologyReader->Read ();
        }
      if (topologyReader->LinksSize () == 0)
        {
          NS_FATAL_ERROR ("Unable to read/parse topology file");
          return -1;
        }

      // Create nodes
      NS_LOG_INFO ("Installing internet stack..");
      InternetStackHelper internetStack;

      // Routing helper
      LSRoutingHelper lsRouting;
      DVRoutingHelper dvRouting;

      Ipv4ListRoutingHelper listRoutingHelper;

      UpperCase (routingProtocol);
      if (routingProtocol == LS_MODULE_NAME)
        {
          listRoutingHelper.Add (lsRouting, 0);
        }
      else if (routingProtocol == DV_MODULE_NAME)
        {
          listRoutingHelper.Add (dvRouting, 0);
        }
      else if (routingProtocol == "ANY")
        {
          listRoutingHelper.Add (dvRouting, 0);
          listRoutingHelper.Add (lsRouting, 10);
        }
      else if (routingProtocol == "NS3")
        {
          Ipv4StaticRoutingHelper staticRouting;
          OlsrHelper olsrRouting;
          //Ipv4GlobalRoutingHelper globalRouting;
          listRoutingHelper.Add (staticRouting, 0);
          listRoutingHelper.Add (olsrRouting, -10);
          //listRoutingHelper.Add (globalRouting, -10);
        }
      else 
        {
          NS_LOG_ERROR ("Routing protocol not selected! Exiting...");
          return -1;
        }
      
      NS_LOG_INFO ("Routing protocol: " << routingProtocol);
      internetStack.SetRoutingHelper (listRoutingHelper);
      internetStack.Install (nodeContainer);

      NS_LOG_INFO ("Assigning IP addresses to nodes...");
      Ipv4AddressHelper address;
      address.SetBase ("10.0.0.0", "255.255.255.0");

      totalLinks = topologyReader->LinksSize ();
      totalNodes = nodeContainer.GetN ();

      NS_LOG_INFO ("Creating node containers... Nodes : " << totalNodes);
      nc = new NodeContainer[totalLinks];
      TopologyReader::ConstLinksIterator iter;
      int num = 0;
      for (iter = topologyReader->LinksBegin (); iter != topologyReader->LinksEnd(); iter++, num++)
        {
          std::istringstream fromName(iter->GetFromNodeName ());
          std::istringstream toName (iter->GetToNodeName ());
          uint32_t from, to;
          fromName >> from;
          toName >> to;
          NS_LOG_INFO ("Adding Link From: " << from << " To: " << to);
          nodeMap.insert (std::make_pair (from, iter->GetFromNode()));
          nodeMap.insert (std::make_pair (to, iter->GetToNode()));
          nc[num] = NodeContainer (iter->GetFromNode (), iter->GetToNode ());
        }

      // Create real node container
      for (uint32_t i = 0 ; i < totalNodes ; i++)
        {
          std::map<uint32_t, Ptr<Node> >::iterator iter = nodeMap.find(i);
          if (iter != nodeMap.end())
            {
              Ptr<Node> node = iter->second;
              realNodeContainer.Add (node);
            }
          else
            {
              NS_FATAL_ERROR ("Corrupt node container!");
            }
        }

      NS_LOG_INFO ("Creating network device containers... Links : " << totalLinks);
      ndc = new NetDeviceContainer[totalLinks];
      PointToPointHelper p2p;
      for (uint32_t i = 0 ; i < totalLinks ; i++)
        {
          p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));
          p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
          ndc[i] = p2p.Install (nc[i]);
        }

      // Create subnets for each couple of nodes
      ipic = new Ipv4InterfaceContainer[totalLinks];
      for (uint32_t i = 0 ; i < totalLinks ; i++)
        {
          ipic[i] = address.Assign (ndc[i]);
          address.NewNetwork ();
        } 

      NS_LOG_INFO ("Assigning Main IP addresses...");
      // Assign main ip-address(es)
      for (uint32_t i = 0 ; i < totalNodes ; i++)
        {
          bool mainSet = false;
          Ptr<Ipv4> ipv4 = realNodeContainer.Get(i)->GetObject<Ipv4> ();
          for (uint32_t j = 0 ; j < ipv4->GetNInterfaces () ; j++)
            {
              Ipv4Address addr = ipv4->GetAddress (j,0).GetLocal ();
              if (addr == Ipv4Address::GetLoopback ())
                continue;
              // Add to address-node map
              simulatorMain.g_addressNodeMap.insert (std::make_pair(addr, i));
              if (mainSet == false)
                {
                  mainSet = true;
                  Ipv4Address mainAddress = addr;
                  simulatorMain.g_nodeAddressMap.insert (std::make_pair (i, mainAddress)); 
                  NS_LOG_INFO ("Node: " << i << " Main Address: " << mainAddress);
                  //Get Routing Protocol
                  Ptr<Ipv4ListRouting> listRouting = DynamicCast<Ipv4ListRouting> (ipv4->GetRoutingProtocol ());
                  if (!listRouting)
                    {
                      continue;
                    }
                  uint32_t protocols = listRouting->GetNRoutingProtocols ();
                  for (uint32_t k = 0 ; k < protocols ; k++)
                    {
                      int16_t priority;
                      Ptr<LSRoutingProtocol> lsRouting = DynamicCast<LSRoutingProtocol> (listRouting->GetRoutingProtocol (k, priority));
                      if (lsRouting)
                        {
                          std::ostringstream nodeId;
                          nodeId << i;
                          lsRouting->SetNodeId (nodeId.str ());
                          lsRouting->SetModuleName (LS_MODULE_NAME);
                          lsRouting->SetMainInterface (j);
                          continue;
                        }
                      Ptr<DVRoutingProtocol> dvRouting = DynamicCast<DVRoutingProtocol> (listRouting->GetRoutingProtocol (k, priority));
                      if (dvRouting)
                        {
                          std::ostringstream nodeId;
                          nodeId << i;
                          dvRouting->SetNodeId (nodeId.str ());
                          dvRouting->SetModuleName (DV_MODULE_NAME);
                          dvRouting->SetMainInterface (j);
                          continue;
                        }
                    }
                }
            }
        }

      // Assign node-address map
      for (uint32_t i = 0 ; i < totalNodes ; i++)
        {      
          Ptr<Ipv4> ipv4 = realNodeContainer.Get(i)->GetObject<Ipv4> ();
          //Get Routing Protocol
          Ptr<Ipv4ListRouting> listRouting = DynamicCast<Ipv4ListRouting> (ipv4->GetRoutingProtocol ());
          if (!listRouting)
            {
              continue;
            }
          uint32_t protocols = listRouting->GetNRoutingProtocols ();
          for (uint32_t k = 0 ; k < protocols ; k++)
            {
              int16_t priority;
              Ptr<LSRoutingProtocol> lsRouting = DynamicCast<LSRoutingProtocol> (listRouting->GetRoutingProtocol (k, priority));
              if (lsRouting)
                {
                  lsRouting->SetNodeAddressMap (simulatorMain.g_nodeAddressMap);
                  lsRouting->SetAddressNodeMap (simulatorMain.g_addressNodeMap);
                  continue;
                }
              Ptr<DVRoutingProtocol> dvRouting = DynamicCast<DVRoutingProtocol> (listRouting->GetRoutingProtocol (k, priority));
              if (dvRouting)
                {
                  dvRouting->SetNodeAddressMap (simulatorMain.g_nodeAddressMap);
                  dvRouting->SetAddressNodeMap (simulatorMain.g_addressNodeMap);
                  continue;
                }
            }
        }

    }
  else 
    {
      // Install real-stack
      // Find local address
      Ipv4Address local;
      if (localAddress == "")
      {
        char name[80];
        gethostname (name, 80);
        std::string nodeName = std::string (name);
        nodeName = nodeName + std::string (argv[4]);
        struct hostent *hp;
        struct in_addr **addr_list;
        NS_LOG_INFO ("Node Name: " << std::string (name) );
        hp = gethostbyname (name);
        addr_list = (struct in_addr**)hp->h_addr_list;
        for (int i = 0; addr_list[i] != NULL; i++)
        {
          NS_LOG_INFO ("Available addresses: " << Ipv4Address (inet_ntoa (*addr_list[i])));    
        }
        for (int i = 0; addr_list[i] != NULL; i++)
          {
             if (Ipv4Address (inet_ntoa (*addr_list[i])) != Ipv4Address::GetLoopback() ) 
                {
                  // hp->addr_list[i] is a non-loopback address
                  // bcopy (hp->h_addr, (char *) &sin.sin_addr, hp->h_length);
                  local = Ipv4Address (inet_ntoa (*addr_list[i]));
                  NS_LOG_INFO ("localAddress: " << local);
                }
          }
      }
      else
      {
        local = Ipv4Address (localAddress.c_str());
      }

      NS_LOG_ERROR ("Creating Node...");
      realNodeContainer.Create (1);
      NS_LOG_INFO ("Installing L4-Platform");
      L4PlatformHelper platform;
      platform.Install (realNodeContainer);
      Ptr<L4Device> l4Device = DynamicCast<L4Device> (realNodeContainer.Get(0)->GetDevice(0));
      totalNodes = 0;
      totalLinks = 0;
    }


  NS_LOG_INFO ("Installing TestApp Application...");

  TestAppHelper appHelper = TestAppHelper ();
  ApplicationContainer apps = appHelper.Install (realNodeContainer);
  for (uint32_t i = 0 ; i < totalNodes ; i++)
    {
      Ptr<TestApp> application = realNodeContainer.Get(i)->GetApplication(0)->GetObject<TestApp> ();
      application->SetNodeAddressMap (simulatorMain.g_nodeAddressMap);
      application->SetAddressNodeMap (simulatorMain.g_addressNodeMap);
      std::ostringstream str;
      str << i;
      application->SetNodeId (str.str());
      application->SetModuleName (APP_MODULE_NAME);
    }
  apps.Start (MilliSeconds (0));

  NS_LOG_INFO ("Creating script/command handler...");  
  // Start simulator main
  simulatorMain.Start (scriptFile, realNodeContainer, ndc, totalNodes, totalLinks);

  // Create the animation object and configure for specified output
  AnimationInterface anim;
  if (!animFile.empty ())
    {
      anim.SetOutputFile (animFile);
      anim.StartAnimation ();
    }

  // Run the simulation
  NS_LOG_INFO ("Running Simulation...");
  Simulator::Run ();
  Simulator::Destroy ();

  if (ipic)
    delete[] ipic;
  if (ndc)
    delete[] ndc;
  if (nc)
    delete[] nc;

  closeResultFile();

  NS_LOG_INFO ("End of Simulation.");
  return 0;
}

