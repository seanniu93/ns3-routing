/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
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


#include "ns3/comm-log.h"

using namespace ns3;

CommLog::CommLog ()
{
  g_nodeId = "Unknown";
  g_moduleName = "Unknown";
  g_errorVerbose = true;
  g_statusVerbose = true;
  g_trafficVerbose = false;
  g_debugVerbose = false;
}

CommLog::~CommLog ()
{}

void
CommLog::SetTrafficVerbose (bool on)
{ 
  g_trafficVerbose = on;
}

void
CommLog::SetErrorVerbose (bool on)
{ 
  g_errorVerbose = on;
}

void
CommLog::SetDebugVerbose (bool on)
{ 
  g_debugVerbose = on;
}

void
CommLog::SetStatusVerbose (bool on)
{
  g_statusVerbose = on;
}

void
CommLog::SetNodeId (std::string nodeId)
{
  g_nodeId = nodeId;
}


void
CommLog::SetModuleName (std::string moduleName)
{
  g_moduleName = moduleName;
}
