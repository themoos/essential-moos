/*
 * ShareHelp.h
 *
 *  Created on: Sep 8, 2012
 *      Author: pnewman
 */

#ifndef SHAREHELP_H_
#define SHAREHELP_H_

/*
 *
 */
class ShareHelp {
public:
	ShareHelp();
	virtual ~ShareHelp();
	static void PrintHelpAndExit();
	static void PrintInterfaceAndExit();
	static void PrintConfigurationExampleAndExit();
};

#endif /* SHAREHELP_H_ */
