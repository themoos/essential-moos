///////////////////////////////////////////////////////////////////////////
//
//   MOOS - Mission Oriented Operating Suite 
//  
//   A suit of Applications and Libraries for Mobile Robotics Research 
//   Copyright (C) 2001-2005 Massachusetts Institute of Technology and 
//   Oxford University. 
//    
//   This software was written by Paul Newman at MIT 2001-2002 and Oxford 
//   University 2003-2005. email: pnewman@robots.ox.ac.uk. 
//      
//   This file is part of a  MOOS Core Component. 
//        
//   This program is free software; you can redistribute it and/or 
//   modify it under the terms of the GNU General Public License as 
//   published by the Free Software Foundation; either version 2 of the 
//   License, or (at your option) any later version. 
//          
//   This program is distributed in the hope that it will be useful, 
//   but WITHOUT ANY WARRANTY; without even the implied warranty of 
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
//   General Public License for more details. 
//            
//   You should have received a copy of the GNU General Public License 
//   along with this program; if not, write to the Free Software 
//   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 
//   02111-1307, USA. 
//
//////////////////////////    END_GPL    //////////////////////////////////
// MOOSBridge.cpp: implementation of the CMOOSBridge class.
//
//////////////////////////////////////////////////////////////////////

#include "MOOSBridge.h"
#include "MOOS/libMOOS/Utils/MOOSUtilityFunctions.h"
#include "MOOSCommunity.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////


