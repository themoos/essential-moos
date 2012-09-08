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
#define MAX_UDP_SIZE 48*1024

#define RED MOOS::ConsoleColours::Red()
#define GREEN MOOS::ConsoleColours::Green()
#define YELLOW MOOS::ConsoleColours::Yellow()
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
	bool OnCommandMsg(CMOOSMsg  Msg);
	bool Run(const std::string & moos_name, const::std::string & moos_file, int argc, char * argv[]);

protected:

	bool ApplyRoutes(CMOOSMsg & msg);

	bool AddOutputRoute(MOOS::IPV4Address address, bool multicast = true);

	bool AddInputRoute(MOOS::IPV4Address address, bool multicast = true);

	bool PublishSharingStatus();

	std::vector<std::string>  GetRepeatedConfigurations(const std::string & token);

	bool ProcessIOConfigurationString(std::string  configuration_string,bool is_output);

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
	typedef CMOOSApp BASE;

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

const std::string trim(const std::string& pString,
                       const std::string& pWhitespace = " \t")
{
    const size_t beginStr = pString.find_first_not_of(pWhitespace);
    if (beginStr == std::string::npos)
    {
        // no content
        return "";
    }

    const size_t endStr = pString.find_last_not_of(pWhitespace);
    const size_t range = endStr - beginStr + 1;

    return pString.substr(beginStr, range);
}

Share::Share() :_Impl(new Impl)
{
}

Share::~Share()
{
}



bool Share::Impl::AddInputRoute(MOOS::IPV4Address address , bool multicast)
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
			address,
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


bool Share::Impl::Run(const std::string & moos_name, const::std::string & moos_file, int argc, char * argv[])
{
	//here we can add some routes specified on command line...
	//  "./pShare --output 'X->Y multicast_8 multicast_7' 'Z->Q localhost:9000'"
	GetPot cl(argc,argv);

	std::vector<std::string> outputs = cl.nominus_followers(2,"-o","--output");
	for(unsigned int i = 0;i<outputs.size();i++)
	{
		std::cerr<<outputs[i]<<std::endl;
	}

	return BASE::Run(moos_name.c_str(),moos_file.c_str());
}


bool Share::Impl::OnStartUp()
{
	base_address_.set_host  (DEFAULT_MULTICAST_GROUP_ADDRESS);
	base_address_.set_port (DEFAULT_MULTICAST_GROUP_PORT);

	EnableCommandMessageFiltering(true);


	try
	{

/*
		AddRoute("  X   ","X",MOOS::IPV4Address("127.0.0.1",9010),false);
		AddRoute("X","sadfsadf",MOOS::IPV4Address("127.0.0.1",9011),false);
		AddRoute("X","long_name",MOOS::IPV4Address("localhost",9012),false);
		AddRoute("X","fly_across",MOOS::IPV4Address("oceanai.mit.edu",9012),false);
		AddMulticastAliasRoute("X","X",0);
		AddRoute("Square","Triangle",MOOS::IPV4Address("127.0.0.1",9010),false);
		AddRoute("Square","sadfsadf",MOOS::IPV4Address("161.8.5.1",9011),false);
		AddMulticastAliasRoute("Horse","Equine",3);
		ProcessIOConfigurationString("src_name =X,dest_name=Z,route=multicast_8",true);
		ProcessIOConfigurationString("src_name =X,dest_name=Z,route=161.4.5.6:9000&multicast_21&localhost:9832",true);
		ProcessIOConfigurationString("route=multicast_21&localhost:9833&multicast_3",false);
		*/

		std::vector<std::string> outputs = GetRepeatedConfigurations("Output");
		for(std::vector<std::string>::iterator q=outputs.begin();
				q!=outputs.end();
				q++)
		{
			ProcessIOConfigurationString(*q,true);
		}

		std::vector<std::string> inputs = GetRepeatedConfigurations("Input");
		for(std::vector<std::string>::iterator q=inputs.begin();
				q!=inputs.end();
				q++)
		{
			ProcessIOConfigurationString(*q,false);
		}


		PrintRoutes();
	}
	catch(const std::exception & e)
	{
		std::cerr<<RED<<"OnStartUp::exception "<<e.what()<<NORMAL<< std::endl;
	}

	return true;
}




