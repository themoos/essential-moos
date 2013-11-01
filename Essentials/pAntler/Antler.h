/*
 *  Antler.h
 *  MOOS
 *
 *  Created by pnewman on 18/04/2008.
 *  Copyright 2008 __MyCompanyName__. All rights reserved.
 *
 */


#ifdef _WIN32
#pragma warning(disable : 4786)
#endif

#ifndef ANTLERH
#define ANTLERH

#include "MOOS/libMOOS/MOOSLib.h"
#include "MOOS/libMOOS/Utils/MOOSUtilityFunctions.h"
#include <string>
#include <iostream>

#include "MOOSProc.h"

#define DEFAULTTIMEBETWEENSPAWN 1000

using namespace std;

class CAntler 
{
public:
        
    //constructor
    CAntler();
    //this is the only public function. Call it to have Antler do its thing.
    bool Run(const string & sMissionFile,
             set<string> Filter = set<string>() );
        
    //run in a headless fashion - instructions will be recieved via MOOSComms
    bool Run(const string & sHost,  int lPort, const string & sAntlerName);
		
    enum VERBOSITY_LEVEL
    {
        QUIET,
        TERSE,
        CHATTY,
    };

    enum LAUNCH_TYPE
    {
        L_UNDEF,
        L_PROCESS,
        L_CONSOLE,
        L_SCREEN // GNU screen, not X11
    };

    bool SetVerbosity(VERBOSITY_LEVEL eLevel);
		
    //call this to cause a clean shut down 
    bool ShutDown();
						  
protected:
        
    //top level spawn - all comes from here
    bool Spawn(const string & sMissionFile, bool bHeadless = false);
        
        
    //create, configure and launch a process
    CMOOSProc* CreateMOOSProcess( string sProcName);
        

    bool ConfigureMOOSComms();
    bool SendMissionFile();
        
    //tell a Monarch what is goinon remotely
    bool PublishProcessQuit(const string & sProc);
    bool PublishProcessLaunch(const string & sProc);
        
    typedef list<CMOOSProc*> MOOSPROC_LIST;
    MOOSPROC_LIST    m_ProcList;
    string m_sDefaultExecutablePath;
    CProcessConfigReader m_MissionReader;
        
    //if this set is non empty then only procs listed here will be run..
    set<string> m_Filter;
        
    int m_nCurrentLaunch;
        
    //this is used to communicate with the BD and ultimately other instantiations of
    //pAntler on different machines...
    CMOOSThread m_RemoteControlThread;
    CMOOSCommClient * m_pMOOSComms;
    /**method to allow Listen thread to be launched with a MOOSThread.*/
    static bool _RemoteControlCB(void* pParam)
    {
        CAntler* pMe = (CAntler*) pParam;
        return pMe->DoRemoteControl();
    }
    /** internal MOOS client callbacks*/
    static bool _MOOSConnectCB(void *pParam)
    {
        CAntler* pMe = (CAntler*) pParam;
        return pMe->OnMOOSConnect();
    }
    static bool _MOOSDisconnectCB(void *pParam)
    {
        CAntler* pMe = (CAntler*) pParam;
        return pMe->OnMOOSDisconnect();
    }
    /** main comms handling thread*/
    bool DoRemoteControl();
    /** hellos MOOSDB!*/
    bool OnMOOSConnect();
    /** goodby MOOSDB*/
    bool OnMOOSDisconnect();
		
    CMOOSLock m_JobLock;
    string m_sMissionFile;
    bool m_bHeadless;
    bool m_bQuitCurrentJob;
    bool m_bSupportGentleKill;
    bool m_bRunning;
    bool m_bNewJob;
    string m_sMonarchAntler;
    bool m_bKillOnDBDisconnect;
    string m_sReceivedMissionFile;
    string m_sAntlerName;
    string m_sDBHost;
    int m_nDBPort;
        
    VERBOSITY_LEVEL m_eVerbosity;


        
};
#endif

