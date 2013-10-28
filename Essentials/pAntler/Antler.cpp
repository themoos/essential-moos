
#include "Antler.h"
#include "MOOSProc.h"

using namespace std;
#include <sstream>
#include <iostream>
#include <sstream>
#include <string>
#include <iterator>
#include <algorithm>




CAntler::CAntler()
{
    m_JobLock.UnLock();    
    m_bNewJob = false;
    m_sAntlerName = "Monarch";
    m_sDBHost = "localhost";
    m_nDBPort = 9000;
    m_bQuitCurrentJob = false;
    m_eVerbosity = CHATTY;
}

//this is the vanilla version of Run - called to run from a single mission file
bool CAntler::Run(const std::string & sMissionFile, std::set<std::string> Filter)
{
    m_bHeadless = false;
    m_sMissionFile = sMissionFile;
    m_Filter = Filter;
    return Spawn(m_sMissionFile);
}

//this version will wait for a mission fiel to be sent via a DB
bool CAntler::Run(const std::string & sHost, int nPort, const std::string & sAntlerName)
{
    //this is more interesting...
    m_sAntlerName = sAntlerName;
    m_sDBHost = sHost;
    m_nDBPort = nPort;
    m_bHeadless = true;
    
    
    if (!ConfigureMOOSComms())
        return true;
        
    
    MOOSTrace("   This is headless Antler called \"%s\"\n"
              "Waiting for mission file from %s:%d\n",
              m_sAntlerName.c_str(),
              m_sDBHost.c_str(),
              m_nDBPort);
    
       
    const char * sSpin = "-\\|/";
    while (1)
    {
        //wait to be signalled that there is work to do...
        int i = 0;
        while (!m_bNewJob)
        {
            MOOSPause(500);
            MOOSTrace("   Speak to me Monarch....%c\r", sSpin[i++%3]);
        }            
        //no more launching until this community is complete
        m_JobLock.Lock();
        Spawn(m_sReceivedMissionFile, true);
        m_bNewJob = false;
        m_JobLock.UnLock();
        
    }
}



bool CAntler::DoRemoteControl()
{
    while (1)
    {
        MOOSPause(100);
        if (!m_pMOOSComms->IsConnected())
            continue;
        
        //better check mail
        MOOSMSG_LIST NewMail;
        if (m_pMOOSComms->Fetch(NewMail))
        {
            CMOOSMsg Msg;
            if (!m_bHeadless)
            {
                if (m_pMOOSComms->PeekAndCheckMail(NewMail, "ANTLER_STATUS", Msg))
                {
                    std::string sWhat;
                    MOOSValFromString(sWhat, Msg.GetString(), "Action");
                    std::string sProc;
                    MOOSValFromString(sProc, Msg.GetString(), "Process");
                    std::string sID;
                    MOOSValFromString(sID, Msg.GetString(), "AntlerID");
                    
                    MOOSTrace("   [rmt] Process %-15s has %s (by %s)\n",
                              sProc.c_str(),
                              sWhat.c_str(),
                              sID.c_str());
                }    
            }
            else // headless
            {
                
                if (m_pMOOSComms->PeekMail(NewMail, "MISSION_FILE", Msg))
                {
                    MOOSTrace("\n|***** Dynamic Brief *****|\n\n");
                    
                    //make a new file name
                    m_sReceivedMissionFile = MOOSFormat("runtime_%s.moos",
                                                        MOOSGetTimeStampString().c_str());
                    
                    MOOSTrace("   %s received [%d bytes]\n",
                              m_sReceivedMissionFile.c_str(),
                              Msg.GetString().size());
                    MOOSTrace("   shutting down all current spawned processes:\n");
                    
                    //tell the current job to quit, and wait for it to happen
                    m_bQuitCurrentJob = true;
                    m_JobLock.Lock();        
                    
                    //here we copy the mission file contained in the message to 
                    std::stringstream ss(Msg.GetString());
                    
                    //suck out the Antler filter line
                    std::string sFilter;
                    std::getline(ss, sFilter);
                    MOOSTrace("%s\n", sFilter.c_str());
                    MOOSChomp(sFilter, "ANTLERFILTER:", true);
                    std::stringstream ssF(sFilter);
                    
                    //fill in the filter set
                    std::copy(istream_iterator<std::string>(ssF), 
                              istream_iterator<string>(),
                              std::inserter(m_Filter, m_Filter.begin()));
                    
                    //write out the whole file
                    std::ofstream Out(m_sReceivedMissionFile.c_str());
                    if (!Out.is_open())
                    {
                        m_JobLock.UnLock();
                        return MOOSFail("failed to open mission file for writing");
                    }
                    
                    Out << ss.rdbuf();  //you've gotta lurve C++ ...
                    Out.close();
                    
                    m_bQuitCurrentJob = false;  // don't quit; it has already quit
                    m_bNewJob = true;  //signal that we have more work to do
                    m_JobLock.UnLock();  //let thread 0 continue
                }
            }
        }
    }        
}


