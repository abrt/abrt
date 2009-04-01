/*
    CCpp.cpp

    Copyright (C) 2009  Zdenek Prikryl (zprikryl@redhat.com)
    Copyright (C) 2009  RedHat inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
    */

#include "CCpp.h"
#include <fstream>
#include "DebugDump.h"
#include "Settings.h"
#include <sstream>
#include <iostream>
#include <hash_map>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#define CORE_PATTERN_IFACE "/proc/sys/kernel/core_pattern"
#define CORE_PATTERN "|"CCPP_HOOK_PATH" %p %s %u"

CAnalyzerCCpp::CAnalyzerCCpp() :
	m_bMemoryMap(false)
{}

void CAnalyzerCCpp::InstallDebugInfos(const std::string& pPackage)
{
    char buff[1024];
    int pipein[2], pipeout[2];
    struct timeval delay;
    fd_set rsfd;
    pid_t child;

    pipe(pipein);
    pipe(pipeout);

    fcntl(pipein[0], F_SETFD, FD_CLOEXEC);
    fcntl(pipeout[1], F_SETFD, FD_CLOEXEC);

    child = fork();
    if (child < 0)
    {
        throw std::string("CAnalyzerCCpp::RunGdb():  fork failed.");
    }
    if (child == 0)
    {
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);

        dup2(pipein[0], STDIN_FILENO);
        close(pipein[0]);
        dup2(pipeout[1], STDOUT_FILENO);
        close(pipeout[1]);

        setsid();
        execlp("debuginfo-install", "debuginfo-install", pPackage.c_str(), NULL);
        exit(0);
    }

    close(pipein[0]);
    close(pipeout[1]);

    bool quit = false;

    while(!quit)
    {
        FD_ZERO(&rsfd);

        FD_SET(pipeout[0], &rsfd);

        delay.tv_sec = 1;
        delay.tv_usec = 0;

        if(select(FD_SETSIZE, &rsfd, NULL, NULL, &delay) > 0)
        {
            if(FD_ISSET(pipeout[0], &rsfd))
            {
                int r = read(pipeout[0], buff, sizeof(buff));
                if (r <= 0)
                {
                    quit = true;
                }
                else
                {
                    buff[r] = '\0';
                    std::cerr << buff;
                    if (strstr(buff, "already installed and latest version") != NULL)
                    {
                        break;
                    }
                    if (strstr(buff, "No debuginfo packages available to install") != NULL ||
                        strstr(buff, "Could not find debuginfo for main pkg") != NULL ||
                        strstr(buff, "Could not find debuginfo pkg for dependency package") != NULL)
                    {
                        close(pipein[1]);
                        close(pipeout[0]);
                        kill(child, SIGTERM);
                        wait(NULL);
                        throw std::string("CAnalyzerCCpp::InstallDebugInfos(): cannot install debuginfos for ") + pPackage;
                    }
                    if (strstr(buff, "Total download size") != NULL)
                    {
                        int r =  write(pipein[1], "y\n", sizeof("y\n"));
                        if (r != sizeof("y\n"))
                        {
                            close(pipein[1]);
                            close(pipeout[0]);
                            kill(child, SIGTERM);
                            wait(NULL);
                            throw std::string("CAnalyzerCCpp::InstallDebugInfos(): cannot install debuginfos for ") + pPackage;
                        }
                    }
                }
            }
        }
    }
    close(pipein[1]);
    close(pipeout[0]);

    wait(NULL);
}

void CAnalyzerCCpp::GetBacktrace(const std::string& pDebugDumpDir, std::string& pBacktrace)
{
    std::string tmpFile = "/tmp/" + pDebugDumpDir.substr(pDebugDumpDir.rfind("/"));
    std::ofstream fTmp;
    std::string UID;
    fTmp.open(tmpFile.c_str());
    if (fTmp.is_open())
    {
        std::string executable;
        CDebugDump dd;
        dd.Open(pDebugDumpDir);
        dd.LoadText(FILENAME_EXECUTABLE, executable);
        dd.LoadText(FILENAME_UID, UID);
        dd.Close();
        fTmp << "file " << executable << std::endl;
        fTmp << "core " << pDebugDumpDir << "/" << FILENAME_BINARYDATA1 << std::endl;
        fTmp << "bt" << std::endl;
        fTmp << "q" << std::endl;
        fTmp.close();
    }
    else
    {
        throw "CAnalyzerCCpp::GetBacktrace(): cannot create gdb script " + tmpFile ;
    }

    RunGdb(tmpFile, UID, pBacktrace);
}

void CAnalyzerCCpp::GetIndependentBacktrace(const std::string& pBacktrace, std::string& pIndependentBacktrace)
{
    int ii = 0;
    while (ii < pBacktrace.length())
    {
        std::string line = "";
        int jj = 0;

        while (pBacktrace[ii] != '\n' && ii < pBacktrace.length())
        {
            line += pBacktrace[ii];
            ii++;
        }
        while (isspace(line[jj]))
        {
            jj++;
        }
        if (line[jj] == '#')
        {
            while(jj < line.length())
            {
                if (isspace(line[jj]))
                {
                    jj++;
                }
                else if (line[jj] == '0' && line[jj+1] == 'x')
                {
                    while (isalnum(line[jj]))
                    {
                        jj++;
                    }
                }
                else
                {
                    pIndependentBacktrace += line[jj];
                    jj++;
                }
            }
        }
        ii++;
    }
}

