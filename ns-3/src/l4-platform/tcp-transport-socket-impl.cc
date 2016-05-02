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
#include "ns3/tcp-socket-factory.h"
#include "ns3/trace-source-accessor.h"
#include "tcp-transport-socket-impl.h"
#include "sys/types.h"
#include <sys/socket.h>
#include "fcntl.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <limits>
#include "l4-device.h"
#include "ns3/ipv4.h"
#include <errno.h>
#include <sys/ioctl.h>
#ifndef DARWIN
#include <linux/sockios.h>
#endif
#include <errno.h>


NS_LOG_COMPONENT_DEFINE ("TcpTransportSocketImpl");
#define BACKLOG 10
namespace ns3 {


TypeId
TcpTransportSocketImpl::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TcpTransportSocketImpl")
    .SetParent<Socket> ()
    .AddConstructor<TcpTransportSocketImpl> ()
    ;
  return tid;
}

TcpTransportSocketImpl::TcpTransportSocketImpl ()
{
  NS_LOG_FUNCTION_NOARGS ();
  m_socket = 0;
  if ((m_socket = socket (PF_INET, SOCK_STREAM, 0)) == -1)
  {
    NS_ABORT_MSG ("Could not create Tcp socket");
  }
  fcntl (m_socket, F_SETFL, O_NONBLOCK);
  NS_LOG_INFO("TcpTransportSocketImpl::TcpTransportSocketImpl : Creating Socket : " << m_socket);
  m_packetBuffer = new uint8_t[65536];
}

TcpTransportSocketImpl::TcpTransportSocketImpl (int fd, Ipv4Address address, uint16_t port)
{
  m_socket = fd;
  m_defaultAddress = address;
  m_port = port;
  fcntl (m_socket, F_SETFL, O_NONBLOCK);
  m_packetBuffer = new uint8_t[65536];
}

TcpTransportSocketImpl::~TcpTransportSocketImpl ()
{
  NS_LOG_FUNCTION_NOARGS ();
  delete [] m_packetBuffer;
  m_packetBuffer = 0; 
}

enum Socket::SocketErrno
TcpTransportSocketImpl::GetErrno (void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  return m_errno;
}

Ptr<Node>
TcpTransportSocketImpl::GetNode (void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  NS_LOG_INFO("Calling GetNode : m_node :" << m_node);
  return m_node;
}

int
TcpTransportSocketImpl::Bind (void)
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
  return 0;
}

int 
TcpTransportSocketImpl::Bind (const Address &address)
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
  NS_LOG_INFO("Bind Success and Adding Read Fds m_socket :"<< m_socket);
  return 0;
}

int 
TcpTransportSocketImpl::ShutdownSend (void)
{
  NS_LOG_FUNCTION_NOARGS ();
  return 0;
}

int 
TcpTransportSocketImpl::ShutdownRecv (void)
{
  NS_LOG_FUNCTION_NOARGS ();
  return 0;
}

int
TcpTransportSocketImpl::Close (void)
{
  NS_LOG_FUNCTION_NOARGS ();
  NS_LOG_INFO ("Closing socket fd: " << m_socket);
  m_l4Device->CloseFd (m_socket);
  return 0;
}

int
TcpTransportSocketImpl::Connect(const Address & address)
{
  NS_LOG_FUNCTION (this << address);
  InetSocketAddress transport = InetSocketAddress::ConvertFrom (address);
  m_defaultAddress = transport.GetIpv4 ();
  m_defaultPort = transport.GetPort ();
  struct sockaddr_in destAddr;
  destAddr.sin_family = AF_INET;
  destAddr.sin_addr.s_addr = htonl (m_defaultAddress.Get());
  destAddr.sin_port = htons (m_defaultPort);
  if (connect (m_socket, (struct sockaddr *) &destAddr, sizeof(destAddr)) == -1)
  {
    if (errno != EINPROGRESS)
    {
      NS_LOG_ERROR ("Could not connect!");
      NotifyErrorClose ();
      Close ();
      return -1;
    }
  } 
  //Add writeFd
  m_type = TCP_CONNECTING;
  m_l4Device->AddWriteFd (m_socket, this);
  NS_LOG_INFO("Connecting to address :" << m_defaultAddress << " port :" << m_defaultPort);
  return 0;
}

int 
TcpTransportSocketImpl::Listen (void)
{
  m_type = TCP_LISTENER;
  if (listen (m_socket, BACKLOG) == -1)
  {
    NS_LOG_ERROR ("Could not listen!");
    NotifyErrorClose ();
    Close ();
    return -1;
  }
  //Send this request to transport thread
  m_l4Device->AddReadFd (m_socket, this);
  NS_LOG_INFO("Listen Success for m_socket :" << m_socket);
  return 0;
}

