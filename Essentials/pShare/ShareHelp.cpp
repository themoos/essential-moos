/*
 * ShareHelp.cpp
 *
 *  Created on: Sep 8, 2012
 *      Author: pnewman
 */

#include <iostream>
#include "MOOS/libMOOS/Utils/ConsoleColours.h"


#define RED MOOS::ConsoleColours::Red()
#define GREEN MOOS::ConsoleColours::Green()
#define YELLOW MOOS::ConsoleColours::Yellow()
#define NORMAL MOOS::ConsoleColours::reset()

#include "ShareHelp.h"

ShareHelp::ShareHelp() {
	// TODO Auto-generated constructor stub

}

ShareHelp::~ShareHelp() {
	// TODO Auto-generated destructor stub
}
void ShareHelp::PrintConfigurationExampleAndExit()
{
	std::cerr<<RED<<"\n--------------------------------------------\n";
	std::cerr<<RED<<"      \"pShare\" Example Configuration      \n";
	std::cerr<<RED<<"--------------------------------------------\n\n"<<NORMAL;

	std::cerr<<"ProcessConfig =pShare\n"
			"{\n"
			<<YELLOW<<"  //simple forward of X to A on channel 8\n"<<NORMAL<<
			"  output = src_name = X,dest_name=A,route=multicast_8\n\n"

			<<YELLOW<<"  //simple forward of Y to B on a udp address\n"<<NORMAL<<
			"  output = src_name = Y,dest_name = B, route = localhost:9010\n\n"

			<<YELLOW<<"  //simple forward of Z to Z on a named address (no renaming)\n"<<NORMAL<<
			"  output = src_name = Z, route = oceanai.mit.edu:9020\n\n"

			<<YELLOW<<"  //forwarding to a list of places outputs\n"<<NORMAL<<
			"  output = src_name = P,dest_name = H,route=multicast_9 & robots.ox.ac.uk:10200 & 178.4.5.6:9001\n\n"

			<<YELLOW<<"  //setting up an input\n"<<NORMAL<<
			"  input = route = multicast_9\n"
			"  input = route = localhost:9067\n\n"

			<<YELLOW<<"  //setting up lots at once\n"<<NORMAL<<
			"  input = route = localhost:9069 & multicast_9 & multicast_65\n"
			"}\n"<<std::endl;

	exit(0);
}

void ShareHelp::PrintHelpAndExit()
{
	std::cerr<<RED<<"\n--------------------------------------------\n";
	std::cerr<<RED<<"      \"pShare\" Help                       \n";
	std::cerr<<RED<<"--------------------------------------------\n";

	std::cerr<<YELLOW<<"\nGeneral Usage\n\n"<<NORMAL;
	std::cerr<<"pShare MissionFile MOOSName\n"
			"switches:\n"
			"  --config MissionFile  : specify configuration file\n"
			"  --alias MOOSName      : specify MOOS name\n"
			"  -i                    : display interface help\n"
			"  -h, --help            : display this help\n";

	std::cerr<<YELLOW<<"\nExamples:\n\n"<<NORMAL;

	std::cerr<<"a)   \"./pShare\"  (register under default name of pShare, no "
			"configuration all sharing configured online)\n";
	std::cerr<<"b)   \"./pShare special.moos\" (register under default name of pShare)\n";
	std::cerr<<"c)   \"./pShare special.moos\" (register under default name of pShare)\n";
	std::cerr<<"d)   \"./pShare special.moos pShare2\" (register under MOOSName of pShare2)\n";
	std::cerr<<"e)   \"./pShare special.moos --alias pShare2\" (register under MOOSName of pShare2)\n";

	exit(0);
}

void ShareHelp::PrintInterfaceAndExit()
{
	std::cerr<<RED<<"\n--------------------------------------------\n";
	std::cerr<<RED<<"      \"pShare\" Interface Description    \n";
	std::cerr<<RED<<"--------------------------------------------\n";

	std::cerr<<GREEN<<"\nSynopsis:\n\n"<<NORMAL;
	std::cerr<<
			"Transfers data between a connected MOOSDB and udp based connections.\n"
			"Best way to understand this is via an example. A client could be publishing\n"
			"variable X to the DB, pShare can subscribe to that variable, rename it as\n"
			"Y and push send it via UDP to any number of specified address.\n"
			"The addresses need not be simple UDP destinations, pShare also supports \n"
			"multicasting. There are 256 predefined multicast channels available which can \n"
			"be reference with the short hand multicast_N, where N<256. Forwarding data\n"
			"to a multicast channel allows one send to be received by any number of receivers.\n\n"
			"Of course pShare can also receive data on udp / multicast channel and forward it to\n"
			"the local, connect MOOSDB. This is done using the input directive. \n"
			"pShare supports dynamic sharing configuration by subscribing to PSHARE_CMD\n"
			"or <AppName>_CMD if running under and alias.\n";

	std::cerr<<GREEN<<"\nSubscriptions:\n\n"<<NORMAL;
	std::cerr<<"  a) <AppName>_CMD\n\n";
	std::cerr<<YELLOW<<"PSHARE_CMD\n"<<NORMAL;
	std::cerr<<"This variable can be used to dynamically configure sharing at run time\n"
			"It has the following format:\n";




	std::cerr<<GREEN<<"\nPublishes:\n\n"<<NORMAL;
	std::cerr<<"  a) <AppName>_INPUT_SUMMARY\n";
	std::cerr<<"  b) <AppName>_OUTPUT_SUMMARY\n\n";


	std::cerr<<YELLOW<<"PSHARE_OUTPUT_SUMMARY\n"<<NORMAL;
	std::cerr<<"This variable describes the forwarding or sharing currently being undertaken by pShare.\n";
	std::cerr<<"\nIt has the following format:\n ";
	std::cerr<<"  Output = src_name->route1 & route 2, src_name->route1 & route 2....\n";
	std::cerr<<"where a route is a colon delimited tuple\n ";
	std::cerr<<"  dest_name:host_name:port:protocol \n";
	std::cerr<<"example:\n";
	std::cerr<<"  \"Output = X->Y:165.45.3.61:9000:udp & Z:165.45.3.61.2000:multicast_8,K->K:192.168.66.12:3000:udp\"\n";
	std::cerr<<"\n\n";
	std::cerr<<YELLOW<<"PSHARE_INPUT_SUMMARY\n"<<NORMAL;
	std::cerr<<"This variable describes channels and ports on which pShare receives data.\n";
	std::cerr<<"\nIt has the following format:\n ";
	std::cerr<<"  Input = hostname:port:protocol,hostname:port:protocol...\n";
	std::cerr<<"example:\n";
	std::cerr<<"  \"input = localhost:9001:udp , 221.1.1.18:multicast_18\"\n";
	std::cerr<<"\n\n";


	exit(0);
}
