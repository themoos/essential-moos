/*
 * MulticastListener.h
 *
 *  Created on: Aug 27, 2012
 *      Author: pnewman
 */

#ifndef MULTICASTLISTENER_H_
#define MULTICASTLISTENER_H_

/*
 *
 */
#include "MOOS/libMOOS/Utils/SafeList.h"
#include "MOOS/libMOOS/Utils/MOOSThread.h"
#include "MOOS/libMOOS/Comms/MOOSMsg.h"

namespace MOOS {

class Listener {
public:

	Listener(SafeList<CMOOSMsg> & queue, const std::string & address, int port,bool multicast);
	virtual ~Listener();
	bool Run();
	std::string address(){return address_;};
	int port(){return port_;};
protected:
	bool ListenLoop();
	CMOOSThread thread_;
	SafeList<CMOOSMsg > & queue_;
	std::string address_;
	int port_;
	bool multicast_;

public:
	static bool dispatch(void * pParam)
	{
		Listener* pMe = (Listener*)pParam;
		return pMe->ListenLoop();
	}


};

}

#endif /* MULTICASTLISTENER_H_ */
