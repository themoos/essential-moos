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
#include "MOOS/libMOOS/App/MOOSApp.h"

#include "Share.h"

#define DEFAULT_GROUP_ADDRESS "224.1.1.11"
#define DEFAULT_GROUP_PORT 90000

namespace MOOS {

struct MulticastSocket {
	std::string address;
	int port;
	int socket_fd;
	struct sockaddr_in multicast_addr;
};

struct ShareInfo {
	std::string socket_name;
	std::string dest_name;
	std::string src_name;

};

class Share::Impl: public CMOOSApp {
public:
	bool OnNewMail(MOOSMSG_LIST & new_mail);
	bool AddRoute(const ShareInfo & share_info);
	bool AddRoute(std::string share_specification);
	bool Route(CMOOSMsg & msg);
	bool OnStartUp();

private:
	bool AddMulticastSocketForOutgoingTraffic(const std::string & socket_name,
			const std::string & address, int port);

	typedef std::map<std::string, MulticastSocket> MulticastSocketMap;
	MulticastSocketMap multicast_socket_map_;

	typedef std::map<std::string, ShareInfo> ShareInfoMap;
	ShareInfoMap routing_table_;
};

Share::Share() :_Impl(new Impl)
{
}

Share::~Share()
{
}

bool Share::Impl::OnStartUp()
{
	if(!AddMulticastSocketForOutgoingTraffic("DEFAULT",DEFAULT_GROUP_ADDRESS,DEFAULT_GROUP_PORT))
		return false;

	AddRoute("src_name=X, dest_name=Y ");

	return true;
}

bool Share::Impl::AddRoute(std::string share_specification)
{
	ShareInfo share_info;

	if(!MOOSValFromString(share_info.src_name,share_specification,"src_name"))
		throw std::runtime_error("failed to parse \"src_name\" from configuration string");

	share_info.dest_name = share_info.src_name;
	MOOSValFromString(share_info.dest_name,share_specification,"dest_name");

	share_info.socket_name = "DEFAULT";
	MOOSValFromString(share_info.socket_name,share_specification,"channel_name");

	return AddRoute(share_info);
}

bool Share::Impl::AddRoute(const ShareInfo & share_info)
{
	MulticastSocketMap::iterator mcg = multicast_socket_map_.find(share_info.socket_name);
	if(mcg == multicast_socket_map_.end())
	{
		throw std::runtime_error("no multicast group for channel name " + share_info.socket_name);
	}

	if(multicast_socket_map_.find(share_info.socket_name)==multicast_socket_map_.end())
		throw std::runtime_error("channel called "+share_info.socket_name+" does not exist");

	if(!Register(share_info.src_name, 0.0))
		throw std::runtime_error("failed to subscribe to "+share_info.src_name);


	//finally add this toour routing bale
	routing_table_[share_info.src_name] = share_info;

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
				std::cerr<<"Routeing";
				Route(*q);
			}catch(const std::exception & e){
				std::cerr << "caught an exception reason was: " << e.what() << std::endl;
			}
		}

	}

	return true;
}

bool Share::Impl::Route(CMOOSMsg & msg)
{
	//do we know how to route this? double check
	ShareInfoMap::iterator g = routing_table_.find(msg.GetKey());
	if(g == routing_table_.end())
		throw std::runtime_error("no specified route");

	//we need to find the socket to send via
	ShareInfo & share_info = g->second;
	MulticastSocketMap::iterator mcg = multicast_socket_map_.find(share_info.socket_name);
	if(mcg == multicast_socket_map_.end())
		throw std::runtime_error("no multicast group for channel name " + share_info.socket_name);

	MulticastSocket & multicast_socket = mcg->second;

	//rename here...
	msg.m_sKey= share_info.dest_name;

	//serialise here
	unsigned int msg_buffer_size = msg.GetSizeInBytesWhenSerialised();
	std::vector<unsigned char> buffer(msg_buffer_size);
	if(!msg.Serialize(buffer.data(), msg_buffer_size))
		throw std::runtime_error("failed msg serialisation");

	//send here
	if(sendto(multicast_socket.socket_fd, buffer.data(), buffer.size(), 0, (struct sockaddr*)(&multicast_socket.multicast_addr),
		sizeof(multicast_socket.multicast_addr)) < 0)
	{
		throw std::runtime_error("failed \"sendto\"");
	}

	std::cerr<<"sent "<<buffer.size()<<"bytes"<<std::endl;
	return true;

}


bool Share::Impl::AddMulticastSocketForOutgoingTraffic(const std::string & socket_name, const std::string & address, int port)
{
	MulticastSocket multicast_socket;
	multicast_socket.address = address;
	multicast_socket.port = port;

	if ((multicast_socket.socket_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	throw std::runtime_error(
			"AddMulticastSocketForOutgoingTraffic() failed to open sender socket");

	int reuse = 1;
	if (setsockopt(multicast_socket.socket_fd, SOL_SOCKET, SO_REUSEADDR,
		&reuse, sizeof(reuse)) == -1)
	throw std::runtime_error("failed to set resuse socket option");


	int send_buffer_size = 4 * 64 * 1024;
	if (setsockopt(multicast_socket.socket_fd, SOL_SOCKET, SO_SNDBUF,
		&send_buffer_size, sizeof(send_buffer_size)) == -1)
	throw std::runtime_error("failed to set size of socket send buffer");


	char enable_loop_back = 1;
	if (setsockopt(multicast_socket.socket_fd, IPPROTO_IP, IP_MULTICAST_LOOP,
		&enable_loop_back, sizeof(enable_loop_back)) == -1)
		throw std::runtime_error("failed to disable loop back");

	char num_hops = 1;
	if (setsockopt(multicast_socket.socket_fd, IPPROTO_IP, IP_MULTICAST_TTL,
		&num_hops, sizeof(num_hops)) == -1)
		throw std::runtime_error("failed to set ttl hops");


	memset(&multicast_socket.multicast_addr, 0, sizeof (multicast_socket.multicast_addr));
	multicast_socket.multicast_addr.sin_family = AF_INET;
	multicast_socket.multicast_addr.sin_addr.s_addr = inet_addr(multicast_socket.address.c_str());
	multicast_socket.multicast_addr.sin_port = htons(multicast_socket.port);

	//finally add it to our collection of sockets
	multicast_socket_map_[socket_name] = multicast_socket;
	return true;
}


int Share::Run(int argc,char * argv[])
{
	try
	{
		_Impl->Run("pShare","Mission.moos");
	}
	catch(const std::exception & e)
	{
		std::cerr<<"oops: "<<e.what();
	}
}



}//end of MOOS namespace..
