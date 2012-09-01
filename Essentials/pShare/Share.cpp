/*
 * Share.cpp
 *
 *  Created on: Aug 26, 2012
 *      Author: pnewman
 */

#include <ifaddrs.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <map>
#include <stdexcept>
#include <string>
#include <iomanip>

#include "MOOS/libMOOS/Utils/MOOSUtilityFunctions.h"
#include "MOOS/libMOOS/Utils/IPV4Address.h"
#include "MOOS/libMOOS/Thirdparty/getpot/getpot.h"
#include "MOOS/libMOOS/Utils/SafeList.h"
#include "MOOS/libMOOS/Utils/ConsoleColours.h"
#include "MOOS/libMOOS/App/MOOSApp.h"

#include "Listener.h"
#include "Share.h"
#include "Route.h"


#define DEFAULT_MULTICAST_GROUP_ADDRESS "224.1.1.11"
#define DEFAULT_MULTICAST_GROUP_PORT 90000
#define MAX_MULTICAST_CHANNELS 256

#define RED MOOS::ConsoleColours::Red()
#define GREEN MOOS::ConsoleColours::Green()
#define NORMAL MOOS::ConsoleColours::reset()

namespace MOOS {


struct Socket {
	MOOS::IPV4Address address;
	int socket_fd;
	struct sockaddr_in sock_addr;
};


class Share::Impl: public CMOOSApp {
public:
	bool OnNewMail(MOOSMSG_LIST & new_mail);
	bool OnStartUp();
	bool Iterate();
	bool OnConnectToServer();
	bool OnCommandMsg(CMOOSMsg & Msg);

protected:

	bool ApplyRoutes(CMOOSMsg & msg);

	bool AddOutputSocket(MOOS::IPV4Address address, bool multicast = true);

	bool AddInputSocket(MOOS::IPV4Address address, bool multicast = true);

	std::vector<std::string>  GetRepeatedConfigurations(const std::string & token);

	bool ProcessRouteConfigurationString(const std::string & configuration_string );

	bool AddRoute(const std::string & src_name,
				const std::string & dest_name,
				MOOS::IPV4Address address,
				bool multicast);

	bool  AddMulticastAliasRoute(const std::string & src_name,
					const std::string & dest_name,
					unsigned int channel_num);

	MOOS::IPV4Address GetAddressFromChannelAlias(unsigned int channel_number);

	void PrintRoutes();

	bool PrintSocketMap();

private:
	//this maps channel number to a multicast socket
	typedef std::map<MOOS::IPV4Address, Socket> SocketMap;
	SocketMap socket_map_;

	//this maps variable name to route
	typedef std::map<std::string, std::list<Route> > RouteMap;
	RouteMap routing_table_;

	//this maps channel number to a listener (with its own thread)
	SafeList<CMOOSMsg > incoming_queue_;
	std::map<MOOS::IPV4Address, Listener*> listeners_;

