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

class MulticastListener {
public:

	MulticastListener(SafeList<CMOOSMsg> & queue, const std::string & address, int port);
	virtual ~MulticastListener();
	bool Run();
protected:
	bool ListenLoop();
	CMOOSThread thread_;
	SafeList<CMOOSMsg > & queue_;
	std::string address_;
	int port_;

public:
	static bool dispatch(void * pParam)
	{
		MulticastListener* pMe = (MulticastListener*)pParam;
		return pMe->ListenLoop();
	}


};

}

#endif /* MULTICASTLISTENER_H_ */
