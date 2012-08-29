/*
 * MulticastListener.cpp
 *
 *  Created on: Aug 27, 2012
 *      Author: pnewman
 */
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string>
#include <stdexcept>
#include <iostream>
#include "Listener.h"



namespace MOOS {



Listener::Listener(SafeList<CMOOSMsg> & queue,
		const std::string & address,
		int port,
		bool multicast):queue_(queue), address_(address), port_(port),multicast_(multicast) {
	// TODO Auto-generated constructor stub

}

Listener::~Listener() {
	// TODO Auto-generated destructor stub
}


bool Listener::Run()
{
	thread_.Initialise(dispatch, this);
	return thread_.Start();
}

bool Listener::ListenLoop()
{

	//set up socket....
	int socket_fd;
	socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if(socket_fd<0)
		throw std::runtime_error("MulticastListener::Run()::socket()");

	//we want to be able to resuse it (multiple folk are interested)
	int reuse = 1;
	if (setsockopt(socket_fd, SOL_SOCKET,SO_REUSEADDR/* SO_REUSEPORT*/, &reuse, sizeof(reuse)) == -1)
		throw std::runtime_error("MulticastListener::Run()::setsockopt::reuse");

	//give ourselves plenty of receive space
	//set aside some space for receiving - just a few multiples of 64K
	int rx_buffer_size = 64*1024*28;
	if (setsockopt(socket_fd, SOL_SOCKET, SO_RCVBUF, &rx_buffer_size, sizeof(rx_buffer_size)) == -1)
		throw std::runtime_error("MulticastListener::Run()::setsockopt::rcvbuf");

	/* construct a multicast address structure */
	struct sockaddr_in mc_addr;
	memset(&mc_addr, 0, sizeof(mc_addr));
	mc_addr.sin_family = AF_INET;
	mc_addr.sin_addr.s_addr = inet_addr(address_.c_str());
	mc_addr.sin_port = htons(port_);

	if (bind(socket_fd, (struct sockaddr*) &mc_addr, sizeof(mc_addr)) == -1)
		throw std::runtime_error("MulticastListener::Run()::bind");

	if(multicast_)
	{
		//join the multicast group
		struct ip_mreq mreq;
		mreq.imr_multiaddr.s_addr = inet_addr(address_.c_str());
		mreq.imr_interface.s_addr = INADDR_ANY;
		if(setsockopt(socket_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq))==-1)
			throw std::runtime_error("MulticastReceiver::Run()::setsockopt::ADD_MEMBERSHIP");
	}


	//make a receive buffer
	std::vector<unsigned char > incoming_buffer(2*64*1024);

	while(!thread_.IsQuitRequested())
	{
		//read socket blocking
		int num_bytes_read = read(socket_fd,
				incoming_buffer.data(),
				incoming_buffer.size());

		if(num_bytes_read>0)
		{
			std::cerr<<"read "<<num_bytes_read<<std::endl;

			//deserialise
			CMOOSMsg msg;
			msg.Serialize(incoming_buffer.data(), incoming_buffer.size(), false);

			//push onto queue
			queue_.Push(msg);
		}

	}

	return true;

}

}