void CAnalyzerCCpp::RunGdb(const std::string& pScript, const std::string pUID, std::string& pOutput)
{
    int pipeout[2];
    char buff[1024];
    struct timeval delay;
    fd_set rsfd;
    pid_t child;

    pipe(pipeout);
    fcntl(pipeout[1], F_SETFD, FD_CLOEXEC);

    child = fork();
    if (child == -1)
    {
        throw std::string("CAnalyzerCCpp::RunGdb():  fork failed.");
    }
    if(child == 0)
    {
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);

        dup2(pipeout[1], STDOUT_FILENO);
        close(pipeout[1]);

        setuid(atoi(pUID.c_str()));
        seteuid(atoi(pUID.c_str()));
        setsid();

        execlp("gdb", "gdb","-batch", "-x", pScript.c_str(), NULL);
        exit(0);
    }

    close(pipeout[1]);

    bool quit = false;

    while(!quit)
    {
        FD_ZERO(&rsfd);
        FD_SET(pipeout[0], &rsfd);

        delay.tv_sec = 1;
        delay.tv_usec = 0;

        if(select(FD_SETSIZE, &rsfd, NULL, NULL, &delay) > 0)
        {
            if(FD_ISSET(pipeout[0], &rsfd))
            {
                int r = read(pipeout[0], buff, sizeof(buff));
                if (r <= 0)
                {
                    quit = true;
                }
                else
                {
                    buff[r] = '\0';
                    pOutput += buff;
                }
            }
        }
    }
    close(pipeout[0]);
    wait(NULL);
}

std::string CAnalyzerCCpp::GetLocalUUID(const std::string& pDebugDumpDir)
{

	std::stringstream ss;
	char* core;
	unsigned int size;
	std::string executable;
	CDebugDump dd;

	dd.Open(pDebugDumpDir);
	dd.LoadBinary(FILENAME_BINARYDATA1, &core, &size);
	dd.LoadText(FILENAME_EXECUTABLE, executable);
	dd.Close();
	// TODO: compute local UUID, remove this hack
	ss << executable << "_" << size;

	return ss.str();
}
std::string CAnalyzerCCpp::GetGlobalUUID(const std::string& pDebugDumpDir)
{
    std::stringstream ss;
    std::string backtrace;
    std::string independentBacktrace;
    __gnu_cxx::hash<const char*> hash;

    CDebugDump dd;
    dd.Open(pDebugDumpDir);
    dd.LoadText(FILENAME_TEXTDATA1, backtrace);
    dd.Close();
    GetIndependentBacktrace(backtrace, independentBacktrace);
    // TODO: compute global UUID, remove this hack
    ss << hash(independentBacktrace.c_str());
    return ss.str();
}

void CAnalyzerCCpp::CreateReport(const std::string& pDebugDumpDir)
{
    std::string package;
    std::string backtrace;
    CDebugDump dd;
    dd.Open(pDebugDumpDir);
    if (dd.Exist(FILENAME_TEXTDATA1))
    {
        dd.Close();
        return;
    }
    dd.LoadText(FILENAME_PACKAGE, package);
    dd.Close();

    InstallDebugInfos(package);

    GetBacktrace(pDebugDumpDir, backtrace);

    dd.Open(pDebugDumpDir);
    dd.SaveText(FILENAME_TEXTDATA1, backtrace);
    if (m_bMemoryMap)
    {
        dd.SaveText(FILENAME_TEXTDATA2, "memory map of the crashed C/C++ application, not implemented yet");
    }
    dd.Close();
}

void CAnalyzerCCpp::Init()
{
	std::ifstream fInCorePattern;
	fInCorePattern.open(CORE_PATTERN_IFACE);
	if (fInCorePattern.is_open())
	{
		getline(fInCorePattern, m_sOldCorePattern);
		fInCorePattern.close();
	}
	std::ofstream fOutCorePattern;
	fOutCorePattern.open(CORE_PATTERN_IFACE);
	if (fOutCorePattern.is_open())
	{
		fOutCorePattern << CORE_PATTERN << std::endl;
		fOutCorePattern.close();
	}
}


void CAnalyzerCCpp::DeInit()
{
	std::ofstream fOutCorePattern;
	fOutCorePattern.open(CORE_PATTERN_IFACE);
	if (fOutCorePattern.is_open())
	{
		fOutCorePattern << m_sOldCorePattern << std::endl;
		fOutCorePattern.close();
	}
}

void CAnalyzerCCpp::LoadSettings(const std::string& pPath)
{
    map_settings_t settings;
    load_settings(pPath, settings);

    if (settings.find("MemoryMap")!= settings.end())
      {
          m_bMemoryMap = settings["MemoryMap"] == "yes";
      }
}