bool CAntler::SetVerbosity(VERBOSITY_LEVEL eLevel)
{
    m_eVerbosity = eLevel;
    switch(eLevel)
    {
    case CHATTY:
        break;
    case TERSE:
        InhibitMOOSTraceInThisThread(true);
        break;
    case QUIET:
        InhibitMOOSTraceInThisThread(true);
        if (m_pMOOSComms != NULL)
            m_pMOOSComms->SetQuiet(true);
			
    }
    return true;
}


bool CAntler::ConfigureMOOSComms()
{
    
    //start a monitoring thread
    m_RemoteControlThread.Initialise(_RemoteControlCB, this);
    m_RemoteControlThread.Start();
    
    m_pMOOSComms = new CMOOSCommClient;
    m_pMOOSComms->SetOnConnectCallBack(_MOOSConnectCB, this);
    m_pMOOSComms->SetOnDisconnectCallBack(_MOOSDisconnectCB, this);
    m_pMOOSComms->SetQuiet(true);
    
    std::string sMe = MOOSFormat("Antler{%s}", m_sAntlerName.c_str());
    
    //try and connect to a DB
    if (!m_pMOOSComms->Run(m_sDBHost.c_str(), (long int)m_nDBPort, sMe.c_str(), 1))
        return MOOSFail("could not set up MOOSComms\n");
    
    return true;
}



bool CAntler::SendMissionFile()
{
    MOOSTrace("\n\n|***** Propagate *****|\n\n");
    CMOOSFileReader FR;
    FR.SetFile(m_sMissionFile);
    std::stringstream ss;

    //copy the filters in
    ss<<"ANTLERFILTER:";
    std::copy (m_Filter.begin(), 
               m_Filter.end(), 
               ostream_iterator <std::string> (ss, " "));
    ss<<std::endl;
    
    while (!FR.eof())
    {
        std::string sL = FR.GetNextValidLine()+"\n";
        ss<<sL;
    }
    m_pMOOSComms->Notify("MISSION_FILE", ss.str());
    
    
    MOOSTrace("   Monarch published thinned mission file [%d bytes]\n\n",
              ss.str().size());
    return true;
    
}


bool CAntler::OnMOOSConnect()
{
    if (m_bHeadless)
    {
        MOOSTrace("  Connecting to a DB\n");    
        m_pMOOSComms->Register("MISSION_FILE", 0);
      
    }
    else
    {
        m_pMOOSComms->Register("ANTLER_STATUS", 0);
        SendMissionFile();
    }
    return true;
}


bool CAntler::OnMOOSDisconnect()
{
    if (m_bHeadless)
    {
        MOOSTrace("   DB Connection Lost\n");    
        
        if (m_bKillOnDBDisconnect)
        {
            //look likes the monarch is dead.....
            MOOSTrace("   shutting down all current spawned processes:\n");
            
            //tell the current job to quit
            m_bQuitCurrentJob = true;
            
            //wait for that to happen
            m_JobLock.Lock();        
            m_JobLock.UnLock();        
        }
       
    }
    return true;
}


bool CAntler::PublishProcessQuit(const std::string & sProc)
{
    if (!m_bHeadless)
        return false;
    
    if (!m_pMOOSComms->IsConnected())
        return false;

    m_pMOOSComms->Notify("ANTLER_STATUS",
                         MOOSFormat("Action=Quit,Process=%s,AntlerID=%s",
                                    sProc.c_str(),
                                    m_sAntlerName.c_str()));
    
    return true;
}


bool CAntler::PublishProcessLaunch(const std::string & sProc)
{
    if (!m_bHeadless)
        return false;
    
    if (!m_pMOOSComms->IsConnected())
        return false;
    
    m_pMOOSComms->Notify("ANTLER_STATUS",
                         MOOSFormat("Action=Launched,Process=%s,AntlerID=%s",
                                    sProc.c_str(), m_sAntlerName.c_str()));
    
    return true;
}