int 
TcpTransportSocketImpl::Send (Ptr<Packet> p, uint32_t flags)
{
  NS_LOG_FUNCTION (this << p << flags);
  p->CopyData (m_packetBuffer, p->GetSize ());
  int sent = 0;
  if ((sent = send (m_socket, m_packetBuffer, p->GetSize (), flags)) == -1)
  {
    //Add writeFd back
    m_l4Device->AddWriteFd (m_socket, this);
    return -1;
  }
  //Add WriteFd back
  m_l4Device->AddWriteFd (m_socket, this);
  NS_LOG_INFO("Send packet, fd:" << m_socket <<" bytes: " << (int) sent);
  return sent;
}

uint32_t
TcpTransportSocketImpl::GetTxAvailable (void) const
{
  socklen_t len;
  #ifndef DARWIN
  int sendBuf, txBuf;
  if (getsockopt (m_socket, SOL_SOCKET, SO_SNDBUF, &sendBuf, &len) == -1)
  {
    NS_LOG_ERROR ("TcpTransportSocketImpl::GetTxAvailable: getsockopt failed!");
    std::string errStr = std::string (strerror (errno));
    NS_LOG_ERROR ("TcpTransportSocketImpl::GetTxAvailable errno: " << std::hex << errno << std::dec << " Reason: " << errStr);
    //Fallback to default
    sendBuf = 16384;
  }
  if (ioctl (m_socket, SIOCOUTQ, &txBuf) == -1)
  {
    NS_LOG_ERROR ("TcpTransportSocketImpl::GetTxAvailable: ioctl failed!");
  }
  NS_LOG_INFO("TcpTransportSocketImpl::GetTxAvailable Fd:"<< m_socket << " Space:" << (sendBuf - txBuf));
  return (sendBuf - txBuf);
  #else
  int sendBuf;
  if (getsockopt (m_socket, SOL_SOCKET, SO_NWRITE, &sendBuf, &len) == -1)
  {
    NS_LOG_ERROR ("TcpTransportSocketImpl::GetTxAvailable: getsockopt failed!");
  }
  return sendBuf;
  #endif
}

int 
TcpTransportSocketImpl::SendTo (Ptr<Packet> p, uint32_t flags, const Address &address)
{
  NS_LOG_FUNCTION (this << p << flags << address);
  return Send (p, flags);
}

uint32_t
TcpTransportSocketImpl::GetRxAvailable (void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  int rxBuf;
  #ifndef DARWIN
  if (ioctl (m_socket, SIOCINQ, &rxBuf) == -1)
  {
    NS_LOG_ERROR ("TcpTransportSocketImpl::GetRxAvailable: getsockopt failed!");
  }
  NS_LOG_INFO("TcpTransportSocketImpl::GetRxAvailable :" << rxBuf);
  #else
  socklen_t len;
  if (getsockopt (m_socket, SOL_SOCKET, SO_NREAD, &rxBuf, &len) == -1)
  {
    NS_LOG_ERROR ("TcpTransportSocketImpl::GetRxAvailable: getsockopt failed!");
  }
  return rxBuf;
  #endif
  return rxBuf;
}

Ptr<Packet>
TcpTransportSocketImpl::Recv (uint32_t maxSize, uint32_t flags)
{
  NS_LOG_FUNCTION (this << maxSize << flags);
  int len;
  if ((len = recv (m_socket, m_packetBuffer, maxSize, flags)) == -1)
  {
    NS_LOG_ERROR ("TcpTransportSocketImpl: Recv error, closing socket...");
    NotifyErrorClose ();
    Close ();
    return NULL;
  }
  else if (len == 0)
  {
    NS_LOG_INFO ("TcpTransportSocketImpl: Recv error (Peer Close), closing socket...");
    NotifyNormalClose ();
    Close ();
    return NULL;
  }
  Ptr<Packet> packet = Create<Packet> ((const uint8_t *) (m_packetBuffer), len);
  //Add ReadFd back
  m_l4Device->AddReadFd (m_socket, this);
  NS_LOG_INFO("Recv TCP data, Fd: " << m_socket << " bytes :" << len);
  return packet;
}

Ptr<Packet>
TcpTransportSocketImpl::RecvFrom (uint32_t maxSize, uint32_t flags, 
  Address &fromAddress)
{
  NS_LOG_FUNCTION (this << maxSize << flags);
  Ptr<Packet> packet;
  fromAddress = InetSocketAddress (m_defaultAddress, m_defaultPort);
  return Recv (maxSize, flags);
}

int
TcpTransportSocketImpl::GetSockName (Address &address) const
{
  NS_LOG_FUNCTION_NOARGS ();
  address = InetSocketAddress (m_defaultAddress, m_defaultPort);
  return 0;
}

void
TcpTransportSocketImpl::BindToNetDevice (Ptr<NetDevice> netdevice)
{
  NS_LOG_FUNCTION (netdevice);
  return;
}

bool 
TcpTransportSocketImpl::SetAllowBroadcast (bool allowBroadcast)
{
  return false;
}

bool
TcpTransportSocketImpl::GetAllowBroadcast () const
{
  return false;
}

