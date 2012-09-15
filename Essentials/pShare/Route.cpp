/*
 * Route.cpp
 *
 *  Created on: Sep 1, 2012
 *      Author: pnewman
 */

#include "Route.h"
#include <sstream>
namespace MOOS {

Route::Route() {
	// TODO Auto-generated constructor stub
}

Route::~Route() {
	// TODO Auto-generated destructor stub
}

std::string Route::to_string() const
{
	std::stringstream ss;
	ss<<"ShareInfo:\n"
			<<"add: "<<dest_address.to_string()<<std::endl
			<<"dest_name: "<<dest_name<<std::endl
			<<"src_name: "<<src_name<<std::endl
			<<"multicast: "<<multicast<<std::endl;

	return ss.str();
}



}
