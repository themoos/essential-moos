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

#include "MOOS/libMOOS/MOOSLib.h"
#include "MOOS/libMOOS/Thirdparty/getpot/getpot.h"
#include "MOOSLogger.h"

#include <signal.h>
CMOOSLogger gLogger;

#ifdef _WIN32
BOOL CatchMeBeforeDeath( DWORD fdwCtrlType )
#else
void CatchMeBeforeDeath(int sig) 
#endif
{
  
	gLogger.ShutDown();
    exit(0);
	
  
}


int main(int argc ,char * argv[])
{

	//here we do some command line parsing...
	GetPot cl(argc,argv);

	std::vector<std::string>  nominus = cl.nominus_vector();

	//mission file could be first parameter or after --config
	std::string mission_file = nominus.size()>0 ? nominus[0] : "Mission.moos";
	std::string app_name = nominus.size()>1 ? nominus[1] : "pLogger";

    //set up some control handling
#ifndef _WIN32
	//register a handler for shutdown
	signal(SIGINT, CatchMeBeforeDeath);
	signal(	SIGQUIT, CatchMeBeforeDeath);
	signal(	SIGTERM, CatchMeBeforeDeath);
#else
    if( !SetConsoleCtrlHandler( (PHANDLER_ROUTINE) CatchMeBeforeDeath, TRUE ))
    {
        MOOSTrace("this is bad");
    }
#endif

  
    //GO!
    gLogger.Run(app_name,mission_file,argc,argv);

    return 0;
}
