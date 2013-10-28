#include "MOOSProc.h"

#define DEBUG_LAUNCH 0


using namespace std;

// read the bare minimum of text to begin processing
bool CProcCfg::ParseConfig(string sConfig)
{
    
    //what is its name? (if no @ symbol we just get the name and no cmdline)
    m_sAppName = MOOSChomp(sConfig, "@");

    if (m_sAppName.empty())
    {
        MOOSTrace("no process specified - RUN=???\n");
        return false;
    }

    //further parameters are to right of @
    string sTmp = sConfig;
    
    //look for tilde demarking end of param=val block
    m_sParams = MOOSChomp(sTmp, "~");
    
    //read the remainder 
    m_sMOOSName = MOOSChomp(sTmp);

    return true;
    
}


bool CProcCfg::GetParamValue(string sParam, string &sValue)
{
    return MOOSValFromString(sValue, m_sParams, sParam, true);
}


// fully parse the config, looking up all references to alternate configs
bool CMOOSProc::BuildConfig()
{

    // Look for a line containing additional command line params (ExtraProcessParams)
    m_CommandLineParameters.clear();
    string sExtraParamsName;    
    if (m_cfg.GetParamValue("ExtraProcessParams", sExtraParamsName))
    {
        std::string sExtraParams;
        
        //OK look for this configuration string
        if (!m_MissionReader->GetConfigurationParam(sExtraParamsName, sExtraParams))
            return MOOSFail("   warning cannot find extra parameters named \"%s\"\n",
                            sExtraParamsName.c_str());
        
        // convert commas to spaces
        while (!sExtraParams.empty())
            m_CommandLineParameters.push_back(MOOSChomp(sExtraParams, ","));
    }
    
    // Inhibit MOOS params?
    MOOSValFromString(m_bInhibitMOOSParams, m_cfg.m_sParams, "InhibitMOOSParams", true);

    //by default process are assumed to be on the system path
    //users can specify an alternative path for all process by setting 
    //    "ExectutablePath=<Path>" in the mission file configuration block.
    //user has the option of specifying paths individually process by process.
    //alternativelt they can specify that a particular process should be located by 
    // system wide and not in the default executable path
    string sSpecifiedPath;
    m_cfg.m_sAppPath = m_cfg.m_sAppName;
    if (!m_cfg.GetParamValue("PATH", sSpecifiedPath))
    {
        //we just use the Antler-wide Exepath
        m_cfg.m_sAppPath = m_cfg.m_sExecPath + m_cfg.m_sAppName;
    }
    else
    {
        if (!MOOSStrCmp(sSpecifiedPath, "SYSTEM"))
        {
            //we are being told to look in a special place
            m_cfg.m_sAppPath = sSpecifiedPath + "/" + m_cfg.m_sAppName;
        }
        // otherwise it's up to the SYSTEM, so do nothing
    }
    
    
    //here we figure out what MOOS name is implied if none is given (thanks to N.P and F.A)
    if (m_cfg.m_sMOOSName.empty())
    {
        std::string sTmp = m_cfg.m_sAppName;
        while (sTmp.rfind('/') != string::npos) 
        {
            sTmp = sTmp.substr(sTmp.rfind('/') + 1);
        }

        if (sTmp.empty()) 
        { 
            // ended with a / ?
            MOOSTrace("Error in configuration  -MOOS Name cannot end in \" / \" : %s\n",
                      m_cfg.m_sAppName.c_str());
            return false;
        }
        m_cfg.m_sMOOSName = sTmp;
    }
    
    

    // other stuff

    return this->BuildConfigSpecific();
}



#ifdef _WIN32

CMOOSProcWin32::~CMOOSProcWin32()
{
    delete m_pWin32Attrib;
    delete m_pWin32Proc;
}


bool CMOOSProcWin32::BuildConfigSpecific()
{
    

    return this->BuildConfigSpecific2();
}

bool CMOOSProcConsoleWin32::BuildConfigSpecific2()
{
    if (MOOSValFromString(sLaunchConfigurationName, sParam, "Win32Config"))
    {
        //OK look for this configuration string
        if (!m_MissionReader->GetConfigurationParam(sLaunchConfigurationName, 
                                                    sLaunchConfiguration))
            return MOOSFail("   warning: could not find resource string called \"%s\"\n",
                            sLaunchConfigurationName.c_str()) ;

        // which is ridiculous, because we can't do anything with it
    }
    else
    {
        if (IsMOOSDB(sAppName))
        {
            this->m_ConsoleFlags.push_back(BACKGROUND_BLUE);
        }
        
        if (IsIRemote(sAppName))
        {
            this->m_ConsoleFlags.push_back(BACKGROUND_RED);
        }
    }

    return true;
}

