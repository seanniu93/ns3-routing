/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2009 University of Pennsylvania
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

#include "l4-device.h"
#include "ns3/node.h"
#include "ns3/channel.h"
#include "ns3/packet.h"
#include "ns3/ethernet-header.h"
#include "ns3/llc-snap-header.h"
#include "ns3/log.h"
#include "ns3/abort.h"
#include "ns3/boolean.h"
#include "ns3/string.h"
#include "ns3/enum.h"
#include "ns3/ipv4.h"
#include "ns3/simulator.h"
#include "ns3/realtime-simulator-impl.h"
#include "ns3/system-thread.h"

#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <limits>
#include <stdlib.h>
#include "sys/types.h"
#include "fcntl.h"

#include "udp-transport-socket-impl.h"
#include "tcp-transport-socket-impl.h"
#include <unistd.h>
NS_LOG_COMPONENT_DEFINE ("L4Device");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (L4Device);

TypeId
L4Device::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::L4Device")
    .SetParent<NetDevice> ()
    .AddConstructor<L4Device> ()
    ;
  return tid;
}

L4Device::L4Device ()
: m_node (0),
  m_ifIndex (0),
  m_mtu (0)
{
  NS_LOG_FUNCTION_NOARGS ();
  m_fdMap.clear ();
}

L4Device::~L4Device()
{
  NS_LOG_FUNCTION_NOARGS ();
  m_fdMap.clear ();
}

void 
L4Device::SetIfIndex(const uint32_t index)
{
  NS_LOG_FUNCTION_NOARGS ();
  m_ifIndex = index;
}

uint32_t 
L4Device::GetIfIndex(void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  return m_ifIndex;
}

Ptr<Channel> 
L4Device::GetChannel (void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  return 0;
}

void
L4Device::SetAddress (Address address)
{
  NS_LOG_FUNCTION (address);
  m_address = Mac48Address::ConvertFrom (address);
}

Address 
L4Device::GetAddress (void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  return m_address;
}

bool 
L4Device::SetMtu (const uint16_t mtu)
{
  NS_LOG_FUNCTION_NOARGS ();
  m_mtu = mtu;
  return true;
}

uint16_t 
L4Device::GetMtu (void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  return m_mtu;
}


bool 
L4Device::IsLinkUp (void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  return true;
}

void 
L4Device::AddLinkChangeCallback (Callback<void> callback)
{
  NS_LOG_FUNCTION_NOARGS ();
}

bool 
L4Device::IsBroadcast (void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  return true;
}

Address
L4Device::GetBroadcast (void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  return Mac48Address ("ff:ff:ff:ff:ff:ff");
}

bool
L4Device::IsMulticast (void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  return true;
}

Address
L4Device::GetMulticast (Ipv4Address multicastGroup) const
{
 NS_LOG_FUNCTION (this << multicastGroup);
 Mac48Address multicast = Mac48Address::GetMulticast (multicastGroup);
 return multicast;
}

bool 
L4Device::IsPointToPoint (void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  return false;
}

bool 
L4Device::IsBridge (void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  return false;
}

bool 
L4Device::Send (Ptr<Packet> packet, const Address& dst, uint16_t protocol)
{
  NS_LOG_FUNCTION (packet << dst << protocol);
  NS_FATAL_ERROR ("L4Device::Send: You may not call Send on a L4Device directly");
  return false;
}

bool 
L4Device::SendFrom (Ptr<Packet> packet, const Address& src, const Address& dst, uint16_t protocol)
{
  NS_LOG_FUNCTION (packet << src << dst << protocol);
  NS_FATAL_ERROR ("L4Device::Send: You may not call SendFrom on a L4Device directly");
  return false;
}

Ptr<Node> 
L4Device::GetNode (void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  return m_node;
}

void 
L4Device::SetNode (Ptr<Node> node)
{
  NS_LOG_FUNCTION_NOARGS ();
  m_node = node;
}

bool 
L4Device::NeedsArp (void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  return true;
}

void 
L4Device::SetReceiveCallback (NetDevice::ReceiveCallback cb)
{
  NS_LOG_FUNCTION_NOARGS ();
}

