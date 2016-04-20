/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
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
 *
 */

#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/packet.h"
#include "ns3/abort.h"
#include "ns3/inet-socket-address.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/trace-source-accessor.h"
#include "udp-transport-socket-impl.h"
#include "sys/types.h"
#include <sys/socket.h>
#include "fcntl.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <limits>
#include "l4-device.h"
#include "ns3/ipv4.h"

NS_LOG_COMPONENT_DEFINE ("UdpTransportSocketImpl");

namespace ns3 {

static const uint32_t MAX_IPV4_UDP_DATAGRAM_SIZE = 65507;

TypeId
UdpTransportSocketImpl::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::UdpTransportSocketImpl")
    .SetParent<Socket> ()
    .AddConstructor<UdpTransportSocketImpl> ()
    ;
  return tid;
}

UdpTransportSocketImpl::UdpTransportSocketImpl ()
{
  NS_LOG_FUNCTION_NOARGS ();
  m_socket = 0;
  if ((m_socket = socket (PF_INET, SOCK_DGRAM, 0)) == -1)
  {
    NS_ABORT_MSG ("Could not create UDP socket");
  }
  fcntl (m_socket, F_SETFL, O_NONBLOCK);
  NS_LOG_INFO("UdpTransportSocketImpl::UdpTransportSocketImpl : Creating Socket : " << m_socket);
  m_packetBuffer = new uint8_t[65536];
}

UdpTransportSocketImpl::~UdpTransportSocketImpl ()
{
  NS_LOG_FUNCTION_NOARGS ();
  delete [] m_packetBuffer;
  m_packetBuffer = 0; 
}

enum Socket::SocketErrno
UdpTransportSocketImpl::GetErrno (void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  return m_errno;
}

Ptr<Node>
UdpTransportSocketImpl::GetNode (void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  return m_node;
}