namespace MOOS
{

#define DEFAULT_BRIDGE_FREQUENCY 20
#define DEFAULT_UDP_PORT 10000

#define BOUNCE_WITH_GUSTO 0
using namespace std;


Bridge::Bridge()
{
    m_nBridgeFrequency    = DEFAULT_BRIDGE_FREQUENCY;
    m_sLocalCommunity = "#1";
    m_bAllowLoopBack   = false;
}

Bridge::~Bridge()
{
    
}

bool Bridge::OnNewMail(MOOSMSG_LIST & NewMail)
{
	MOOSMSG_LIST::iterator m;
	for(m=NewMail.begin();m!=NewMail.end();m++)
	{
		if(MOOSStrCmp(m->GetKey(),"PMB_REGISTER"))
			HandleDynamicShareRequest(m->GetString());
	}
	return true;
}

bool Bridge::HandleDynamicShareRequest(std::string sStr)
{
	Bridge::ShareInfo SI;

	// Example String :
	// "SrcVarName=DB_CLIENTS,DestCommunity=henry,DestCommunityHost=128.30.24.246,DestCommunityPort=9201,DestVarName=BAR"


	//some defaults
	SI._SrcCommunity = m_sLocalCommunity;
	SI._SrcCommunityHost = "LOCALHOST";
	SI._SrcCommunityPort = "9000";
	SI._UseUDP = true;

	//which could be overridden
	MOOSValFromString(SI._UseUDP,sStr,"UseUDP");
	MOOSValFromString(SI._SrcCommunity,sStr,"SrcCommunity");
	MOOSValFromString(SI._SrcCommunityHost,sStr,"SrcCommunityHost");
	MOOSValFromString(SI._SrcCommunityPort,sStr,"SrcCommunityPort");

	//and these are compulsory
	if(!MOOSValFromString(SI._SrcVarName,sStr,"SrcVarName"))
		return false;

	if(!MOOSValFromString(SI._DestCommunity,sStr,"DestCommunity"))
		return false;

	if(!MOOSValFromString(SI._DestCommunityHost,sStr,"DestCommunityHost"))
		return false;

	if(!MOOSValFromString(SI._DestCommunityPort,sStr,"DestCommunityPort"))
		return false;

	if(!MOOSValFromString(SI._DestVarName,sStr,"DestVarName"))
		return false;

	if(AddShare(SI))
	{
		MOOSTrace("dynamic share successful\n[-- %s --]\n",sStr.c_str());
	}
	else
	{
		MOOSTrace("dynamic share FAIL\n[-- %s --]\n",sStr.c_str());
	}

	return true;
}

bool Bridge::AddShare(ShareInfo SI)
{
	  //look for loopback - not always wanted
	if (MOOSStrCmp(SI._DestCommunityHost, SI._SrcCommunityHost)
		&& MOOSStrCmp(SI._DestCommunityPort, SI._SrcCommunityPort))
	{
		if (m_bAllowLoopBack == false)
		{
			MOOSTrace("\t Ignoring Loop Back - (bridge not built)\n");
			return (false);
		}
	}

	//convert to numeric after format checking
	if (SI._SrcCommunity.empty() || SI._SrcCommunityHost.empty()
			|| SI._SrcCommunityPort.empty())
		return MOOSFail("empty SrcCommunity field");

	if (SI._DestCommunity.empty() || SI._DestCommunityHost.empty()
			|| SI._DestCommunityPort.empty())
		return MOOSFail("empty DestCommunity field");

	long lSrcPort = atoi(SI._SrcCommunityPort.c_str());
	long lDestPort = atoi(SI._DestCommunityPort.c_str());

	//we will force all broadcast address directives to be the same "community" called "ALL"
	if (MOOSStrCmp(SI._DestCommunityHost, "BROADCAST") || MOOSStrCmp(
			SI._DestCommunity, "ALL"))
	{
		//this is trixksy - need to qualify this generic address with the a port so each Bridge can
		//UDP broadcast to multiple addresses
		SI._DestCommunity = "ALL";
		SI._DestCommunityHost = "BROADCAST-" + SI._DestCommunityPort;
	}

	//make two communities (which will be bridged)
	CMOOSCommunity* pSrcCommunity = GetOrMakeCommunity(SI._SrcCommunity,SI._SrcCommunityHost);
	CMOOSCommunity* pDestCommunity = GetOrMakeCommunity(SI._DestCommunity,SI._DestCommunity);

	if (!SI._UseUDP)
	{
		// depending on what kind of share this is we may want to simply specify
		// a UDP end point or start a MOOS client
		// we will register with each DB with a unique name
		std::string sFullyQualifiedMOOSName = m_MissionReader.GetAppName()
				+ "@" + m_sLocalCommunity;

		//for (connecting to) the source community (where messages come from)
		if (!pSrcCommunity->IsMOOSClientRunning()) {
			pSrcCommunity->InitialiseMOOSClient(SI._SrcCommunityHost, lSrcPort,
					sFullyQualifiedMOOSName, m_nBridgeFrequency);
		}

		// for (connecting to) the destination community (where messages go to)
		if (!pDestCommunity->IsMOOSClientRunning()) {
			pDestCommunity->InitialiseMOOSClient(SI._DestCommunityHost,
					lDestPort, sFullyQualifiedMOOSName, m_nBridgeFrequency);
		}
	}
	else
	{
		// MOOSTrace("Setting UDP port for community %s as %s:%d\n",pDestCommunity->GetCommunityName().c_str(),
		// dest_community_host.c_str(),lDestPort);
		if (SI._DestCommunityHost.find("BROADCAST-") != std::string::npos
				&& MOOSStrCmp(SI._DestCommunity, "ALL")) {
			//this is special
			pDestCommunity->SetUDPInfo("255.255.255.255", lDestPort);
		}
		else
		{
			pDestCommunity->SetUDPInfo(SI._DestCommunityHost, lDestPort);
		}
	}

	//populate bridge with variables to be shared (including translation)
	if (pSrcCommunity && pDestCommunity)
	{
		string sVar = MOOSChomp(SI._SrcVarName, ",");
		while (!sVar.empty()) {
			pSrcCommunity->AddSource(sVar);
			CMOOSCommunity::SP Index(sVar, pSrcCommunity->GetCommunityName());
			pDestCommunity->AddSink(Index, MOOSChomp(SI._DestVarName, ","));

			if (SI._UseUDP)
			{
				//we need to store in the Bridge class what variables appearing in our
				//local community we are asked to forward on via UDP to some other
				//commnity
				m_UDPShares.insert(Index);
			}

			//suck another VAR
			sVar = MOOSChomp(SI._SrcVarName, ",");
		}
	}
	return true;
}


bool Bridge::Iterate()
{
  return MarshallLoop();

}

bool Bridge::OnStartUp()
{
	if(!Configure())
		return MOOSFail("MOOSBridge failed to configure itself - probably a configuration block error\n");

	//	if(!RegisterVariables())
	//		return MOOSFail("MOOSBridge failed to register variables\n");
	RegisterVariables(); // change by mikerb

	return true;
}


bool Bridge::OnConnectToServer()
{
  return RegisterVariables();
}

bool Bridge::RegisterVariables()
{
  return m_Comms.Register("PMB_REGISTER", 0);
}




bool Bridge::MarshallLoop()
{
    COMMUNITY_MAP::iterator p,q;
    MOOSMSG_LIST InMail;
    for(p = m_Communities.begin();p!=m_Communities.end();p++)
    {
        CMOOSCommunity* pSrcCommunity = (p->second);
        
        if(pSrcCommunity->Fetch(InMail))
        {
            int nMail = InMail.size();
            for(q = m_Communities.begin();q!=m_Communities.end();q++)
            {
                CMOOSCommunity* pDestCommunity = q->second;
                if(pDestCommunity!=pSrcCommunity)
                {
                    MOOSMSG_LIST::iterator w;
                    for(w = InMail.begin();w!=InMail.end();w++)
                    {
                        
                        CMOOSCommunity::SP Index(w->GetKey(),pSrcCommunity->GetCommunityName() );
                        
                        if(pDestCommunity->WantsToSink(Index))
                        {    
                            //decrement mail count
                            nMail--;
                            
                            CMOOSMsg MsgCopy = *w;
                            MsgCopy.m_sKey = pDestCommunity->GetAlias(Index);
                            if(IsUDPShare(Index))
                            {
                                if(pDestCommunity->HasUDPConfigured())
                                {
#if(BOUNCE_WITH_GUSTO) 
                                    MOOSTrace("UDP posting %s to community %s@%s:%d as %s (source community is %s)\n",
                                              w->GetKey().c_str(),
                                              pDestCommunity->GetCommunityName().c_str(),
                                              pDestCommunity->GetUDPHost().c_str(),
                                              pDestCommunity->GetUDPPort(),                                              
                                              MsgCopy.m_sKey.c_str(),
                                              w->GetCommunity().c_str());
#endif
                                    
                                    //Send via UDP (directed to  single machine and port) - fire and forget...
                                	m_UDPLink.Post(MsgCopy,pDestCommunity->GetUDPHost(),pDestCommunity->GetUDPPort());
									                                }
                                else
                                {
                                    MOOSTrace("cannot send %s via UDP to %s - destination community has no UDP port\n",MsgCopy.m_sKey.c_str(),pDestCommunity->GetFormattedName().c_str());
                                }
                            }
                            else
                            {
                            	pDestCommunity->Post(MsgCopy);
                            }
#if(BOUNCE_WITH_GUSTO)  
                            MOOSTrace("Bouncing %s in %s -> %s on %s t = %f\n",
                                      w->GetKey().c_str(),
                                      pSrcCommunity->GetFormattedName().c_str(),
                                      MsgCopy.GetKey().c_str(),
                                      pDestCommunity->GetFormattedName().c_str(),
                                      MsgCopy.GetTime());
#endif
                        }
                    }
                }
            }
            //here we could look for broadcasts...
        }
    }
    
    
    //have we received any UDP mail? if so it was meant for our own DB
    //remember if UDP is being used there MUST be one MOOSBridge per community
    MOOSMSG_LIST UDPMail;
    
	if(m_UDPLink.Fetch(UDPMail))
    {
        //find our local community
        
        COMMUNITY_MAP::iterator qq = m_Communities.find(m_sLocalCommunity );
        if(qq!=m_Communities.end())
        {
            CMOOSCommunity * pLocalCommunity = qq->second;
            MOOSMSG_LIST::iterator p;
            for(p = UDPMail.begin();p!=UDPMail.end();p++)
            {
#if(BOUNCE_WITH_GUSTO) 
                MOOSTrace("Received %s from %s on UDP  - inserting into %s\n",p->GetKey().c_str(), p->GetCommunity().c_str(),pLocalCommunity->GetFormattedName().c_str());
#endif
                
                //now be careful we aren't looking to subscribing for this mail....now that would be some horrible positive
                //feedback loop! Can alos check as mikerb suggess on source community - it must not
                //be us!
                
                if(!pLocalCommunity->HasMOOSSRegistration(p->GetKey()) && pLocalCommunity->GetCommunityName()!= p->GetCommunity() )
                {
                	pLocalCommunity->Post(*p);
                }
                else
                {
                    
                	MOOSTrace("no way!\n");
                }
            }
        }
        
    }
    
    return true;
}

bool Bridge::IsUDPShare(CMOOSCommunity::SP & Index)
{
    return !m_UDPShares.empty() && m_UDPShares.find(Index)!=m_UDPShares.end();
}

bool Bridge::Configure()
{
    STRING_LIST sParams;
    
    if(!m_MissionReader.GetConfiguration(m_MissionReader.GetAppName(),sParams))
        return MOOSFail("ERROR - Could not find a configuration block called %s \n",m_MissionReader.GetAppName().c_str()) ;
    
    //if user set LOOPBACK = TRUE then both src and destination communities can be identical
    //default is FALSE this means if src=dest then the bridging will be ignored
    m_MissionReader.GetConfigurationParam("LOOPBACK",m_bAllowLoopBack);
    
    
    //capture default file scope settings - maybe useful later
    string sLocalHost = "LOCALHOST";
    string sLocalPort = "9000";
    
    if(!m_MissionReader.GetValue("COMMUNITY",m_sLocalCommunity))
    {
        MOOSTrace("WARNING : Cannot read ::MOOS-scope variable COMMUNITY - assuming %s\n",m_sLocalCommunity.c_str());
    }
    
    if(!m_MissionReader.GetValue("SERVERPORT",sLocalPort))
    {
        MOOSTrace("WARNING :Cannot read ::MOOS-scope variable SERVERPORT - assuming %s\n",sLocalPort.c_str());
    }
    
    if(!m_MissionReader.GetValue("SERVERHOST",sLocalHost))
    {
        MOOSTrace("WARNING :Cannot read ::MOOS-scope variable SERVERHOST - assuming %s\n",sLocalHost.c_str());
    }
    
    //how fast should the bridge operate in Hz (setting this to zero is a special case and
    //makes all registrations with dfPeriod = 0)
    m_nBridgeFrequency = DEFAULT_BRIDGE_FREQUENCY;
    m_MissionReader.GetConfigurationParam("BridgeFrequency",m_nBridgeFrequency);
    
    //loop over every parameter line looking for a SHARE directive
    STRING_LIST::iterator q;
    
    for(q = sParams.begin();q!=sParams.end();q++)
    {
        string sLine = *q;
        //NB is alias's aren't specified the sink name is the source name
        //also you don't need as many alias's as sources...
        //SHARE = COMMUNITYNAME@HOSTNAME:PORT [VAR1,VAR2,VAR3,....] -> COMMUNITYNAME@HOSTNAME:PORT [VarAlias1,....]
        // or using mission file defaults i.e file scope constants
        //SHARE = [VAR1,VAR2,VAR3,....] -> COMMUNITYNAME@HOSTNAME:PORT [VarAlias1,....]
        string sCmd = MOOSChomp(sLine,"=");
        
        if(MOOSStrCmp(sCmd,"SHARE") || MOOSStrCmp(sCmd,"UDPSHARE") )
        {

        	// here comes some tedious syntax parsing
            bool bUDP = MOOSStrCmp(sCmd,"UDPSHARE");
            
            Bridge::ShareInfo SI;

            string sSrc = MOOSChomp(sLine,"->");
            string sDest = sLine;
            
            string sSrcCommunity =m_sLocalCommunity ;
            string sSrcCommunityHost = sLocalHost;
            string sSrcCommunityPort = sLocalPort;
            if(sSrc[0]=='[')
            {
                
                //tell user what we are doing - this is the short-hand set up...
                MOOSTrace("Using abbreviated configuration protocol Source: %s@%s:%s\n",
                          sSrcCommunity.c_str(),
                          sSrcCommunityPort.c_str(),
                          sSrcCommunityHost.c_str());
                
                
                MOOSChomp(sSrc,"[");
            }
            else
            {
                sSrcCommunity = MOOSChomp(sSrc,"@");
                sSrcCommunityHost = MOOSChomp(sSrc,":");            
                sSrcCommunityPort = MOOSChomp(sSrc,"[");
            }
            
            string sVars =MOOSChomp(sSrc,"]"); 
            
            string sDestCommunity = MOOSChomp(sDest,"@");
            string sDestCommunityHost = MOOSChomp(sDest,":");            
            string sDestCommunityPort = MOOSChomp(sDest,"[");
            string sAliases = MOOSChomp(sDest,"]");
            
            SI._SrcVarName  = sVars;      // added by mikerb jan04/12
            SI._DestVarName = sAliases;   // added by mikerb jan04/12

            SI._SrcCommunity = sSrcCommunity;
            SI._SrcCommunityHost = sSrcCommunityHost;
            SI._SrcCommunityPort = sSrcCommunityPort;
            SI._DestCommunity = sDestCommunity;
            SI._DestCommunityHost = sDestCommunityHost;
            SI._DestCommunityPort = sDestCommunityPort;
            SI._UseUDP = bUDP;


            
            //now we can add the share explicitly
            if(!AddShare(SI))
            {
            	MOOSTrace("failed to add a share from a configuration block line\n%s\n",q->c_str());
            	MOOSTrace("correct format is \n");
            	MOOSTrace("SHARE = COMMUNITYNAME@HOSTNAME:PORT [VAR1,VAR2,VAR3,....] -> COMMUNITYNAME@HOSTNAME:PORT\n");
            	continue;
            }

        }
        
    }//end of passing line by line looking for *SHARE directives
    
    ///think about setting up UDP connections....there is one UDP Link per instance of a 
    //MOOSBridge. If poepl wnat UDP bridging they need on pMOOSBridge per community (ie the 
    //toplogy of N communities and just 1 bridge is not allowed
    int nLocalUDPPort = DEFAULT_UDP_PORT;
	if(m_MissionReader.GetConfigurationParam("UDPListen",nLocalUDPPort)   )
    {
        //start the UDP listener
        m_UDPLink.Run(nLocalUDPPort);
        m_Comms.Notify("PMB_UDP_LISTEN", nLocalUDPPort);
    }
	else
	{
        MOOSTrace("warning no UDPListen port specified for local community - outgoing UDP comms only\n");
        
        //passing run with a -1 port means build the socket but don't bind or start a listen thread
        m_UDPLink.Run(-1);
    }


    //ensure we have at least the local MOOS-enabled community in existence - maybe all we want to do is map LocalMOOS->UDP_Out
    CMOOSCommunity * pLocalCommunity  = GetOrMakeCommunity(m_sLocalCommunity,"LOCALHOST");
    
    if(pLocalCommunity!=NULL && !pLocalCommunity->IsMOOSClientRunning())
    {
        //make a connection to the local DB
        pLocalCommunity->InitialiseMOOSClient(sLocalHost, 
                                              atoi(sLocalPort.c_str()),
                                              m_MissionReader.GetAppName()+ "@" + m_sLocalCommunity, // hack by mikerb moderated by pmn
                                              m_nBridgeFrequency);
    }
    
    return true;
}



CMOOSCommunity * Bridge::GetOrMakeCommunity(const string &sCommunity, const string & sHostMachine)
{
    CMOOSCommunity* pCommunity = NULL;
    std::string sKey = sCommunity+"@"+sHostMachine;
    COMMUNITY_MAP::iterator p = m_Communities.find(sKey);
    if(p==m_Communities.end())
    {
        pCommunity = new CMOOSCommunity;
        pCommunity->Initialise(sCommunity);
        m_Communities[sKey] = pCommunity;
    }
    else
    {
        pCommunity = p->second;
    }
    
    return pCommunity;
}


};//namespace MOOS