void 
L4Device::SetPromiscReceiveCallback (NetDevice::PromiscReceiveCallback cb)
{
  NS_LOG_FUNCTION_NOARGS ();
}

bool
L4Device::SupportsSendFrom () const
{
  NS_LOG_FUNCTION_NOARGS ();
  return true;
}

Address 
L4Device::GetMulticast (Ipv6Address addr) const
{
  NS_LOG_FUNCTION (this << addr);
  return Mac48Address::GetMulticast (addr);
}

void 
L4Device::Start ()
{
  NS_LOG_FUNCTION_NOARGS ();

  //Create a socketpair to notify TransportSelect thread for control messages
  m_selectNotifyFd [0] = -1;
  m_selectNotifyFd [1] = -1;
  if (socketpair (AF_LOCAL, SOCK_DGRAM, 0, m_selectNotifyFd) == -1)
  {
    NS_ABORT_MSG ("Fatal Error: Could not create socketpair ()");
  }
  NS_LOG_INFO("L4Device : Socket Pair created successfully ");
  //Make other end non-blocking for use in select command
  if (fcntl (m_selectNotifyFd [1], F_SETFL, O_NONBLOCK) == -1)
  {
    NS_ABORT_MSG ("Fatal Error: Could not set socket as Non blocking");
  }
  NS_LOG_INFO("L4Device :Creating TransportSelect object with m_selectNotifyFd [1] : " << m_selectNotifyFd [1]);
  //Create TransportSelect Object and pass it listening socket
  m_transportSelect = TransportSelect (m_selectNotifyFd [1], this);

  //Spawn TransportSelect thread
  m_transportSelectThread = Create<SystemThread> (MakeCallback (&TransportSelect::Run, &m_transportSelect));
  m_transportSelectThread-> Start ();
  NS_LOG_INFO("Invoked a System thread to run select");
}

void
L4Device::Stop ()
{
  if (m_transportSelectThread != 0)
  {
    FdOperation (TransportSelect::SHUTDOWN, m_selectNotifyFd[1]);
    m_transportSelectThread -> Join (); 
    close (m_selectNotifyFd[0]);
  }
}

void
L4Device::AddFd (int fd, Ptr<Socket> socket)
{
  FDMap::iterator iterator = m_fdMap.find (fd);
  if (iterator == m_fdMap.end ())
  {
    FDType fdType;
    fdType.m_socket = socket;
    fdType.m_type = FDType::SOCKET;
    m_fdMap.insert (std::make_pair (fd, fdType));
  }
}

void
L4Device::AddGenericFd (int fd,
                        Callback<void, int> dataIndFn,
                        Callback<void, int> dataWriteIndFn,
                        Callback<void, int> closeIndFn)
{
  FDMap::iterator iterator = m_fdMap.find (fd);
  if (iterator == m_fdMap.end ())
  {
    FDType fdType;
    fdType.m_type = FDType::GENERIC;
    fdType.m_dataIndFn = dataIndFn;
    fdType.m_dataWriteIndFn = dataWriteIndFn;
    fdType.m_closeIndFn = closeIndFn;
    m_fdMap.insert (std::make_pair (fd, fdType));
  }
}

void
L4Device::AddReadFd (int fd, Ptr<Socket> socket)
{
  NS_LOG_INFO ("L4Device Adding Read Fd: " << fd);
  FdOperation (TransportSelect::SELECT_ADD, fd);
  //Add to map
  AddFd (fd, socket);
}

void 
L4Device::AddGenericReadFd (int fd,
                        Callback<void, int> dataIndFn,
                        Callback<void, int> dataWriteIndFn,
                        Callback<void, int> closeIndFn)
{
  NS_LOG_INFO ("L4Device Adding Generic Read Fd: " << fd);
  FdOperation (TransportSelect::SELECT_ADD, fd);
  //Add to map
  AddGenericFd (fd, dataIndFn, dataWriteIndFn, closeIndFn);
}

void
L4Device::RemoveReadFd (int fd)
{
  NS_LOG_INFO ("L4Device Remove Read Fd: " << fd);
  FdOperation (TransportSelect::SELECT_REMOVE, fd);
  return;
}