bool Share::Impl::ProcessIOConfigurationString(std::string  configuration_string, bool is_output )
{
	std::string src_name, dest_name, routes;

	MOOSRemoveChars(configuration_string, " ");

	if(is_output)
	{
		if(!MOOSValFromString(src_name,configuration_string,"src_name"))
			throw std::runtime_error("ProcessIOConfigurationString \"src_name\" is a required field");

		//default no change in name
		dest_name = src_name;
		MOOSValFromString(dest_name,configuration_string,"dest_name");
	}

	//we do need a route....
	if(!MOOSValFromString(routes,configuration_string,"route"))
		throw std::runtime_error("ProcessIOConfigurationString \"route\" is a required field");


	while(!routes.empty())
	{
		//look for a space separated list of routes...
		std::string route = MOOSChomp(routes,"&");

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

			if(is_output)
			{
				if(!AddMulticastAliasRoute(src_name,dest_name,channel_num))
					return false;
			}
			else
			{
				if(!AddInputRoute(GetAddressFromChannelAlias(channel_num),true))
					return false;
			}

		}
		else
		{
			MOOS::IPV4Address route_address(route);

			if(is_output)
			{
				if(!AddRoute(src_name,dest_name,route_address,false))
					return false;
			}
			else
			{
				if(!AddInputRoute(route_address,false))
				{
					return false;
				}
			}
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
	PublishSharingStatus();
	return true;
}

bool Share::Impl::PublishSharingStatus()
{
	static double last_time = MOOS::Time();
	if(MOOS::Time()-last_time<1.0)
		return true;

	std::stringstream sso;

	//Output = X->Y@165.45.3.61:9000-[udp] & Z@165.45.3.61.2
	RouteMap::iterator q;
	for(q = routing_table_.begin();q!=routing_table_.end();q++)
	{
		if(q!=routing_table_.begin())
			sso<<", ";
		std::list<Route> & routes = q->second;
		sso<<q->first<<"->";
		std::list<Route>::iterator p;
		for(p = routes.begin();p!=routes.end();p++)
		{
			if(p!=routes.begin())
				sso<<" & ";
			Route & route = *p;
			sso<<route.dest_name<<":"<<route.dest_address.to_string();
			if(route.multicast)
			{
				sso<<":multicast_"<<route.dest_address.port()-base_address_.port();
			}
			else
			{
				sso<<":udp";
			}
		}
	}

	std::stringstream ssi;

	std::map<MOOS::IPV4Address, Listener*>::iterator t;
	for(t = listeners_.begin();t!=listeners_.end();t++)
	{
		if(t!=listeners_.begin())
			ssi<<",";
		ssi<<t->first.to_string();
		if(t->second->multicast())
		{
			ssi<<":multicast_"<<t->second->port()-base_address_.port();
		}
		else
		{
			ssi<<":udp";
		}
	}


	last_time = MOOS::Time();

	Notify(GetAppName()+"_OUTPUT_SUMMARY",sso.str());
	Notify(GetAppName()+"_INPUT_SUMMARY",ssi.str());

	//std::cerr<<sso.str()<<std::endl;
	//std::cerr<<ssi.str()<<std::endl;

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
		if(!AddOutputRoute(address,multicast))
			return false;
	}

	std::string trimed_src_name = trim(src_name);
	std::string trimed_dest_name = trim(dest_name);

	Register(trimed_src_name, 0.0);

	//finally add this to our routing table
	Route route;
	route.dest_name = trimed_dest_name;
	route.src_name = trimed_src_name;
	route.dest_address = address;
	route.multicast = multicast;
	routing_table_[trimed_src_name].push_back(route);

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

bool Share::Impl::OnCommandMsg(CMOOSMsg  Msg)
{
	std::string cmd;
	MOOSValFromString(cmd,Msg.GetString(),"cmd");

	if(MOOSStrCmp("output",cmd))
	{
		if(!ProcessIOConfigurationString(Msg.GetString(),true))
			return false;

		PrintRoutes();
	}
	else if(MOOSStrCmp("input",cmd))
	{
		if(!ProcessIOConfigurationString(Msg.GetString(),false))
			return false;

		PrintRoutes();
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
	std::cout<<"-------------------------------------------------------------------------------"<<std::endl;

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

	std::cout<<"Listening on:\n";
	std::map<MOOS::IPV4Address, Listener*>::iterator t;
	for(t = listeners_.begin();t!=listeners_.end();t++)
	{
		std::cout<<"  <-- "<<std::setw(20)<<t->first.to_string();
		if(t->second->multicast())
		{
			unsigned int channel_num = t->second->port()-base_address_.port();
			std::cout<<"[multicast_"<<channel_num<<"]";
		}
		else
		{
			std::cout<<"[udp]";
		}

		std::cout<<std::endl;
	}

	std::cout<<"-------------------------------------------------------------------------------"<<std::endl;

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

		if(msg_buffer_size>MAX_UDP_SIZE)
		{
			std::cerr<<"Message size exceeded payload size of "<<MAX_UDP_SIZE/1024<<" kB - not forwarding\n";
			return false;
		}

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


bool Share::Impl::AddOutputRoute(MOOS::IPV4Address address, bool multicast)
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

void PrintInterfaceAndExit()
{
	std::cerr<<RED<<"\n--------------------------------------------\n";
	std::cerr<<RED<<"      \"pShare\" Interface Description    \n";
	std::cerr<<RED<<"--------------------------------------------\n";
	std::cerr<<GREEN<<"Publishes:\n\n"<<NORMAL;
	std::cerr<<"   <AppName>_INPUT_SUMMARY\n";
	std::cerr<<"   <AppName>_OUTPUT_SUMMARY\n\n";


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


int Share::Run(int argc,char * argv[])
{

	//here we do some command line parsing...

	GetPot cl(argc,argv);

	//mission file could be first parameter or after --config
	std::string mission_file = cl.get(1,"Mission.moos");
	mission_file = cl("--config", mission_file.c_str());

	//alias could be second parameter or after --alias or after --moos_name
	std::string moos_name = cl.get(2,"pShare");
	moos_name = cl("--alias", moos_name.c_str());
	moos_name = cl("--moos_name", moos_name.c_str());

	if(cl.search("-i"))
		PrintInterfaceAndExit();

	try
	{
		_Impl->Run(moos_name.c_str(),mission_file.c_str(),argc,argv);
	}
	catch(const std::exception & e)
	{
		std::cerr<<"oops: "<<e.what();
	}
	return 0;
}



}//end of MOOS namespace..