int
UdpTransportSocketImpl::Bind (void)
{
  NS_LOG_FUNCTION_NOARGS ();
  struct sockaddr_in sin;
  bzero ((char*) &sin, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = INADDR_ANY;
  sin.sin_port = htons (0);
  if (bind (m_socket, (struct sockaddr *)&sin, sizeof(sin)) == -1)
  {
    NS_ABORT_MSG ("Bind failed!");
  }
  //Send this request to transport thread
  m_l4Device->AddReadFd (m_socket, this);
  return 0;
}

int 
UdpTransportSocketImpl::Bind (const Address &address)
{
  NS_LOG_FUNCTION (this << address);

  if (!InetSocketAddress::IsMatchingType (address))
    {
      NS_LOG_ERROR ("Not IsMatchingType");
      m_errno = ERROR_INVAL;
      return -1;
    }
  InetSocketAddress transport = InetSocketAddress::ConvertFrom (address);
  Ipv4Address ipv4 = transport.GetIpv4 ();
  uint16_t port = transport.GetPort ();
  struct sockaddr_in sin;
  bzero ((char *) &sin, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = htonl(ipv4.Get());
  sin.sin_port = htons (port);
  if (bind (m_socket, (struct sockaddr *)&sin, sizeof(sin)) < 0)
  {
    NS_ABORT_MSG ("Bind failed!");
  }
  //Send this request to transport thread
  m_l4Device->AddReadFd (m_socket, this);
  return 0;
}

int 
UdpTransportSocketImpl::ShutdownSend (void)
{
  NS_LOG_FUNCTION_NOARGS ();
  return 0;
}

int 
UdpTransportSocketImpl::ShutdownRecv (void)
{
  NS_LOG_FUNCTION_NOARGS ();
  return 0;
}

int
UdpTransportSocketImpl::Close (void)
{
  NS_LOG_FUNCTION_NOARGS ();
  m_l4Device -> CloseFd (m_socket);
  return 0;
}

int
UdpTransportSocketImpl::Connect(const Address & address)
{
  NS_LOG_FUNCTION (this << address);
  InetSocketAddress transport = InetSocketAddress::ConvertFrom (address);
  m_defaultAddress = transport.GetIpv4 ();
  m_defaultPort = transport.GetPort ();

  return 0;
}

int 
UdpTransportSocketImpl::Listen (void)
{
  return -1;
}

int 
UdpTransportSocketImpl::Send (Ptr<Packet> p, uint32_t flags)
{
  NS_LOG_FUNCTION (this << p << flags);
  SendTo (p, flags, InetSocketAddress (m_defaultAddress, m_defaultPort));
  return 0;
}

// XXX maximum message size for UDP broadcast is limited by MTU
// size of underlying link; we are not checking that now.
uint32_t
UdpTransportSocketImpl::GetTxAvailable (void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  // No finite send buffer is modelled, but we must respect
  // the maximum size of an IP datagram (65535 bytes - headers).
  return MAX_IPV4_UDP_DATAGRAM_SIZE;
}

int 
UdpTransportSocketImpl::SendTo (Ptr<Packet> p, uint32_t flags, const Address &address)
{
  NS_LOG_FUNCTION (this << p << flags << address);
  InetSocketAddress transport = InetSocketAddress::ConvertFrom (address);
  Ipv4Address ipv4 = transport.GetIpv4 ();
  uint16_t port = transport.GetPort ();
  struct sockaddr_in destAddr;
  destAddr.sin_family = AF_INET;
  destAddr.sin_addr.s_addr = htonl (ipv4.Get());
  destAddr.sin_port = htons (port);
  p->CopyData (m_packetBuffer, p->GetSize ());
  sendto (m_socket, m_packetBuffer, p->GetSize(), 0, (struct sockaddr *)&destAddr, sizeof(destAddr));
  return 0;
}

uint32_t
UdpTransportSocketImpl::GetRxAvailable (void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  return 0;
}

Ptr<Packet>
UdpTransportSocketImpl::Recv (uint32_t maxSize, uint32_t flags)
{
  NS_LOG_FUNCTION (this << maxSize << flags);
  Address fromAddress;
  return RecvFrom (maxSize, flags, fromAddress);
}

Ptr<Packet>
UdpTransportSocketImpl::RecvFrom (uint32_t maxSize, uint32_t flags, 
  Address &fromAddress)
{
  NS_LOG_FUNCTION (this << maxSize << flags);
  Ptr<Packet> packet;
  int len;
  struct sockaddr_in srcAddr;
  socklen_t srcAddrLen = sizeof (srcAddr);
  if ((len = recvfrom (m_socket, m_packetBuffer, 65536, 0, (struct sockaddr *)&srcAddr, &srcAddrLen)) != -1)
  {
      NS_LOG_INFO ("Received packet of len: " << (int) len);
      packet = Create<Packet> ((const uint8_t *) (m_packetBuffer), len);
      fromAddress = InetSocketAddress (inet_ntoa(srcAddr.sin_addr), ntohs (srcAddr.sin_port));  
  }
  //Add ReadFd back
  m_l4Device->AddReadFd (m_socket, this);
  return packet;
}

int
UdpTransportSocketImpl::GetSockName (Address &address) const
{
  NS_LOG_FUNCTION_NOARGS ();
  return 0;
}

void
UdpTransportSocketImpl::BindToNetDevice (Ptr<NetDevice> netdevice)
{
  NS_LOG_FUNCTION (netdevice);
  return;
}

bool 
UdpTransportSocketImpl::SetAllowBroadcast (bool allowBroadcast)
{
  return false;
}

bool 
UdpTransportSocketImpl::GetAllowBroadcast () const
{
  return false;
}

void 
UdpTransportSocketImpl::ForwardUp (Ptr<Packet> packet, Ipv4Address ipv4, uint16_t port)
{
  NS_LOG_FUNCTION (this << packet << ipv4 << port);
}

void
UdpTransportSocketImpl::SetNode (Ptr<Node> node)
{
  m_node = node;
  Ptr<NetDevice> netDevice =  GetNode()->GetDevice(0) ;
  m_l4Device = DynamicCast<L4Device> (netDevice);
}

void
UdpTransportSocketImpl::DataInd (void)
{
  NotifyDataRecv ();
}

void
UdpTransportSocketImpl::DataWriteInd (void)
{
  return;
}

void
UdpTransportSocketImpl::CloseInd ()
{
  return;
}

} //namespace ns3
