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
#include <algorithm>
#include <functional>
#include <MOOS/libMOOS/Thirdparty/getpot/getpot.h>
#include "MOOS/libMOOS/Utils/SafeList.h"
#include "Listener.h"
#include "MOOS/libMOOS/App/MOOSApp.h"


#include "Share.h"

#define DEFAULT_MULTICAST_GROUP_ADDRESS "224.1.1.11"
#define DEFAULT_MULTICAST_GROUP_PORT 90000
#define MAX_MULTICAST_CHANNELS 32

namespace MOOS {

struct Address{
	std::string ip_num;
	int port;
	Address(){};
	Address(const std::string & ip, unsigned int p):ip_num(ip),port(p){};
	bool operator<(const Address & P) const
	{
		if (ip_num<P.ip_num)
			return true;
		if(port<P.port)
			return true;

		return false;
	}
};

struct Socket {
	Address address;
	int socket_fd;
	struct sockaddr_in sock_addr;
};

struct ShareInfo {
	Address dest_address;
	std::string dest_name;
	std::string src_name;
	bool multicast;
};


class Share::Impl: public CMOOSApp {
public:
	bool OnNewMail(MOOSMSG_LIST & new_mail);
	bool OnStartUp();
	bool Iterate();
	bool OnConnectToServer();

private:

	bool ApplyRoutes(CMOOSMsg & msg);

	bool AddOutputSocket(Address address, bool multicast = true);
	bool AddInputSocket(Address address, bool multicast = true);

	std::vector<std::string>  GetRepeatedConfigurations(const std::string & token);

	bool AddRoute(const std::string & src_name,
				const std::string & dest_name,
				Address address,
				bool multicast);

	Address GetAddressAndPort(unsigned int channel_number);

	bool PrintSocketMap();

	//this maps channel number to a multicast socket
	typedef std::map<Address, Socket> SocketMap;
	SocketMap socket_map_;

	//this maps variable name to route
	typedef std::map<std::string, ShareInfo> ShareInfoMap;
	ShareInfoMap routing_table_;

	//this maps channel number to a listener (with its own thread)
	SafeList<CMOOSMsg > incoming_queue_;
	std::map<Address, Listener*> listeners_;