void 
TcpTransportSocketImpl::ForwardUp (Ptr<Packet> packet, Ipv4Address ipv4, uint16_t port)
{
  NS_LOG_FUNCTION (this << packet << ipv4 << port);
}

void
TcpTransportSocketImpl::SetNode (Ptr<Node> node)
{
  m_node = node;
  Ptr<NetDevice> netDevice =  GetNode()->GetDevice(0) ;
  m_l4Device = DynamicCast<L4Device> (netDevice);
}

void
TcpTransportSocketImpl::DataInd (void)
{
  int fd;
  Address fromAddress;
  struct sockaddr_in clientAddr;
  socklen_t clientAddrSize = sizeof (clientAddr);
  if (m_type == TCP_LISTENER)
  {
    NS_LOG_INFO("TcpTransportSocketImpl: New Connection");
    if (NotifyConnectionRequest (fromAddress))
    {
      if ((fd = accept (m_socket, (struct sockaddr *) &clientAddr, &clientAddrSize)) != -1)
      {
        fromAddress = InetSocketAddress (inet_ntoa(clientAddr.sin_addr), ntohs (clientAddr.sin_port));
        InetSocketAddress transport = InetSocketAddress::ConvertFrom (fromAddress);
        Ptr<Socket> socket = Create<TcpTransportSocketImpl> (fd, transport.GetIpv4(), transport.GetPort ());
        DynamicCast<TcpTransportSocketImpl>(socket) -> SetNode (GetNode ());
        DynamicCast<TcpTransportSocketImpl>(socket)->MakeTrafficSocket ();
        NotifyNewConnectionCreated (socket, fromAddress);
        NS_LOG_INFO("TcpTransportSocketImpl::Connection from Addr :" << transport.GetIpv4 () );
        //AddReadFd back
        m_l4Device->AddReadFd (m_socket, this);
      }
      else
      {
        std::string errStr = std::string (strerror (errno));
        NS_LOG_ERROR ("TcpTransportSocketImpl::DataInd accept() errno: " << std::hex << errno << std::dec << " Reason: " << errStr);
      }
    }
  }
  else if (m_type == TCP_CONNECTING)
  {
    NS_LOG_WARN ("ReadFd Ready while connecting");
  }
  else 
  {
    int valopt;
    socklen_t len;
    len = sizeof (int);
    if (getsockopt (m_socket, SOL_SOCKET, SO_ERROR, (void *) &valopt, &len) < 0)
    {
      NS_LOG_INFO("TcpSocketImpl::DataReadInd Could not getsockopt:" << m_type);
      NotifyErrorClose ();
      Close ();
      return;
    }

    if (valopt)
    {
      NotifyNormalClose ();
      Close ();
      return;
    }
    NS_LOG_INFO("TcpTransportSocketImpl: Notifying Data Receive");
    NotifyDataRecv ();
  }
}

void
TcpTransportSocketImpl::DataWriteInd (void)
{
  int valopt = 0;
  socklen_t len;
  len = sizeof (int);
  if (getsockopt (m_socket, SOL_SOCKET, SO_ERROR, (void *) &valopt, &len) < 0)
  {
    NS_LOG_INFO("TcpSocketImpl::DataWriteInd Could not getsockopt:" << m_type);
    NotifyErrorClose ();
    Close ();
    return;
  }

  if (valopt)
  {
    if (m_type == TCP_CONNECTING)
    {
      NS_LOG_INFO("TcpSocketImpl::DataWriteInd Error valopt m_type:" << m_type << " valopt: " << valopt);
      if (valopt == EHOSTUNREACH)
      {
        NS_LOG_INFO ("Host Unreachable!!");
      }
      NotifyConnectionFailed ();
      Close ();
    }
    else 
    {
      NS_LOG_INFO("TcpSocketImpl::DataWriteInd Normal close:" << m_type);
      NotifyNormalClose ();
      Close ();
    }
    return;
  }
  else 
  {
    if (m_type == TCP_CONNECTING)
    {
      NS_LOG_INFO("TcpSocketImpl::DataWriteInd connection success m_type :" << m_type );
      m_type = TCP_TRAFFIC;
      m_l4Device -> AddReadFd (m_socket, this); 
      //MakeTrafficSocket ();
      NotifyConnectionSucceeded ();
      NotifySend (GetTxAvailable ());
    }
    else
    {
	    NS_LOG_INFO("DataWriteInd : NotifySend");
      NotifySend (GetTxAvailable ());
    }
  }
  return;
}

void
TcpTransportSocketImpl::CloseInd ()
{
  NS_LOG_INFO ("TcpTransportSocketImpl: CloseInd");
  NotifyNormalClose ();
}

void
TcpTransportSocketImpl::MakeTrafficSocket ()
{
  m_type = TCP_TRAFFIC;
  m_l4Device->AddReadFd (m_socket, this);
  m_l4Device->AddWriteFd (m_socket, this);
}

} //namespace ns3