void
L4Device::AddWriteFd (int fd, Ptr<Socket> socket)
{
  NS_LOG_INFO ("L4 Device Adding Write Fd: " << fd);
  FdOperation (TransportSelect::SELECT_ADD_WRITE, fd);
  //Add to map
  AddFd (fd, socket);
}

void 
L4Device::AddGenericWriteFd (int fd,
                        Callback<void, int> dataIndFn,
                        Callback<void, int> dataWriteIndFn,
                        Callback<void, int> closeIndFn)
{
  NS_LOG_INFO ("L4Device Adding Generic Write Fd: " << fd);
  FdOperation (TransportSelect::SELECT_ADD_WRITE, fd);
  //Add to map
  AddGenericFd (fd, dataIndFn, dataWriteIndFn, closeIndFn);
}

void
L4Device::RemoveWriteFd (int fd)
{
  NS_LOG_INFO ("L4 Device Remove Write Fd: " << fd);
  FdOperation (TransportSelect::SELECT_REMOVE_WRITE, fd);
  return;
}

void
L4Device::FdOperation (TransportSelect::TransportSelectOperation op, int fd)
{
   //pack and send
  unsigned char msg[TS_CONTROL_MSG_SZ];
  msg[0] = op;
  for (unsigned int i = 0; i<sizeof(int); i++)
  {
    msg[i+1] = (fd >> 8*i) & 0xff;
  }
NS_LOG_INFO("L4 Device writing FdOperation fd :"<<fd);
  write (m_selectNotifyFd[0], msg, TS_CONTROL_MSG_SZ);
 
}

void
L4Device::ReadFdReady (int fd)
{
  NS_LOG_INFO ("Main thread got signal to read data from fd");
  //Retrieve from map
  FDMap::iterator iterator = m_fdMap.find (fd);
  if (iterator != m_fdMap.end ())
  {
    FDType fdType = (*iterator).second;
    if (fdType.m_type == FDType::SOCKET)
    {
      Ptr<Socket> socket = fdType.m_socket;
      DynamicCast<TransportSocket> (socket) -> DataInd ();
    }
    else if (fdType.m_type == FDType::GENERIC)
    {
      NS_LOG_INFO("Read FD Ready. Giving a call back to " << fd );
      fdType.m_dataIndFn (fd);
    }
  }
  else
  {
    CloseFd (fd);
  }
}
void
L4Device::WriteFdReady (int fd)
{
  NS_LOG_INFO ("Main thread got signal to write data to fd");
  //Retrieve from map
  FDMap::iterator iterator = m_fdMap.find (fd);
  if (iterator != m_fdMap.end ())
  {
    FDType fdType = (*iterator).second;
    if (fdType.m_type == FDType::SOCKET)
    {
      Ptr<Socket> socket = fdType.m_socket;
      DynamicCast<TransportSocket> (socket) -> DataWriteInd ();
    }
    else if (fdType.m_type == FDType::GENERIC)
    {
      fdType.m_dataWriteIndFn (fd);
    }
  }
  else
  {
    CloseFd (fd);
  }
}

void
L4Device::ExceptionFd (int fd)
{
  NS_LOG_INFO ("Main thread got signal about exception in fd: " << fd);
  //Remove from map
  FDMap::iterator iterator = m_fdMap.find (fd);
  if (iterator != m_fdMap.end ())
  {
    FDType fdType = (*iterator).second;
    if (fdType.m_type == FDType::SOCKET)
    {
      Ptr<Socket> socket = fdType.m_socket;
      DynamicCast<TransportSocket> (socket) -> CloseInd ();
    }
    else if (fdType.m_type == FDType::GENERIC)
    {
      fdType.m_closeIndFn (fd);
    }
    m_fdMap.erase (iterator);
  }
}

void 
L4Device::CloseFd (int fd)
{
  //Remove from map
  FDMap::iterator iterator = m_fdMap.find (fd);
  if (iterator != m_fdMap.end ())
  {
    m_fdMap.erase (iterator);
  }
  NS_LOG_INFO ("L4 Device Close Fd: " << fd);
  FdOperation (TransportSelect::SELECT_CLOSE, fd);
}

} // namespace ns3
