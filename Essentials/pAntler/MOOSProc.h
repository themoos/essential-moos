/*
 *  MOOSProc.h
 *  MOOS
 *
 *  Created by Ian Katz on 2013-10-27
 *  Based on code from pnewman on 18/04/2008.
 *
 */


#ifdef _WIN32
#pragma warning(disable : 4786)
#endif

#ifndef MOOSPROCH
#define MOOSPROCH

#include "MOOS/libMOOS/MOOSLib.h"
#include <string>


#ifdef _WIN32
#include "XPCProcess.h"
#include "XPCProcessAttrib.h"
#include "XPCException.h"
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#endif 

using namespace std;

#define DEFAULT_NIX_TERMINAL "xterm"

// data structure for the "RUN = ..." line in a pAntler config
class CProcCfg
{
public:
    CProcCfg() {};
    CProcCfg(string sConfig) { ParseConfig(sConfig); };
    ~CProcCfg() {};

    // keep attributes public
    string m_sAppName;     // executable name
    string m_sAppPath;     // full path to app
    string m_sMOOSName;    // run-as name of app
    string m_sMissionFile; // path to mission file
    string m_sParams;      // key-val parameters from config line
    
    // SPECIAL USE
    string m_sExecPath;    // "SYSTEM", or prefix to executables
    int* m_nCurrentLaunch; // pointer to number of launches

    // read in all data
    bool ParseConfig(string sConfig);

    // get a value from the parsed data
    bool GetParamValue(string sParam, string &sValue);

};



// data structure for a process instantiated from RUN line
class CMOOSProc
{
public:
    CMOOSProc(CProcCfg config, CProcessConfigReader* MissionReader) 
        : m_cfg(config), m_MissionReader(MissionReader) {};
    virtual ~CMOOSProc() {};

    bool BuildConfig();

    virtual bool BuildConfigSpecific() = 0;
    virtual bool Start(bool bGentle) = 0;
    virtual bool Stop(bool bGentle) = 0;
    virtual bool IsStopped() = 0;

    string MissionFile() { return m_MissionReader->GetFileName(); };
    bool IsMOOSDB() { return MOOSStrCmp("MOOSDB", m_cfg.m_sAppName); };
    bool IsIRemote() { return MOOSStrCmp("iRemote", m_cfg.m_sAppName); };

    string GetAppName() { return m_cfg.m_sAppName; };
    string GetMOOSName() { return m_cfg.m_sMOOSName; };

protected:

    CProcCfg m_cfg;

    CProcessConfigReader* m_MissionReader;

    // settings from more in-depth parsing
    bool m_bInhibitMOOSParams;
    STRING_LIST m_CommandLineParameters;


    // conditional compilation to hold OS-specific program launch info
#ifdef _WIN32
    XPCProcess * m_pWin32Proc;
    XPCProcessAttrib * m_pWin32Attrib;
#else
    pid_t m_ChildPID;

public:
    pid_t GetChildPID() { return m_ChildPID; };

#endif        

};



#ifdef _WIN32

typedef std::list<DWORD> DWORD_LIST;

class CMOOSProcWin32 : public CMOOSProc
{
public:
    CMOOSProcWin32(CProcCfg config, CProcessConfigReader* MissionReader) 
        : CMOOSProc(config, MissionReader) {};
    virtual ~CMOOSProcWin32();

    // value passed to vSetCreationFlag
    virtual DWORD CreationFlags() { return CREATE_SUSPENDED; };

    virtual bool BuildConfigSpecific();
    virtual bool BuildConfigSpecific2() 
    { return true; }; // for console, we ignore it in this instance

    virtual bool Start(bool bGentle);
    virtual bool Stop(bool bGentle);
    virtual bool IsStopped();

protected:
    // unused in this instance, but placed here to cut down on duplicate code
    DWORD_LIST m_ConsoleFlags; 

};


class CMOOSProcConsoleWin32 : public CMOOSProcWin32
{
public:
    CMOOSProc(CProcCfg config, CProcessConfigReader* MissionReader) 
        : CMOOSProcProcWin32Base(config, MissionReader) {};
    virtual ~CMOOSProc() {};

    virtual bool BuildConfigSpecific2(string sConfig);
    virtual DWORD CreationFlags() { return CREATE_NEW_CONSOLE | CREATE_SUSPENDED; };

};


#else

class CMOOSProcNixBase : public CMOOSProc
{
public:
    CMOOSProcNixBase(CProcCfg config, CProcessConfigReader* MissionReader) 
        : CMOOSProc(config, MissionReader) {};
    virtual ~CMOOSProcNixBase() {};

    // we add a 300ms pause after the "stop" signal on *nix
    virtual bool Stop(bool bGentle);
    virtual bool StopSpecific(bool bGentle) = 0;

    // we need to do more sophisticated things with the command line
    virtual bool Start(bool bGentle);
    virtual bool StartSpecific() = 0;

    virtual bool IsStopped();

protected:
    STRING_LIST m_FullCommandLine;
};


class CMOOSProcNix : public CMOOSProcNixBase
{
public:
    CMOOSProcNix(CProcCfg config, CProcessConfigReader* MissionReader) 
        : CMOOSProcNixBase(config, MissionReader) {};
    virtual ~CMOOSProcNix() {};

    virtual bool BuildConfigSpecific();
    virtual bool StartSpecific();
    virtual bool StopSpecific(bool bGentle);
};


class CMOOSProcConsoleNix : public CMOOSProcNixBase
{
public:
    CMOOSProcConsoleNix(CProcCfg config, CProcessConfigReader* MissionReader) 
        : CMOOSProcNixBase(config, MissionReader) {};
    virtual ~CMOOSProcConsoleNix() {};

    virtual bool BuildConfigSpecific();
    virtual bool StartSpecific();
    virtual bool StopSpecific(bool bGentle);

protected:
    STRING_LIST m_ConsoleLaunchParameters;
};


class CMOOSProcScreenNix : public CMOOSProcNixBase
{
public:
    CMOOSProcScreenNix(CProcCfg config, CProcessConfigReader* MissionReader) 
        : CMOOSProcNixBase(config, MissionReader) {};
    virtual ~CMOOSProcScreenNix() {};

    virtual bool BuildConfigSpecific();
    virtual bool StartSpecific();
    virtual bool StopSpecific(bool bGentle);

protected:
    STRING_LIST m_ScreenLaunchParameters;
};


#endif // _WIN32


#endif // ifndef MOOSPROC