bool CMOOSProcWin32::Start(bool bGentle)
{
    try
    {
        // make the command line ProcessBinaryName MissionFile ProcessMOOSName
        string sCmd = this->m_sApp + " ";
        if (!this->m_bInhibitMOOSParams)
            sCmd += this->MissionFile() + " " + this->m_sMOOSName + " ";
        
        //continuing this task, here we pass extra parameters to the MOOS process if required
        for (STRING_LIST::iterator p = this->m_CommandLineParameters.begin();
             p != this->m_ExtraCommandLineParameters.end();
             p++)
        {
            sCmd += *p + " ";
        }
        
        //make a new process attributes class
        this->m_pWin32Attrib = NULL;
        this->m_pWin32Attrib = new XPCProcessAttrib(NULL, (char *)sCmd.c_str());        
        this->m_pWin32Attrib->vSetCreationFlag(this->CreationFlags());
        
        //we shall use our own start up image as a starting point
        STARTUPINFO StartUp;
        GetStartupInfo (&StartUp);
        
        //set up the title
        StartUp.lpTitle = (char*)this->m_sApp.c_str();
        
        //set up white text
        StartUp.dwFillAttribute = 
            FOREGROUND_INTENSITY | 
            FOREGROUND_BLUE |
            FOREGROUND_RED |
            FOREGROUND_GREEN |
            BACKGROUND_INTENSITY ;
        
        
        //give users basic control over backgroun as an RGB combination
        for (DWORD_LIST::iterator q = this->m_ConsoleFlags.begin();
             q != this->m_ConsoleFlags.end();
             q++)
        {
            StartUp.dwFillAttribute |= *q;
            StartUp.dwFlags |= STARTF_USEFILLATTRIBUTE;
        }
        
        this->m_pWin32Attrib->vSetStartupInfo(&StartUp);
        
        //no create an object capable of laucnhing win32 processes
        this->m_pWin32Proc = NULL;
        this->m_pWin32Proc = new XPCProcess(* this->m_pWin32Attrib);
        
        //go!
        this->m_pWin32Proc->vResume();
        
    }
    catch (XPCException & e)
    {
        if (this->m_pWin32Attrib != NULL)
        {
            delete this->m_pWin32Attrib;
        }
        if (this->m_pWin32Proc != NULL)
        {
            delete this->m_pWin32Proc;
        }
       	MOOSTrace("*** %s Launch Failed:***\n\a\a", this->m_sApp.c_str());
       	MOOSTrace("%s\n", e.sGetException());
        return false;
    }
    
    return true;

}


bool CMOOSProcWin32::Stop(bool bGentle)
{
    m_pWin32Proc->vTerminate();
    m_pWin32Proc->vWaitForTerminate(100);
    return true;
}

bool CMOOSProcWin32::IsStopped()
{
    return m_pWin32Proc->dwGetExitCode() != STILL_ACTIVE;
}



#else // NIX DEFINITIONS




bool CMOOSProcNix::BuildConfigSpecific()
{
    return true;
}