	//teh address form which we count
	MOOS::IPV4Address base_address_;

};

Share::Share() :_Impl(new Impl)
{
}

Share::~Share()
{
}



bool Share::Impl::AddInputSocket(MOOS::IPV4Address address , bool multicast)
{

	if(listeners_.find(address)!=listeners_.end())
	{
		std::cerr<<	"Error ::Listener already"
					" listening on "
				<<address.to_string()<<std::endl;
		return false;
	}


	//OK looking good, make it
	listeners_[address] = new Listener(incoming_queue_,
			address.host(),
			address.port(),
			multicast);

	//run it
	return listeners_[address]->Run();

}

std::vector<std::string>  Share::Impl::GetRepeatedConfigurations(const std::string & token)
{
	STRING_LIST params;
	m_MissionReader.GetConfiguration( GetAppName(),params);

	STRING_LIST::iterator q;
	std::vector<std::string> results;
	for(q=params.begin(); q!=params.end();q++)
	{
		std::string tok,val;
		m_MissionReader.GetTokenValPair(*q,tok,val);
		if(MOOSStrCmp(tok,token))
		{
			results.push_back(val);
		}
	}
	return results;
}


bool Share::Impl::OnStartUp()
{
	base_address_.set_host  (DEFAULT_MULTICAST_GROUP_ADDRESS);
	base_address_.set_port (DEFAULT_MULTICAST_GROUP_PORT);

	EnableCommandMessageFiltering(true);


	try
	{
		//add default outgoing socket
		MOOS::IPV4Address default_address  = GetAddressFromChannelAlias(0);

		//add default listener
		if(!AddInputSocket(default_address,true))
			return false;

		AddRoute("X","X",MOOS::IPV4Address("127.0.0.1",9010),false);
		AddRoute("X","sadfsadf",MOOS::IPV4Address("127.0.0.1",9011),false);
		AddRoute("X","long_name",MOOS::IPV4Address("localhost",9012),false);
		AddRoute("X","fly_across",MOOS::IPV4Address("oceanai.mit.edu",9012),false);
		AddMulticastAliasRoute("X","X",0);

		AddRoute("Square","Triangle",MOOS::IPV4Address("127.0.0.1",9010),false);
		AddRoute("Square","sadfsadf",MOOS::IPV4Address("161.8.5.1",9011),false);
		AddMulticastAliasRoute("Horse","Equine",3);

		ProcessRouteConfigurationString("src_name = X,dest_name=Z,route=multicast_8");
		ProcessRouteConfigurationString("src_name = X,dest_name=Z,route=161.4.5.6:9000 multicast_21 localhost:9832");

		std::vector<std::string> shares = GetRepeatedConfigurations("Output");

		for(std::vector<std::string>::iterator q=shares.begin();
				q!=shares.end();
				q++)
		{
			ProcessRouteConfigurationString(*q);
		}

		PrintRoutes();
	}
	catch(const std::exception & e)
	{
		std::cerr<<RED<<"OnStartUp::exception "<<e.what()<<NORMAL<< std::endl;
	}

	return true;
}

bool Share::Impl::ProcessRouteConfigurationString(const std::string & configuration_string )
{
	std::string src_name, dest_name, routes;
	MOOSValFromString(src_name,configuration_string,"src_name");

	//default no change in name
	dest_name = src_name;
	MOOSValFromString(dest_name,configuration_string,"dest_name");

	//we do need a route....
	MOOSValFromString(routes,configuration_string,"route");

	while(!routes.empty())
	{
		//look for a space separated list of routes...
		std::string route = MOOSChomp(routes," ");

		if(route.find("multicast_")==0)
		{
			//is this a special multicast one?
			std::stringstream ss(std::string(route,10));
			unsigned int channel_num=0;
			ss>>channel_num;
			if(!ss)
			{
				std::cerr<<RED<<"cannot parse "<<route<<channel_num<<std::endl;
				continue;
			}

			AddMulticastAliasRoute(src_name,dest_name,channel_num);
		}
		else
		{
			AddRoute(src_name,dest_name,route,false);
		}

	}

	return true;
}

bool Share::Impl::Iterate()
{
	if(incoming_queue_.Size()!=0)
	{
		CMOOSMsg new_msg;
		if(incoming_queue_.Pull(new_msg))
		{
			//new_msg.Trace();
			if(!m_Comms.IsRegisteredFor(new_msg.GetKey()))
			{
				m_Comms.Post(new_msg);
			}
		}
	}
	return true;
}

bool Share::Impl::OnNewMail(MOOSMSG_LIST & new_mail)
{
	MOOSMSG_LIST::iterator q;

	//for all mail
	for(q = new_mail.begin();q != new_mail.end();q++)
	{
		//do we need to forward it
		RouteMap::iterator g = routing_table_.find(q->GetKey());
		if(g != routing_table_.end())
		{
			try
			{
				//yes OK - try to do so
				ApplyRoutes(*q);
			}
			catch(const std::exception & e)
			{
				std::cerr <<RED<< "Exception thrown: " << e.what() <<NORMAL<< std::endl;
			}
		}

	}

	return true;
}


bool  Share::Impl::AddMulticastAliasRoute(const std::string & src_name,
				const std::string & dest_name,
				unsigned int channel_num)
{
	MOOS::IPV4Address alias_address = GetAddressFromChannelAlias(channel_num);
	return AddRoute(src_name,dest_name,alias_address,true);
}

bool  Share::Impl::AddRoute(const std::string & src_name,
				const std::string & dest_name,
				MOOS::IPV4Address address,
				bool multicast)
{

	SocketMap::iterator mcg = socket_map_.find(address);
	if (mcg == socket_map_.end())
	{
		if(!AddOutputSocket(address,multicast))
			return false;
	}

	Register(src_name, 0.0);

	//finally add this to our routing table
	Route route;
	route.dest_name = dest_name;
	route.src_name = src_name;
	route.dest_address = address;
	route.multicast = multicast;
	routing_table_[src_name].push_back(route);

	return true;
}


bool Share::Impl::OnConnectToServer()
{
	RouteMap::iterator q;

	for(q=routing_table_.begin();q!=routing_table_.end();q++)
	{
		Register(q->first, 0.0);
	}
	return true;
}

bool Share::Impl::OnCommandMsg(CMOOSMsg & Msg)
{
	std::string cmd;
	MOOSValFromString(cmd,Msg.GetString(),"cmd");

	if(MOOSStrCmp("output",cmd))
	{
		return ProcessRouteConfigurationString(Msg.GetString());
	}
	else if(MOOSStrCmp("input",cmd))
	{

	}
	return true;
}

bool Share::Impl::PrintSocketMap()
{
	SocketMap::iterator q;
	std::cerr<<"socket_map_:\n";
	for(q = socket_map_.begin();q!=socket_map_.end();q++)
	{
		std::cerr<<" "<<q->first.to_string() <<" -> "<<q->second.socket_fd<<std::endl;
	}
	return true;
}

void Share::Impl::PrintRoutes()
{
	RouteMap::iterator q;
	std::cout<<std::setiosflags(std::ios::left);
	for(q = routing_table_.begin();q!=routing_table_.end();q++)
	{
		std::list<Route> & routes = q->second;
		std::cout<<"routing for \""<< q->first<<"\""<<std::endl;
		std::list<Route>::iterator p;
		for(p = routes.begin();p!=routes.end();p++)
		{
			Route & route = *p;
			std::cout<<"  --> "<<std::setw(20)<<route.dest_address.to_string()<<" as "<<std::setw(10)<<route.dest_name;
			if(route.multicast)
				std::cout<<" [multicast]";
			else
				std::cout<<" [udp]";
			std::cout<<std::endl;
		}

	}
}

bool Share::Impl::ApplyRoutes(CMOOSMsg & msg)
{
	//do we know how to route this? double check
	RouteMap::iterator g = routing_table_.find(msg.GetKey());
	if(g == routing_table_.end())
		throw std::runtime_error("no specified route");

	//we need to find the socket to send via
	std::list<Route> & route_list = g->second;

	std::list<Route>::iterator q;
	for(q = route_list.begin();q!=route_list.end();q++)
	{
		//process every route
		Route & route = *q;
		SocketMap::iterator mcg = socket_map_.find(route.dest_address);
		if (mcg == socket_map_.end()) {
			std::stringstream ss;
			ss << "no output socket for "
					<< route.dest_address.to_string() << std::endl;
			PrintSocketMap();
			throw std::runtime_error(ss.str());
		}

		Socket & relevant_socket = mcg->second;

		//rename here...
		msg.m_sKey = route.dest_name;

		//serialise here
		unsigned int msg_buffer_size = msg.GetSizeInBytesWhenSerialised();
		std::vector<unsigned char> buffer(msg_buffer_size);
		if (!msg.Serialize(buffer.data(), msg_buffer_size))
		{
			throw std::runtime_error("failed msg serialisation");
		}

		//send here
		if (sendto(relevant_socket.socket_fd, buffer.data(), buffer.size(), 0,
				(struct sockaddr*) (&relevant_socket.sock_addr),
				sizeof(relevant_socket.sock_addr)) < 0)
		{
			throw std::runtime_error("failed \"sendto\"");
		}
	}

	return true;

}

MOOS::IPV4Address Share::Impl::GetAddressFromChannelAlias(unsigned int channel_number)
{
	MOOS::IPV4Address address = base_address_;
	address.set_port(channel_number+address.port());
	return address;
}


bool Share::Impl::AddOutputSocket(MOOS::IPV4Address address, bool multicast)
{

	Socket new_socket;
	new_socket.address=address;

	if ((new_socket.socket_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	throw std::runtime_error(
			"AddSocketForOutgoingTraffic() failed to open sender socket");

	int reuse = 1;
	if (setsockopt(new_socket.socket_fd, SOL_SOCKET, SO_REUSEADDR,
		&reuse, sizeof(reuse)) == -1)
	throw std::runtime_error("failed to set resuse socket option");


	int send_buffer_size = 4 * 64 * 1024;
	if (setsockopt(new_socket.socket_fd,
			SOL_SOCKET, SO_SNDBUF,
			&send_buffer_size,
			sizeof(send_buffer_size)) == -1)
	{
		throw std::runtime_error("failed to set size of socket send buffer");
	}

	if(multicast)
	{
		char enable_loop_back = 1;
		if (setsockopt(new_socket.socket_fd, IPPROTO_IP, IP_MULTICAST_LOOP,
			&enable_loop_back, sizeof(enable_loop_back)) == -1)
			throw std::runtime_error("failed to disable loop back");

		char num_hops = 1;
		if (setsockopt(new_socket.socket_fd, IPPROTO_IP, IP_MULTICAST_TTL,
			&num_hops, sizeof(num_hops)) == -1)
			throw std::runtime_error("failed to set ttl hops");
	}


	memset(&new_socket.sock_addr, 0, sizeof (new_socket.sock_addr));
	new_socket.sock_addr.sin_family = AF_INET;

	std::string dotted_ip = MOOS::IPV4Address::GetNumericAddress(new_socket.address.host());

	if(inet_aton(dotted_ip.c_str(), &new_socket.sock_addr.sin_addr)==0)
	{
		throw std::runtime_error("failed to intepret "
				+dotted_ip+" as an ip address");
	}

	//new_socket.sock_addr.sin_addr.s_addr = inet_addr(new_socket.address.ip_num.c_str());
	new_socket.sock_addr.sin_port = htons(new_socket.address.port());

	//finally add it to our collection of sockets
	socket_map_[address] = new_socket;

	return true;
}


void PrintHelpAndExit()
{

}

int Share::Run(int argc,char * argv[])
{

	//here we do some command line parsing...

	GetPot cl(argc,argv);

	if(cl.search(2,"--help","-h"))
		PrintHelpAndExit();

	std::vector<std::string>   nominus_params = cl.nominus_vector();
	std::cerr<<nominus_params.size()<<std::endl;

	std::string moos_name="pShare";
	std::string mission_file = "Mission.moos";

	switch(nominus_params.size())
	{
	case 1:
		moos_name = nominus_params[0];
		break;
	case 2:
		mission_file = nominus_params[0];
		moos_name = nominus_params[1];
		break;
	}

	//look for overloads
	moos_name=  cl.follow(moos_name.c_str(),3,"--name","--alias","-n");
	mission_file =  cl.follow(mission_file.c_str(),3,"--file","--mission-file","-f");

	try
	{
		_Impl->Run(moos_name.c_str(),mission_file.c_str());
	}
	catch(const std::exception & e)
	{
		std::cerr<<"oops: "<<e.what();
	}
	return 0;
}



}//end of MOOS namespace..
