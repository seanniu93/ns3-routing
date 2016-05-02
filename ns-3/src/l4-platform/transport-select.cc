/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
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


#include "transport-select.h"
#include "l4-device.h"
#include "ns3/simulator.h"
#include "ns3/log.h"
#include "ns3/abort.h"
#include "l4-device.h"
#include <unistd.h>

NS_LOG_COMPONENT_DEFINE ("TransportSelect");

namespace ns3 {

TransportSelect::TransportSelect (int controlFd, Ptr<NetDevice> netDevice)
{
  m_controlFd = controlFd;
  m_l4Device = netDevice;
}

TransportSelect::TransportSelect ()
{
}

TransportSelect::~TransportSelect ()
{

}
  void
TransportSelect::Run (void)
{
  NS_LOG_INFO ("TransportSelect thread spawned");
  if (m_controlFd == 0)
    NS_ABORT_MSG ("Fatal Error: ControlFd is 0!");


  //Get pointer to real-time simulator
  Ptr<RealtimeSimulatorImpl> impl = DynamicCast<RealtimeSimulatorImpl> (Simulator::GetImplementation ());
  m_rtImpl = GetPointer (impl);

  //Add controlFd to set
  FD_ZERO (&m_readFds);
  FD_ZERO (&m_masterReadFds);
  FD_ZERO (&m_masterWriteFds);
  FD_ZERO (&m_masterExceptionFds); 
  FD_SET (m_controlFd, &m_masterReadFds);
  m_fdMax = m_controlFd;
  NS_LOG_INFO ("Transport Select Control Fd: " << m_controlFd);
  for (;;)
  {
    m_readFds = m_masterReadFds;
    m_writeFds = m_masterWriteFds;
    m_exceptionFds = m_masterExceptionFds;
    NS_LOG_INFO ("Select: m_fdMax: " << (int) m_fdMax);
    if (select (m_fdMax+1, &m_readFds, &m_writeFds, &m_exceptionFds, 0) == -1)
    {
      NS_LOG_INFO("TransportSelect::Run : Call to select failed err : -1");
      continue;
    }
    NS_LOG_INFO ("Select ready...");

    for (int fd=0; fd<=m_fdMax; fd++)
    {
      if (FD_ISSET (fd, &m_readFds))
      {
        NS_LOG_INFO ("fd is set: " << fd);
        if (fd == m_controlFd)
        {
          unsigned char buf[TS_CONTROL_MSG_SZ];
          int len = 0;
          while ((unsigned int) len < TS_CONTROL_MSG_SZ)
          {
            len += recv (fd, (char*)buf, (TS_CONTROL_MSG_SZ-len), 0);
          }
          NS_LOG_INFO("TransportSelect::Run : Received data buf[0] :" << (int) buf[0]);
          int fdRx;
          switch (buf[0])
          {
            case SELECT_ADD:
              ReadInt (&buf[1], fdRx);
              NS_LOG_INFO ("Transort Select Adding New ReadFd " << fdRx);
              FD_SET (fdRx, &m_masterReadFds);
              FD_SET (fdRx, &m_masterExceptionFds);
              if (m_fdMax < fdRx)
              {
                m_fdMax = fdRx;
              }
              break;
            case SELECT_REMOVE:
              ReadInt (&buf[1], fdRx);
              NS_LOG_INFO ("Transort Select Removing ReadFd " << fdRx);
              FD_CLR (fdRx, &m_masterReadFds);
              if (!FD_ISSET (fdRx, &m_masterWriteFds))
              {
                FD_CLR (fdRx, &m_masterExceptionFds);
              }
              break;
            case SELECT_ADD_WRITE:
              ReadInt (&buf[1], fdRx);
              NS_LOG_INFO ("Transort Select Adding New WriteFd " << fdRx);
              FD_SET (fdRx, &m_masterWriteFds);
              FD_SET (fdRx, &m_masterExceptionFds);
              if (m_fdMax < fdRx)
              {
                m_fdMax = fdRx;
              }
              break;
            case SELECT_REMOVE_WRITE:
              ReadInt (&buf[1], fdRx);
              NS_LOG_INFO ("Transort Select Removing WriteFd " << fdRx);
              FD_CLR (fdRx, &m_masterWriteFds);
              if (!FD_ISSET (fdRx, &m_masterReadFds))
              {
                FD_CLR (fdRx, &m_masterExceptionFds);
              }
              break;
            case SELECT_CLOSE:
              ReadInt (&buf[1], fdRx);
              NS_LOG_INFO ("Closing Fd: " << fdRx);
              FD_CLR (fdRx, &m_masterReadFds);
              FD_CLR (fdRx, &m_masterWriteFds);
              FD_CLR (fdRx, &m_masterExceptionFds);
              close (fdRx);
              break;
            case SHUTDOWN:
              ShutDown ();
              return;
            default:
              NS_ABORT_MSG ("Fatal Error!");
          }
        }
        else
        {
          //Interrupt simulator
          NS_LOG_INFO("TransportSelect::Run : Calling  impl->ScheduleRealtimeNowWithContext");
          //Remove Fd for now
          FD_CLR (fd, &m_masterReadFds);
          impl->ScheduleRealtimeNowWithContext (0, MakeEvent (&L4Device::ReadFdReady, DynamicCast<L4Device> (m_l4Device), fd));
        }
      }
      if (FD_ISSET (fd, &m_writeFds))
      {
        //Interrupt simulator
        NS_LOG_INFO("WriteFd is set, TransportSelect::Run : Calling  impl->ScheduleRealtimeNowWithContext");
        //Remove Fd for now
        FD_CLR (fd, &m_masterWriteFds);
        impl->ScheduleRealtimeNowWithContext (0, MakeEvent (&L4Device::WriteFdReady, DynamicCast<L4Device> (m_l4Device), fd));
      }
      if (FD_ISSET (fd, &m_exceptionFds))
      {
        NS_LOG_INFO ("Fd Exception detected: " << fd);
        int valopt;
        socklen_t len;
        len = sizeof (int);
        if (getsockopt (fd, SOL_SOCKET, SO_ERROR, (void *) &valopt, &len) < 0)
        {
          NS_LOG_ERROR("TransportSelect::Could not getsockopt: " << fd);
          continue;
        }
        if (valopt)
        {
          NS_LOG_INFO ("Fd Exception detected, Closing Fd: " << fd);
          FD_CLR (fd, &m_masterReadFds);
          FD_CLR (fd, &m_masterWriteFds);
          FD_CLR (fd, &m_masterExceptionFds);
          close (fd);
          impl->ScheduleRealtimeNowWithContext (0, MakeEvent (&L4Device::ExceptionFd, DynamicCast<L4Device> (m_l4Device), fd));
        }
      }
    }
  }
}

  void 
TransportSelect::ShutDown ()
{
  for (int fd = 0 ; fd <= m_fdMax; fd++)
  {
    if (FD_ISSET (fd, &m_masterReadFds) || FD_ISSET (fd, &m_masterWriteFds))
    {
      FD_CLR (fd, &m_masterReadFds);
      FD_CLR (fd, &m_masterWriteFds);
      FD_CLR (fd, &m_masterExceptionFds);
      close (fd);
    }
  }
  close (m_controlFd);
}

  void      
TransportSelect::ReadInt (unsigned char* buf, int& num)
{
  num = 0;
  for (unsigned int i=0; i<sizeof(int); i++)
  {
    //NS_LOG_INFO ("buf["<<(unsigned int)i<<"]: " << buf[i]);
    num = num | ((int)buf[i] << 8*i); 
  }
  //NS_LOG_ERROR ("ReadInt: " << num);
}

} //namespace ns3