bool CAntler::Spawn(const std::string &  sMissionFile, bool bHeadless)
{
    
    MOOSTrace("\n\n|****** Launch ******|\n\n");
    m_nCurrentLaunch = 0;
    
    
    //set up the mission file reader
    if (!m_MissionReader.SetFile(sMissionFile))
        return MOOSFail("error reading mission file\n");
    
    
    m_MissionReader.SetAppName("ANTLER"); //NB no point in running under another name...
    // (I guess Antler1 could launch Antler2 though...)
    
    STRING_LIST      sParams;
    
    if (!m_MissionReader.GetConfiguration(m_MissionReader.GetAppName(), sParams))
        return MOOSFail("error reading antler config block from mission file\n");
    
    
    //fetch all the lines in teg Antler configuration block
    STRING_LIST::iterator p;
    sParams.reverse();
    
    int nTimeMSBetweenSpawn = DEFAULTTIMEBETWEENSPAWN;
    m_MissionReader.GetConfigurationParam("MSBetweenLaunches", nTimeMSBetweenSpawn);
    
    //here we'll figure out a what paths to use when looking for  executables
    m_sDefaultExecutablePath = "SYSTEMPATH";
    m_MissionReader.GetConfigurationParam("ExecutablePath", m_sDefaultExecutablePath);
    
    if (MOOSStrCmp("SYSTEMPATH", m_sDefaultExecutablePath))
    {
        MOOSTrace("Unless directed otherwise using system path to locate binaries \n");
        m_sDefaultExecutablePath = "";
    }
    else
    {
        //MOOSTrace("\"ExecutablePath\" is %s\n", m_sDefaultExecutablePath.c_str());
        if (*m_sDefaultExecutablePath.rbegin() != '/')
        {
            //look to add extra / if needed
            m_sDefaultExecutablePath += '/';
        }
        
    }
    
	
    //are we being ask to support gentle process killing?
    m_MissionReader.GetConfigurationParam("GentleKill", m_bSupportGentleKill);
	
	
    //now cycle through each line in the configuration block. 
    // If it begins with run then it means launch
    for (p = sParams.begin(); p != sParams.end(); p++)
    {
        std::string sLine = *p;
        
        std::string sWhat = MOOSChomp(sLine, "=");
        
        if (MOOSStrCmp(sWhat, "RUN"))
        {
            //OK we are being asked to run a process
            
            //try to create a process
            CMOOSProc* pNew  = CreateMOOSProcess(sLine);
            
            if (pNew != NULL)
            {
                //this a really important bit of text most folk will want to see it...
                InhibitMOOSTraceInThisThread(m_eVerbosity == QUIET);
                {
                    MOOSTrace("   [%.3d] Process: %-15s ~ %-15s launched successfully\n",
                              m_nCurrentLaunch,
                              pNew->GetAppName().c_str(),
                              pNew->GetMOOSName().c_str());
                }
                InhibitMOOSTraceInThisThread(m_eVerbosity != CHATTY);
				
                m_ProcList.push_front(pNew);
                m_nCurrentLaunch++;
                PublishProcessLaunch(pNew->GetAppName());
            }
            
            //wait a while
            MOOSPause(nTimeMSBetweenSpawn);
            
        }
    }
    
    
    
    if (bHeadless)
    {
        m_bKillOnDBDisconnect = true;
        m_MissionReader.GetConfigurationParam("KillOnDBDisconnect", m_bKillOnDBDisconnect);
    }
    else
    {
        bool bMaster = false;
        m_MissionReader.GetConfigurationParam("EnableDistributed", bMaster);
        if (bMaster)
        {
            
            m_MissionReader.GetValue("ServerHost", m_sDBHost);
            m_MissionReader.GetValue("ServerPort", m_nDBPort);
            
            
            if (!ConfigureMOOSComms())
                return MOOSFail("failed to start MOOS comms");
            
        }
    }
    

    
    //now wait on all our processes to close....
    while (m_ProcList.size())
    {
        MOOSPROC_LIST::iterator q;
        
        for (q = m_ProcList.begin(); q != m_ProcList.end(); q++)
        {
            CMOOSProc* pMOOSProc = *q;
            
            if (m_bQuitCurrentJob)
            {
                MOOSTrace("   actively killing running child %s\n",
                          pMOOSProc->GetAppName().c_str());
                pMOOSProc->Stop(m_bSupportGentleKill);
            }
 
            if (pMOOSProc->IsStopped())
            {

                //this a really important bit of text most folk will want to see it...
                InhibitMOOSTraceInThisThread(m_eVerbosity == QUIET);
                {
                    MOOSTrace("   [%.3d] Process: %-15s has quit\n",
                              --m_nCurrentLaunch,
                              pMOOSProc->GetAppName().c_str());
                }
                InhibitMOOSTraceInThisThread(m_eVerbosity != CHATTY);

                
                PublishProcessQuit(pMOOSProc->GetAppName());
                delete pMOOSProc;
                
                m_ProcList.erase(q);
                break;
            }

            MOOSPause(100);
            
        }
              
    }
    
    return 0;
}





