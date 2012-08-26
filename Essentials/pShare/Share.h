/*
 * Share.h
 *
 *  Created on: Aug 26, 2012
 *      Author: pnewman
 */

#ifndef SHARE_H_
#define SHARE_H_

/*
 *
 */
namespace MOOS {

class Share {
public:
	Share();
	virtual ~Share();
	int Run(int argc,char * argv[]);

private:
	class Impl;
	Impl* _Impl;
};

}

#endif /* SHARE_H_ */