bool CMOOSProcConsoleNix::BuildConfigSpecific()
{
    string sLaunchConfigurationName;
    string sLaunchConfiguration;

    
    if (m_cfg.GetParamValue("XConfig", sLaunchConfigurationName))
    {
        //OK look for this configuration string
        if (!m_MissionReader->GetConfigurationParam(sLaunchConfigurationName, 
                                                    sLaunchConfiguration))
            return MOOSFail("   warning: could not find resource string called \"%s\"\n",
                            sLaunchConfigurationName.c_str()) ;
    }
    else
    {
        //some applications are v.important in MOOS 
        // -- if not told otherwise they get special colours
        string sBgColor = "#DDDDFF";
        string sFgColor = "#000000";
        
        if (IsIRemote())
        {
            sBgColor = "#CC0000";
            sFgColor = "#FFFFFF";
        }

        if (IsMOOSDB())
        {
            sBgColor = "#003300";
            sFgColor = "#FFFFFF";
        }

        string sLabel = m_cfg.m_sMOOSName.empty()
            ? ""
            : MOOSFormat("as MOOSName \"%s\"", m_cfg.m_sMOOSName.c_str()).c_str();
        
        sLaunchConfiguration = "-geometry," 
            + MOOSFormat("80x12+2+%d", (*(m_cfg.m_nCurrentLaunch)) * 50)
            + ",+sb,"
            + ",-fg," + sFgColor
            + ",-bg," + sBgColor
            + ",-T," + MOOSFormat("%s %s", GetAppName().c_str(), sLabel.c_str());

    }
    
    //OK now simply chomp our way through a space delimited list...
    while (!sLaunchConfiguration.empty())
    {
        MOOSTrimWhiteSpace(sLaunchConfiguration);
        // convert commas to spaces
        string sP = MOOSChomp(sLaunchConfiguration, ",");
        MOOSTrimWhiteSpace(sP);
        if (!sP.empty())
            m_ConsoleLaunchParameters.push_back(sP);
    }
    
    return true;

}

bool CMOOSProcScreenNix::BuildConfigSpecific()
{
    string sLaunchConfigurationName;
    string sLaunchConfiguration;

    
    if (m_cfg.GetParamValue("ScreenConfig", sLaunchConfigurationName))
    {
        //OK look for this configuration string
        // likely they would add -L for loggging, or -l/-ln for shell login options
        if (!m_MissionReader->GetConfigurationParam(sLaunchConfigurationName, 
                                                    sLaunchConfiguration))
            return MOOSFail("   warning: could not find resource string called \"%s\"\n",
                            sLaunchConfigurationName.c_str()) ;
    }
    
    //OK now simply chomp our way through a space delimited list...
    while (!sLaunchConfiguration.empty())
    {
        MOOSTrimWhiteSpace(sLaunchConfiguration);
        // convert commas to spaces
        string sP = MOOSChomp(sLaunchConfiguration, ",");
        MOOSTrimWhiteSpace(sP);
        if (!sP.empty())
            m_ScreenLaunchParameters.push_back(sP);
    }
    return true;
}


bool CMOOSProcNixBase::Start(bool bGentle)
{

    //make a child process
    this->m_ChildPID = fork();
    if (this->m_ChildPID < 0)
    {
        //hell!
        MOOSTrace("fork failed, not good\n");
        return false;
    }

    if (this->m_ChildPID == 0)
    {
        //I'm the child now..
    
        // build m_FullCommandLine according to launch type
        m_FullCommandLine.clear();
        this->StartSpecific();
        
        if (!m_FullCommandLine.size())
        {
            MOOSTrace("pAntler author error: no elements in m_FullCommandLine!\n");
            return false;
        }
        
        // CONVERT STRING LIST TO ARRAY 
        const char ** pExecVParams = new const char* [m_FullCommandLine.size() + 1];
        int i = 0;
        for (STRING_LIST::iterator s = this->m_FullCommandLine.begin();
             s != this->m_FullCommandLine.end();
             ++s)
        {
            pExecVParams[i++] = s->c_str();
        }
        
        //terminate list
        pExecVParams[i++] = NULL;
        if(DEBUG_LAUNCH)
        {
            for (int j = 0; j < i; j++)
                MOOSTrace("argv[%d]:\"%s\"\n", j, pExecVParams[j]);
        }
        
        //and finally replace ourselves with a new xterm process image
	char * const * pParamList = const_cast<char * const *> (pExecVParams);
        if (execvp(pExecVParams[0], pParamList) == -1)
        {
            MOOSTrace("Failed exec - not good. Called exec as follows:\n");
            exit(EXIT_FAILURE);
            
        }
        
    }
    
    //Parent execution stream continues here...
    //we need to change the process group of the spawned process so that ctrl-C
    //does get sent to all the launched xterms. Instead we will catch ctrl-C ourself
    //and find the MOOS process running within the sub process, signal it to die
    //so subprocesses can clean up nicely
    if (bGentle)
        setpgid(this->m_ChildPID, 0);
    
    return true;

}