	//teh address form which we count
	Address base_address_;



};

Share::Share() :_Impl(new Impl)
{
}

Share::~Share()
{
}

std::string to_string(const Address & ap)
{
	std::stringstream ss;
	ss<<ap.ip_num<<":"<<ap.port;
	return ss.str();
}

bool Share::Impl::AddInputSocket(Address address , bool multicast)
{

	if(listeners_.find(address)!=listeners_.end())
	{
		std::cerr<<	"Error ::Listener already"
					" listening on "
				<<to_string(address)<<std::endl;
		return false;
	}


	//OK looking good, make it
	listeners_[address] = new Listener(incoming_queue_,
			address.ip_num,
			address.port,
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
	base_address_.ip_num = DEFAULT_MULTICAST_GROUP_ADDRESS;
	base_address_.port =DEFAULT_MULTICAST_GROUP_PORT;


	try
	{
		//add default outgoing socket
		Address default_address  = GetAddressAndPort(0);

		if(!AddOutputSocket(default_address,true))
			return false;

		//add default listener
		if(!AddInputSocket(default_address,true))
			return false;

		//AddRoute("X","X",default_address,true);

		AddRoute("X","X",Address("localhost",9010),false);

		std::vector<std::string> shares = GetRepeatedConfigurations("Share");

		for(std::vector<std::string>::iterator q=shares.begin();
				q!=shares.end();
				q++)
		{
			//src_name=X,dest_name=Y,route=multicast_7
			//src_name=X,dest_name=Y,route=169.45.6.8:9700
			//src_name=V,dest_name=W,route=171.40.9.1:9700,multicast_9

			//AddRoute(*q);
		}
	}
	catch(const std::exception & e)
	{
		std::cerr<<"OnStartUp::exception "<<e.what()<<std::endl;
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
			new_msg.Trace();
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
		ShareInfoMap::iterator g = routing_table_.find(q->GetKey());
		if(g != routing_table_.end()){
			try {
				//yes OK - try to do so
				ApplyRoutes(*q);
			}catch(const std::exception & e){
				std::cerr << "caught an exception reason was: " << e.what() << std::endl;
			}
		}

	}

	return true;
}


bool  Share::Impl::AddRoute(const std::string & src_name,
				const std::string & dest_name,
				Address address,
				bool multicast)
{

	SocketMap::iterator mcg = socket_map_.find(address);
	if (mcg == socket_map_.end())
	{
		std::cerr<<"adding new output socket for "<<to_string(address)<<std::endl;
		AddOutputSocket(address,multicast);
	}

	Register(src_name, 0.0);

	//finally add this to our routing table
	ShareInfo share_info;
	share_info.dest_name = dest_name;
	share_info.src_name = src_name;
	share_info.dest_address = address;
	share_info.multicast = multicast;
	routing_table_[src_name] = share_info;

	std::cerr<<"route added successfully\n";

	return true;
}


bool Share::Impl::OnConnectToServer()
{
	ShareInfoMap::iterator q;

	for(q=routing_table_.begin();q!=routing_table_.end();q++)
	{
		Register(q->first, 0.0);
	}
	return true;
}

bool Share::Impl::PrintSocketMap()
{
	SocketMap::iterator q;
	std::cerr<<"socket_map_:\n";
	for(q = socket_map_.begin();q!=socket_map_.end();q++)
	{
		std::cerr<<" "<<to_string(q->first) <<" -> "<<q->second.socket_fd<<std::endl;
	}
	return true;
}

bool Share::Impl::ApplyRoutes(CMOOSMsg & msg)
{
	//do we know how to route this? double check
	ShareInfoMap::iterator g = routing_table_.find(msg.GetKey());
	if(g == routing_table_.end())
		throw std::runtime_error("no specified route");

	//we need to find the socket to send via
	ShareInfo & share_info = g->second;
	SocketMap::iterator mcg = socket_map_.find(share_info.dest_address);
	if(mcg == socket_map_.end())
	{
		std::stringstream ss;
		ss<<"no output socket for " << to_string(share_info.dest_address)<<std::endl;
		PrintSocketMap();
		throw std::runtime_error(ss.str());
	}

	Socket & relevant_socket = mcg->second;

	//rename here...
	msg.m_sKey= share_info.dest_name;

	//serialise here
	unsigned int msg_buffer_size = msg.GetSizeInBytesWhenSerialised();
	std::vector<unsigned char> buffer(msg_buffer_size);
	if(!msg.Serialize(buffer.data(), msg_buffer_size))
		throw std::runtime_error("failed msg serialisation");

	//send here
	if(sendto(relevant_socket.socket_fd,
			buffer.data(),
			buffer.size(),
			0, (struct sockaddr*)(&relevant_socket.sock_addr),
		sizeof(relevant_socket.sock_addr)) < 0)
	{
		throw std::runtime_error("failed \"sendto\"");
	}

	std::cerr<<"sent "<<buffer.size()<<"bytes"<<std::endl;
	return true;

}

Address Share::Impl::GetAddressAndPort(unsigned int channel_number)
{
	Address address_pair = base_address_;
	address_pair.port+=channel_number;
	return address_pair;
}


bool Share::Impl::AddOutputSocket(Address address, bool multicast)
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
	if (setsockopt(new_socket.socket_fd, SOL_SOCKET, SO_SNDBUF,
		&send_buffer_size, sizeof(send_buffer_size)) == -1)
	throw std::runtime_error("failed to set size of socket send buffer");

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
	new_socket.sock_addr.sin_addr.s_addr = inet_addr(new_socket.address.ip_num.c_str());
	new_socket.sock_addr.sin_port = htons(new_socket.address.port);

	//finally add it to our collection of sockets
	socket_map_[address] = new_socket;

	std::cerr<<to_string(address)<<" has fd "<<new_socket.socket_fd<<std::endl;
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
