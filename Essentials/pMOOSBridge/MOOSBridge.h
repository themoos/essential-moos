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
#ifndef MOOSBridgeh
#define  MOOSBridgeh

#include "MOOS/libMOOS/MOOSLib.h"
#include "MOOSCommunity.h"

namespace MOOS
{
/**
 * A class which manages the transmission/sharing of data between communities via tcp and udp
 */
class Bridge : public CMOOSApp
{
public:
    Bridge();
    virtual ~Bridge();

    /**
     * standard OnNewMail
     * @param NewMail - the mail
     * @return
     */
	bool OnNewMail(MOOSMSG_LIST &NewMail);

	/**
	 * standard iterate - this is where the work happens
	 * @return
	 */
	bool Iterate();

	/**
	 * standard CMOOSApp startup - called once  before iterate starts being called
	 * again and again
	 * @return true on success
	 */
	bool OnStartUp();

	/**
	 * called when this MOOSApp connects to a DB. Good place to register interest in variables
	 * @return true on success
	 */
	bool OnConnectToServer();

protected:

	/**
	 * a struct used when adding a new share
	 */
	struct ShareInfo
	{
		std::string _SrcCommunity;
		std::string _SrcCommunityHost;
		std::string _SrcCommunityPort;
		std::string _SrcVarName;
		std::string _DestCommunity;
		std::string _DestCommunityHost;
		std::string _DestCommunityPort;
		std::string _DestVarName;
		bool _UseUDP;
	};

    bool Configure();

    /**
     * called when MOOS comms deliver a erquest for a share to be made on the fly
     * @param sStr - a string containing share details. Example form is
     *  "SrcVarName=DB_CLIENTS,DestCommunity=henry,DestCommunityHost=128.30.24.246,
     *   DestCommunityPort=9201,DestVarName=BAR"
     *
     * @return true on success
     */
    bool HandleDynamicShareRequest(std::string  sStr);

    /**
     * Add a share based on information in ShareInfo struct
     * @param Info
     * @return tru on success
     */
    bool AddShare(ShareInfo Info);

    /**
     * register interest in some variables with the MOOS comms layer
     * @return true on success
     */
    bool RegisterVariables();

    /**
     * Make and return or simply fetch a MOOS community object
     * @param sCommunity name of community
     * @return NULL on failure
     */
//    CMOOSCommunity * GetOrMakeCommunity(const std::string & sCommunity);
    CMOOSCommunity * GetOrMakeCommunity(const std::string &sCommunity, const std::string & sHostMachine);


    /**
     * The wizardry routine which does all the marshalling twixt communities
     * @return true on success
     */
    bool MarshallLoop();

    /**
     * simple question - is the referred to community linked to via UDP?
     * @param Index
     * @return
     */
    bool IsUDPShare(CMOOSCommunity::SP & Index);
    
    void PrintShareInfo(const ShareInfo & SI);


    /**
     * how often to run the bridging routine
     */
    int m_nBridgeFrequency;

    /**
     * true if loopback is wanted
     */
    bool m_bAllowLoopBack;

    /** name of local community  to which we belong*/
    std::string m_sLocalCommunity;

    /**
     * a UDPLink manager
     */
    CMOOSUDPLink m_UDPLink;
    
    /** a collection of shares*/
    typedef std::map<std::string,CMOOSCommunity*> COMMUNITY_MAP;
    COMMUNITY_MAP m_Communities;
    std::set< CMOOSCommunity::SP > m_UDPShares;
};

};

#endif