bool CMOOSProcNix::StartSpecific()
{
    
    m_FullCommandLine.push_back(m_cfg.m_sAppPath);
    
    if (!this->m_bInhibitMOOSParams)
    {
        //we do the usual thing of supplying Mission file and MOOSName
        
        m_FullCommandLine.push_back(this->MissionFile());
        m_FullCommandLine.push_back(m_cfg.m_sMOOSName); 
    }
    
    //here we pass extra parameters to the MOOS process if required
    m_FullCommandLine.splice(m_FullCommandLine.end(), m_CommandLineParameters);

    
    return true;
}

bool CMOOSProcConsoleNix::StartSpecific()
{

    // xterm <params> -e <commandline>
    m_FullCommandLine.push_back(DEFAULT_NIX_TERMINAL);
    m_FullCommandLine.splice(m_FullCommandLine.end(), m_ConsoleLaunchParameters);
    m_FullCommandLine.push_back("-e");
        

    // this is the same as the plain-vanilla nix launch
    m_FullCommandLine.push_back(m_cfg.m_sAppPath);
    
    if (!this->m_bInhibitMOOSParams)
    {
        //we do the usual thing of supplying Mission file and MOOSName
        
        m_FullCommandLine.push_back(this->MissionFile());
        m_FullCommandLine.push_back(m_cfg.m_sMOOSName); 
    }
    
    //here we pass extra parameters to the MOOS process if required
    m_FullCommandLine.splice(m_FullCommandLine.end(), m_CommandLineParameters);
    
    return true;
}

bool CMOOSProcScreenNix::StartSpecific()
{
    // screen -dmS <sessionname> <commandline>
    m_FullCommandLine.push_back("screen");
    m_FullCommandLine.push_back("-dmS");
    m_FullCommandLine.push_back(m_cfg.m_sMOOSName);

    m_FullCommandLine.splice(m_FullCommandLine.end(), m_ScreenLaunchParameters);
        

    // this is the same as the plain-vanilla nix launch
    m_FullCommandLine.push_back(m_cfg.m_sAppPath);
    
    if (!this->m_bInhibitMOOSParams)
    {
        //we do the usual thing of supplying Mission file and MOOSName
        
        m_FullCommandLine.push_back(this->MissionFile());
        m_FullCommandLine.push_back(m_cfg.m_sMOOSName); 
    }
    
    //here we pass extra parameters to the MOOS process if required
    m_FullCommandLine.splice(m_FullCommandLine.end(), m_CommandLineParameters);
    
    return true;
}


bool CMOOSProcNixBase::Stop(bool bGentle)
{
    bool ret = this->StopSpecific(bGentle);

    //just give it a little time - for pities sake - no need for this pause
    MOOSPause(300); 

    return ret;
}

bool CMOOSProcNix::StopSpecific(bool bGentle)
{
    kill(m_ChildPID, SIGTERM);

    return true;
}

bool CMOOSProcConsoleNix::StopSpecific(bool bGentle)
{
    if (bGentle)
    {
        kill(m_ChildPID, SIGTERM);
    }
    else
    {
        //we need to be crafty....
        string sCmd = "ps -e -o ppid= -o pid=";
	
        FILE* In = popen(sCmd.c_str(), "r");
	
        if (In == NULL) return false;
        
        bool bFound = false;
        char Line[256];
        while (fgets(Line, sizeof(Line), In))
        {
            stringstream L(Line);
            int ppid, pid;
            L >> ppid;
            L >> pid;
            
            if (m_ChildPID == ppid)
            {
                kill(pid, SIGTERM);
                bFound = true;
            }
        }	
        pclose(In);
        return bFound;
    }

    return true;
}

bool CMOOSProcScreenNix::StopSpecific(bool bGentle)
{
    if (bGentle)
    {
        kill(m_ChildPID, SIGTERM);
    }
    else
    {
        //we need to be crafty....
        string sCmd = "ps -e -o ppid= -o pid=";
	
        FILE* In = popen(sCmd.c_str(), "r");
	
        if (In == NULL) return false;
        
        bool bFound = false;
        char Line[256];
        while (fgets(Line, sizeof(Line), In))
        {
            stringstream L(Line);
            int ppid, pid;
            L >> ppid;
            L >> pid;
            
            if (m_ChildPID == ppid)
            {
                kill(pid, SIGTERM);
                bFound = true;
            }
        }	
        pclose(In);
        return bFound;
    }

    return true;
}


bool CMOOSProcNixBase::IsStopped()
{
    int nStatus = 0;
    return waitpid(this->m_ChildPID, &nStatus, WNOHANG) > 0;
}


#endif        