CMOOSProc* CAntler::CreateMOOSProcess(string sConfiguration)
{
    CProcCfg config;
    
    if (!config.ParseConfig(sConfiguration))
    {
        return NULL;
    }

    // set pointer to current launch number
    config.m_nCurrentLaunch = &m_nCurrentLaunch;

    bool bDistributed = false;
    m_MissionReader.GetConfigurationParam("EnableDistributed", bDistributed);

    if (!bDistributed)
    {
        //we run everything
    }
    else
    {
        std::string sAntlerRequired;
        if (!m_bHeadless)
        {
            //we are a TopMOOS
            if (config.GetParamValue("AntlerID", sAntlerRequired))
                return NULL; //this is for a drone
            
        }
        else
        {
            //we are a drone
            if (config.GetParamValue("AntlerID", sAntlerRequired))
                return NULL; //this is for primary Antler
            
                        
            if (!MOOSStrCmp(sAntlerRequired, m_sAntlerName))
                return NULL; //for some other Antler
            
            //OK it is for us...
            //MOOSTrace("Headless Antler found a RUN directive...\n");
        }
    }
    

    //NEW option: launch (process, console, screen)
    LAUNCH_TYPE lt = L_UNDEF;
    string sLaunchType;
    if (config.GetParamValue("LAUNCHTYPE", sLaunchType))
    {
        if (MOOSStrCmp("PROCESS", sLaunchType))
            lt = L_PROCESS;
        else if (MOOSStrCmp("CONSOLE", sLaunchType))
            lt = L_CONSOLE;
        else if (MOOSStrCmp("SCREEN", sLaunchType))
            lt = L_SCREEN;
        else
        {
            MOOSTrace("Error: LaunchType was not one of (Process, Console, Screen)");
            return NULL;
        }
    }
    
    // backwards compatibility with NEWCONSOLE
    bool bNewConsole = false;
    bool bGotConsoleParam = MOOSValFromString(bNewConsole, 
                                              config.m_sParams, 
                                              "NEWCONSOLE", 
                                              true);

    // fill in the value if we need it.  otherwise, warn if we have both specifiers
    if (L_UNDEF == lt)
    {
        MOOSTrace("Warning: NEWCONSOLE is deprecated.  Use LAUNCHTYPE.");
        lt = bNewConsole ? L_CONSOLE : L_PROCESS;
    }
    else if (bGotConsoleParam)
    {
        MOOSTrace("Warning: LAUNCHTYPE overrides NEWCONSOLE, so ignoring this value");
    }
    
    
    //here we bail according to our filters 
    if (!m_Filter.empty() && m_Filter.find(config.m_sMOOSName) == m_Filter.end())
    {
        return NULL;
    }


    //All good up to here, now make a new process holder stucture...        
    CMOOSProc* pNewProc = NULL;

#ifdef _WIN32

    switch (lt)
    {
    case L_PROCESS:
        pNewProc = new CMOOSProcWin32(config, &m_MissionReader);
        break;
    case L_CONSOLE:
        pNewProc = new CMOOSProcConsoleWin32(config, &m_MissionReader);
        break;
    case L_SCREEN:
        MOOSTrace("GNU screen launch is not yet supported under Win32");
        return NULL;
    default:
        MOOSTrace("LAUNCHTYPE and NEWCONSOLE didn't reveal the launch type");
        return NULL;
    }

#else        

    switch (lt)
    {
    case L_PROCESS:
        pNewProc = new CMOOSProcNix(config, &m_MissionReader);
        break;
    case L_CONSOLE:
        pNewProc = new CMOOSProcConsoleNix(config, &m_MissionReader);
        break;
    case L_SCREEN:
        pNewProc = new CMOOSProcScreenNix(config, &m_MissionReader);
        break;
    default:
        MOOSTrace("LAUNCHTYPE and NEWCONSOLE didn't reveal the launch type");
        return NULL;
    }

#endif
    

    if (!pNewProc->BuildConfig())
    {
        MOOSTrace("App %s failed BuildConfig()", config.m_sAppName.c_str());
        delete pNewProc;
        return NULL;
    }

    
    //finally spawn each according to his own
    if (pNewProc->Start(m_bSupportGentleKill))
    {
        ++m_nCurrentLaunch;
        return pNewProc;
    }
    else
    {        
        delete pNewProc;
        return NULL;
    }
}


bool CAntler::ShutDown()
{
	
    MOOSPROC_LIST::iterator q;

    MOOSTrace("\n\n|***** Shutdown *****|\n\n");

    for (q = m_ProcList.begin();q != m_ProcList.end(); q++)
    {
        CMOOSProc * pMOOSProc = *q;

        if (pMOOSProc->Stop(m_bSupportGentleKill))
        {
	 
#ifndef _WIN32
			
            int nStatus = 0;
            MOOSTrace("\n   Signalling %-15s ", pMOOSProc->GetAppName().c_str());
            if (0 < waitpid(pMOOSProc->GetChildPID(), &nStatus, 0))
            {
                MOOSTrace("[OK]");
            }		
#endif
        }

    }
	
    MOOSTrace("\n\n   All spawned processes shutdown.\n\n   That was the MOOS \n\n");
	 
	 
    return true;
}

