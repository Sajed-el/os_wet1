#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include "Commands.h"
#include <regex>
#include <fcntl.h>
#include <algorithm>
#include <sys/utsname.h>
#include <ctime>
#include <sys/sysinfo.h>
#include <limits.h>
#include <sys/stat.h>

using namespace std;

const std::string WHITESPACE = " \n\r\t\f\v";
std::string original_cmd_line;
#if 0
#define FUNC_ENTRY()  \
  cout << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT()  \
  cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif


std::string is_IO_cmd ;
std::string file_out ;


std::string SmallShell::Prompt = "smash> ";




string _ltrim(const std::string &s) {
    size_t start = s.find_first_not_of(WHITESPACE);
    return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string &s) {
    size_t end = s.find_last_not_of(WHITESPACE);
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string &s) {
    return _rtrim(_ltrim(s));
}

int _parseCommandLine(const char *cmd_line, char **args) {
    FUNC_ENTRY()
    int i = 0;
    std::istringstream iss(_trim(string(cmd_line)).c_str());
    for (std::string s; iss >> s;) {
        args[i] = (char *) malloc(s.length() + 1);
        memset(args[i], 0, s.length() + 1);
        strcpy(args[i], s.c_str());
        args[++i] = NULL;
    }
    return i;
    FUNC_EXIT()
}

bool _isBackgroundComamnd(const char *cmd_line) {
    const string str(cmd_line);
    return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void _removeBackgroundSign(char *cmd_line) {
    const string str(cmd_line);
    // find last character other than spaces
    unsigned int idx = str.find_last_not_of(WHITESPACE);
    // if all characters are spaces then return
    if (idx == string::npos) {
        return;
    }
    // if the command line does not end with & then return
    if (cmd_line[idx] != '&') {
        return;
    }
    // replace the & (background sign) with space and then remove all tailing spaces.
    cmd_line[idx] = ' ';
    // truncate the command line string up to the last non-space character
    cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}

// TODO: Add your implementation for classes in Commands.h

Command::Command(const char *cmd_line) {
    {
        if(!cmd_line){
            this->cmd_line = nullptr;
            return;
        }
        this->cmd_line = new char[strlen(cmd_line) + 1];
        strcpy(this->cmd_line,cmd_line);
        argsNum = _parseCommandLine(this->cmd_line,this->args);

    }
}

BuiltInCommand::BuiltInCommand(const char *cmd_line): Command(cmd_line) {
    _removeBackgroundSign(this->cmd_line);
    argsNum = _parseCommandLine(this->cmd_line,this->args);
}

changePrompt::changePrompt(const char *cmd_line): BuiltInCommand(cmd_line) {
}

void changePrompt::execute() {
    if(argsNum >1){
        SmallShell::Prompt = std::string(args[1]) + "> ";
        return;
    }
    SmallShell::Prompt = "smash> ";
    return;
}

GetCurrDirCommand::GetCurrDirCommand(const char *cmd_line) : BuiltInCommand(cmd_line){}

void GetCurrDirCommand::execute() {
  char *res;
  res = getcwd(nullptr,0);
  if(res){
      int i = 0;
      while(res[i]){
          cout<<res[i];
          i++;
      }
      cout<<endl;
  }else {
      perror("smash error: getcwd failed");
  }
}

ShowPidCommand::ShowPidCommand(const char *cmd_line) : BuiltInCommand(cmd_line){}

void ShowPidCommand::execute() {
    pid_t pid = getpid();
    cout << "smash pid is " << pid << endl;
}

ChangeDirCommand::ChangeDirCommand(const char *cmd_line, char **plastPwd)
    : BuiltInCommand(cmd_line),prev_dir(plastPwd) {}

void ChangeDirCommand::execute() {
    if (argsNum > 2) {
        cerr << "smash error: cd: too many arguments" << endl;
    }
    else if (argsNum > 1 && strcmp(args[1],"-") == 0){
        if (*prev_dir == nullptr) {
            cerr << "smash error: cd: OLDPWD not set" << endl;
        }
        else {
            char* prev_dir_holder = getcwd(nullptr, 0);
            int res = chdir(*prev_dir);
            if (res == -1){
                perror("smash error: chdir failed");
            }
            else{
                *prev_dir = prev_dir_holder;
            }
        }
    }
    else if (argsNum > 1) {
        char* prev_dir_holder = getcwd(nullptr, 0);
        int res = chdir(args[1]);
        if (res == -1){
            perror("smash error: chdir failed");
        }
        else{
            *prev_dir = prev_dir_holder;
        }
    }

}

ForegroundCommand::ForegroundCommand(const char *cmd_line, JobsList *jobs)
    : BuiltInCommand(cmd_line), jobs(jobs) {}

void ForegroundCommand::execute() {
    int jobId = 0;
    if (argsNum > 2) {
        cerr << "smash error: fg: invalid arguments" << endl;
        return;
    }
    else if (argsNum == 1){
        if (!(jobs->getLastJob(&jobId))){
            cerr << "smash error: fg: jobs list is empty" << endl;
            return;
        }
        else {
            JobsList::JobEntry *jobToBring = jobs->getLastJob(&jobId);
            pid_t jobPid = jobToBring->getJobPid();
            cout << _trim(jobToBring->cmd->origianl_cmd) << " " << jobPid << endl;
            jobs->removeJobById(jobId);
            if (waitpid(jobPid,nullptr, 0) == -1){
                perror("smash error: waitpid failed");
            }
        }
    }
    else if (argsNum == 2){
        try {
        jobId = std::stoi(args[1]);
        }
        catch (...) {
            cerr << "smash error: fg: invalid arguments" << endl;
            return;
        }

        if (!(jobs->getJobById(jobId))){
            cerr << "smash error: fg: job-id " << jobId << " does not exist" << endl;
            return;
        }
        else {
            JobsList::JobEntry *jobToBring = jobs->getJobById(jobId);
            pid_t jobPid = jobToBring->getJobPid();
            cout << _trim(jobToBring->cmd->origianl_cmd) << " " << jobPid << endl;
            jobs->removeJobById(jobId);
            if (waitpid(jobPid,nullptr, 0) == -1){
                perror("smash error: waitpid failed");
            }
        }
    }


}

KillCommand::KillCommand(const char *cmd_line, JobsList *jobs)
    : BuiltInCommand(cmd_line), jobs(jobs) {}

void KillCommand::execute() {
    int jobId = 0;
    int killSignal = 0;
    if (argsNum > 3 || argsNum < 3) {
        cerr << "smash error: kill: invalid arguments" << endl;
        return;
    }
    else if (argsNum == 3){
        std::string signalString = args[1];
        if (signalString.length() < 2 || signalString[0] != '-'){
            cerr << "smash error: kill: invalid arguments" << endl;
            return;
        }
        try {
            jobId = std::stoi(args[2]);
            killSignal = std::stoi(signalString.substr(1));
        }
        catch (...) {
            cerr << "smash error: kill: invalid arguments" << endl;
            return;
        }
        if (!(jobs->getJobById(jobId))){
            cerr << "smash error: kill: job-id " << jobId << " does not exist" << endl;
            return;
        }
        else {
            pid_t jobPid = jobs->getJobById(jobId)->getJobPid();
            int res = kill(jobPid,killSignal);
            if (res == -1){
                perror("smash error: kill failed");
            }
            else {
                cout << "signal number " << killSignal << " was sent to pid " << int(jobPid) << endl;
            }
        }
    }

}

UnAliasCommand::UnAliasCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}

void UnAliasCommand::execute() {
    if (argsNum == 1){
        cerr << "smash error: unalias: not enough arguments" << endl;
        return;
    }
    std::map<std::string,std::string> &tempAliasMap = SmallShell::getInstance().aliasMap;
    std::vector<std::string> &tempAliasVec = SmallShell::getInstance().aliasCmdOrder;
    for (int i = 1; i < argsNum; i++){
        std::string aliasToRemove = args[i];
        if (tempAliasMap.find(aliasToRemove) == tempAliasMap.end()){
            cerr << "smash error: unalias: " << aliasToRemove  << " alias does not exist" << endl;
            return;
        }
        tempAliasMap.erase(aliasToRemove);
        auto it = std::find(tempAliasVec.begin(), tempAliasVec.end(), aliasToRemove);
        if (it != tempAliasVec.end()){
            tempAliasVec.erase(it);
        }
    }
}

SysInfoCommand::SysInfoCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}

void SysInfoCommand::execute() {
    struct utsname uts;
    if (uname(&uts) == -1){
        perror("smash error: uname failed");
        return;
    }
    char buffer[4096];
    int fd = open("/proc/stat", O_RDONLY);
    if (fd == -1){
        perror("smash error: open failed");
        return;
    }
    int size = read(fd, buffer, sizeof(buffer)-1);
    if (size == -1){
        perror("smash error: read failed");
        close(fd);
        return;
    }
    buffer[size] = '\0';
    close(fd);
    std::string infoString = std::string(buffer);
    size_t pos = infoString.find("btime ");
    time_t bootTime = 0;
    if (pos != std::string::npos){
        size_t posStart = pos + 6;
        size_t posEnd = infoString.find("\n",posStart);
        std::string bootTimeString = infoString.substr(posStart,posEnd-posStart);
        bootTime = std::stoll(bootTimeString);
    }
    struct tm *sysBootTime = localtime(&bootTime);
    char bufferTime[30];
    strftime(bufferTime, 30, "%Y-%m-%d %H:%M:%S", sysBootTime);
    cout << "System: " << uts.sysname << endl;
    cout << "Hostname: " << uts.nodename << endl;
    cout << "Kernel: " << uts.release << endl;
    cout << "Architecture: " << uts.machine << endl;
    cout << "Boot Time: " << bufferTime << endl;

}

PipeCommand::PipeCommand(const char *cmd_line) : Command(cmd_line) {}

void PipeCommand::execute() {
	std::string cmdLine = string(cmd_line);
	std::string cmdWrite, cmdRead;
	bool stdErrMode = false;
	size_t pos = cmdLine.find("|&");
	if (pos != std::string::npos){
		stdErrMode = true;
		cmdWrite = cmdLine.substr(0, pos);
		cmdRead = cmdLine.substr(pos + 2);
	}
	else {
		pos = cmdLine.find("|");
		if (pos != std::string::npos){
			cmdWrite = cmdLine.substr(0, pos);
			cmdRead = cmdLine.substr(pos + 1);
		}
	}
	int fd[2];
	if (pipe(fd) == -1){
		perror("smash error: pipe failed");
		return;
	}
	pid_t pidWrite = fork();
	if (pidWrite == -1){
		perror("smash error: fork failed");
		return;
	}
	if (pidWrite == 0){
		setpgrp();
		if (stdErrMode){
			dup2(fd[1], STDERR_FILENO);
		}
		else {
			dup2(fd[1], STDOUT_FILENO);
		}
		close(fd[0]);
		close(fd[1]);
		SmallShell::getInstance().executeCommand(cmdWrite.c_str());
        exit(0);
	}
	pid_t pidRead = fork();
	if (pidRead == -1){
		perror("smash error: fork failed");
		return;
	}
	if (pidRead == 0){
		setpgrp();
		dup2(fd[0], STDIN_FILENO);
		close(fd[0]);
		close(fd[1]);
		SmallShell::getInstance().executeCommand(cmdRead.c_str());
	    exit(0);
	}
	close(fd[0]);
	close(fd[1]);
	if (waitpid(pidRead, nullptr, 0) == -1){
		perror("smash error: waitpid failed");
	}
	if (waitpid(pidWrite, nullptr, 0) == -1){
		perror("smash error: waitpid failed");
	}
}

WhoAmICommand::WhoAmICommand(const char *cmd_line) : Command(cmd_line) {}

void WhoAmICommand::execute() {
	uid_t uid = geteuid();
	gid_t gid = getegid();
    pid_t my_pid = getpid();
    std::string path = "/proc/"+ std::to_string(my_pid) +"/environ";
    int openSuc = open(path.c_str(),O_RDONLY);
    if(openSuc == -1){
        perror("smash error: open failed");
        return;
    }
    char buff[100];
    int readSuc = read(openSuc,buff,100);
    std::vector<string> fileVar;
    std::string word="";
    while(readSuc > 0){
        for (int k = 0; k < readSuc; ++k) {
            if(buff[k] != '\0'){
                word += buff[k];
            }else{
                fileVar.push_back(word);
                word = "";
            }
        }
        readSuc = read(openSuc,buff,100);
    }
    if(close(openSuc) == -1){
        perror("smash error: close failed");
    }
    std::string username = "";
    std::string homeDir = "";
    for (const auto& it : fileVar) {
        if (it.rfind("USER=", 0) == 0) {
            username = it.substr(5);
        }
        if (it.rfind("HOME=", 0) == 0) {
            homeDir = it.substr(5);
        }
    }

	cout << username << endl;
	cout << uid << endl;
	cout << gid << endl;
	cout << homeDir << endl;
}


RedirectionCommand::RedirectionCommand(const char *cmd_line) : Command(cmd_line) {}

void RedirectionCommand::execute() {
    original_cmd_line = cmd_line;
    std::string IO_cmd_s = string(cmd_line);
    std::string final_cmd_line;
    size_t pos = IO_cmd_s.find(">>");
    if(pos != std::string::npos ){
        file_out = IO_cmd_s.substr(pos + 2 );
        file_out = _trim(file_out);
        IO_cmd_s = IO_cmd_s.substr(0,pos);
        final_cmd_line = IO_cmd_s.c_str();
        is_IO_cmd = ">>";
    }else {
        pos = IO_cmd_s.find(">");
        if(pos != std::string::npos ){
            file_out = IO_cmd_s.substr(pos + 1);
            file_out = _trim(file_out);
            IO_cmd_s = IO_cmd_s.substr(0,pos);
            final_cmd_line = IO_cmd_s.c_str();
            is_IO_cmd = ">";
        }else{
            return;
        }
    }
    int std_out_new = -1,fd;
    if(!is_IO_cmd.empty()){

        if(is_IO_cmd == ">") {
            fd = open(file_out.c_str(), O_WRONLY | O_CREAT|O_TRUNC, 0666);
        }else{
            fd = open(file_out.c_str(), O_WRONLY|O_APPEND | O_CREAT, 0666);
        }
        if (fd == -1) {
            perror("smash error: open failed");
            return;
        }
        std_out_new = dup(1);
        if(std_out_new == -1){
            perror("smash error: dup failed");
            return;
        }
        if(dup2(fd,1) == -1){
            perror("smash error: dup2 failed");
            return;
        }
    }

    SmallShell::getInstance().executeCommand(final_cmd_line.c_str());
    if(std_out_new != -1){
        if(dup2(std_out_new,1) == -1){
            perror("smash error: dup2 failed");
            close(std_out_new);
            close(fd);
            return;
        }
        close(std_out_new);
        close(fd);
    }
}


AliasCommand::AliasCommand(const char *cmd_line) : BuiltInCommand(cmd_line){
    string cmd_s = _trim(string(this->cmd_line));
     legalAlias = std::regex_match(cmd_s,std::regex("^alias [a-zA-Z0-9_]+='[^']*'$"));
     this->cmd = cmd_s;



}
bool isReservedKeyword(const std::string& word) {
    return (word == "chprompt" || word == "pwd"   || word == "cd"   ||
            word == "alias"    || word == "unalias"|| word == "quit" ||
            word == "jobs"     || word == "fg"     || word == "bg"   ||
            word == "kill");
}

void AliasCommand::execute() {
    std::map<std::string,std::string> &tempAliasMap = SmallShell::getInstance().aliasMap ;
    std::vector<std::string> &tempAliasVec = SmallShell::getInstance().aliasCmdOrder;
    if(cmd == "alias"){
        // print the alias
        for(const auto& name : tempAliasVec) {
            std::cout << name <<"='"<<tempAliasMap[name]<<"'" << std::endl;
        }
        return;
    }
    else if(!legalAlias){
        cerr<<"smash error: alias: invalid alias format"<<endl;

        return;
    }
        size_t equalPos = cmd.find("=");
        string shortCut = this->cmd.substr(6,equalPos - 6);
        if(tempAliasMap.find(shortCut) != tempAliasMap.end() ||  isReservedKeyword(shortCut)) {
            cerr << "smash error: alias: " << shortCut << " already exists or is a reserved command" << endl;
            return;
        }
        size_t aliasCmdStart = equalPos + 2;
        size_t aliasCmdEnd = cmd.length() - 1 - aliasCmdStart;
        string alaisCmd = this->cmd.substr(aliasCmdStart,aliasCmdEnd);
        tempAliasMap[shortCut] = alaisCmd;
        tempAliasVec.push_back(shortCut);

}
UnSetEnvCommand::UnSetEnvCommand(const char *cmd_line) : BuiltInCommand(cmd_line){}

extern char **__environ;
void UnSetEnvCommand::execute() {
    if(argsNum == 1){
        cerr<<"smash error: unsetenv: not enough arguments"<<endl;
        return;
    }
    pid_t my_pid = getpid();
    std::string path = "/proc/"+ std::to_string(my_pid) +"/environ";
    int openSuc = open(path.c_str(),O_RDONLY);
    if(openSuc == -1){
        perror("smash error: open failed");
        return;
    }
    char buff[100];
    int readSuc = read(openSuc,buff,100);
    std::vector<string> fileVar;
    std::string word="";
    while(readSuc > 0){
        for (int k = 0; k < readSuc; ++k) {
            if(buff[k] != '\0'){
                word += buff[k];
            }else{
                fileVar.push_back(word);
                word = "";
            }
        }
        readSuc = read(openSuc,buff,100);
    }
    if(close(openSuc) == -1){
        perror("smash error: close failed");
    }
    int j = 1;
    while(j < argsNum){
        bool exist = false;
        std::string varPrefix;
        for (const auto& var :fileVar) {
            varPrefix = string(args[j]) + "=";
            if(var.rfind(varPrefix,0) == 0){
                exist = true;
                break;
            }
        }
        if(!exist){
            cerr <<"smash error: unsetenv: "<<args[j]<<" does not exist"<<endl;
            return;
        }
        int i = 0;
        bool located = false;
        while(__environ[i] != nullptr){
            if(located){
                __environ[i] = __environ[i+1];
                if(environ == nullptr){
                    break;
                }
            }else{
                std::string env_var = string(__environ[i]);
                if(env_var.rfind(varPrefix,0) == 0 ){
                    located = true;
                    __environ[i] = __environ[i+1];
                    if(__environ[i] == nullptr){
                        break;
                    }

                }
            }
            i++;
        }
        j++;

    }

}

void JobsList::removeFinishedJobs() {
    int status;
    auto it = jobList.begin();
    while(it != jobList.end()){
        pid_t pid = waitpid(it->jobPid,&status,WNOHANG);
        if(pid > 0){
            it = jobList.erase(it);
        }else {
            it++;
        }
    }

    }



void JobsList::addJob(Command *cmd,pid_t jobPid,bool isStopped) {
   //handle signal ctrl+c
    int status;
    size_t pid_BG = waitpid(jobPid,&status,WNOHANG);
    //in case child process that finished before adding it to  the joblist(happends when the child runs first)
    if(pid_BG > 0){
        delete cmd;
        return;
    }
    int max = 1;
    for(const auto& job : jobList){
        if(max <= job.jobId){
            max = job.jobId + 1;
        }
    }
    jobList.push_back(JobEntry (cmd,max,jobPid));

}

void JobsList::removeJobById(int jobId) {
    std::vector<JobEntry> &tempJobList = SmallShell::getInstance().ShellJobList.jobList;
    auto it = std::find_if(tempJobList.begin(),tempJobList.end(),[jobId](const JobEntry& job){
        return job.jobId == jobId;
    });
    if(it != tempJobList.end()){
        tempJobList.erase(it);
    }
}

JobsList::JobEntry *JobsList::getJobById(int jobId) {
    std::vector<JobEntry> &tempJobList = SmallShell::getInstance().ShellJobList.jobList;
    auto it = std::find_if(tempJobList.begin(),tempJobList.end(),[jobId](const JobEntry& job){
        return job.jobId == jobId;
    });
    if(it != tempJobList.end()){
        return &(*it);
    }
    return nullptr;

}

void JobsList::killAllJobs() {
      for (auto &job : jobList){
          if(kill(job.jobPid,9) == -1){
              perror("smash error: kill failed");
          }
      }
}

JobsList::JobEntry *JobsList::getLastStoppedJob(int *jobId) {
    for (auto it = jobList.rbegin(); it != jobList.rend(); it++){
        if (it->stopped){
          if (jobId != nullptr){
            *jobId = it->jobId;
          }
            return &(*it);
        }
    }
    return nullptr;
}

void JobsList::printJobsList() {
    int status;
    auto it = jobList.begin();
    while(it != jobList.end()){
        pid_t pid = waitpid(it->jobPid,&status,WNOHANG);
        if(pid > 0){
            jobList.erase(it);
        }else {
            cout << "[" << it->jobId << "] " << it->cmd->origianl_cmd << endl;
            it++;
        }
    }


}

JobsList::JobEntry *JobsList::getLastJob(int *lastJobId) {
    if (jobList.empty()) {
        return nullptr;
    }
    JobEntry &lastJob = jobList.back();
    if (lastJobId != nullptr) {
        *lastJobId = lastJob.jobId;
    }
    return &lastJob;
}





JobsCommand::JobsCommand(const char *cmd_line) : BuiltInCommand(cmd_line){}

void JobsCommand::execute() {
    JobsList &tempjobsList = SmallShell::getInstance().ShellJobList;
    tempjobsList.removeFinishedJobs();
    tempjobsList.printJobsList();

}




QuitCommand::QuitCommand(const char *cmd_line) : BuiltInCommand(cmd_line){}

void QuitCommand::execute() {


    if(argsNum == 1){
        exit(0);
    }
    if(argsNum > 1 && args[1] == std::string ("kill")){
        JobsList &tempJobList = SmallShell::getInstance().ShellJobList;
        cout<<"smash: sending SIGKILL signal to "<<tempJobList.jobList.size()<<" jobs:"<<endl;
        for (const auto& cmd : tempJobList.jobList) {
            pid_t jobPid = cmd.jobPid;
            cout<<jobPid<<": "<<cmd.cmd->origianl_cmd<<endl;
            if(kill(jobPid,9) == -1){
                perror("smash error: kill failed");
            }

        }
        exit(0);
    }
}

ExternalCommand::ExternalCommand(const char *cmd_line, bool isBackground) : Command(cmd_line){
    this->isBackGround = isBackground;
    this->needFork = true;
    // argsNum = _parseCommandLine(this->cmd_line,this->args);
}



void ExternalCommand::execute() {
    std::string cmdLine = std::string(cmd_line);
    bool complexCheck = false;
    Command* cmd = nullptr;
    if(std::string(args[0]) == "du"){
        cmd = new DiskUsageCommand(this->cmd_line);
        cmd->execute();
        delete cmd;
        return;

    }else if(string (args[0]) == "usbinfo"){
        cmd = new USBInfoCommand(this->cmd_line);
        cmd->execute();
        delete cmd;
        return;
    }
    if (cmdLine.find("?") != std::string::npos || cmdLine.find("*") != std::string::npos) {
        complexCheck = true;
    }
    if (complexCheck) {
        char *firstArg = (char*) "-c";
        char *secondArg = (char*) cmdLine.c_str();
        char* newArgs[] = {firstArg,secondArg,nullptr};
        if (execv("/bin/bash",newArgs) == -1){
            perror("smash error: execv failed");
        }
    }
    else {
        if (execvp(args[0],args) == -1){
            perror("smash error: execvp failed");
        }
    }
}
DiskUsageCommand::DiskUsageCommand(const char *cmd_line) : Command(cmd_line){}

struct linux_dirent64 {
    unsigned long long d_ino;
    long long          d_off;
    unsigned short     d_reclen;
    unsigned char      d_type;
    char               d_name[];
};

int DiskUsageCommand::calcDiskUsage(const char *path) {
    struct stat st;
    int diskSum = 0;
    if(lstat(path,&st) == -1){
        perror("smash error: lstat failed");
        return 0;
    }
    if(!S_ISDIR(st.st_mode)){
        return st.st_blocks;
    }
    int fd = open(path,O_RDONLY | O_DIRECTORY);
    if(fd == -1){
        perror("smash error: open failed");
        return 0;
    }
    char buffer[PATH_MAX];
    int read = syscall(217,fd,buffer,PATH_MAX);
    int bpos = 0;
    while(bpos < read){
        struct linux_dirent64 *sons = (struct linux_dirent64*)(buffer + bpos);
        if(string(sons->d_name) == "." || string(sons->d_name) == ".."){
            bpos += sons ->d_reclen;
            continue;
        }
        std::string son_path = std::string(path) + "/" + sons->d_name;
        bpos += sons ->d_reclen;
        diskSum += calcDiskUsage(son_path.c_str());
    }
    close(fd);
    return (diskSum + st.st_blocks);
}


void DiskUsageCommand::execute() {
    int diskUsage ;
    char res[PATH_MAX];
    if(argsNum == 1){
        if(getcwd(res,PATH_MAX) == nullptr){
            perror("smash error: getcwd failed");
        }
        diskUsage = calcDiskUsage(res);

    }else if(argsNum > 2){
        cerr << "smash error: du: too many arguments"<<endl;
        return;
    }else {
        diskUsage = calcDiskUsage(args[1]);
    }
    cout << "Total disk usage: "<< (diskUsage + 1)/2<<" KB"<<endl;
}
USBInfoCommand::USBInfoCommand(const char *cmd_line) : Command(cmd_line){}

struct UsbDevice{
    int devnum;
    std::string vendor;
    std::string product;
    std::string manufacture;
    std::string product_name;
    std::string power;
    UsbDevice(int dev, std::string vendor, std::string prod,
              std::string manu, std::string prod_name, std::string power)
            : devnum(dev), vendor(vendor), product(prod),
              manufacture(manu), product_name(prod_name), power(power) {}

};
string  usbInfo(std::string path){
    int fd = open(path.c_str(),O_RDONLY);
    if(fd == -1){
        return "N/A";
    }
    char buffer[256];
    int readn = read(fd, buffer,256);
    if(readn <= 0){
        return "N/A";
    }
    close(fd);
    buffer[readn] = '\0';
    string value(buffer);
    if(!value.empty() && (value.back() == '\n' || value.back() == '\r')){
        value.pop_back();
    }
    if(value.empty()){
        return value;
    }
    return value;
}
void USBInfoCommand::execute() {
    const char *usbPath = "/sys/bus/usb/devices/";
    std::vector<struct UsbDevice> usbVector;

    int fd = open(usbPath, O_RDONLY);
    if (fd == -1) {
        perror("smash error: open failed");
        close(fd);
        return;
    }

    char buffer[PATH_MAX];
    int read = syscall(217, fd, buffer, PATH_MAX);
    int bpos = 0;
    while (bpos < read) {
        struct linux_dirent64 *sons = (struct linux_dirent64 *) (buffer + bpos);
        if (string(sons->d_name) == "." || string(sons->d_name) == "..") {
            bpos += sons->d_reclen;
            continue;
        }
        std::string devNumPath = std::string(usbPath) + sons->d_name + "/devnum";
        int devFd = open(devNumPath.c_str(),O_RDONLY);
        if(devFd == -1){
            bpos += sons->d_reclen;
            continue;
        }
        std::string vendorPath = std::string(usbPath) + sons->d_name + "/idVendor";
        std::string productPath = std::string(usbPath) + sons->d_name + "/idProduct";
        std::string manufacturerPath = std::string(usbPath) + sons->d_name + "/manufacturer";
        std::string productNamePath = std::string(usbPath) + sons->d_name + "/product";
        std::string powerPath = std::string(usbPath) + sons->d_name + "/bMaxPower";

        int devnum = std::stoi(usbInfo(devNumPath)) ;
        string vendor = usbInfo(vendorPath);
        string product = usbInfo(productPath);
        string manufacturer = usbInfo(manufacturerPath);
        string productName = usbInfo(productNamePath);
        string power = usbInfo(powerPath);

        usbVector.emplace_back(devnum,vendor,product,manufacturer,productName,power);
        bpos += sons->d_reclen;
    }
    std::sort(usbVector.begin(), usbVector.end(), [](const UsbDevice& a, const UsbDevice& b) {
        return a.devnum < b.devnum;
    });
    close(fd);
    if (usbVector.empty()) {
        cerr << "smash error: usbinfo: no USB devices found" << endl;
        return;
    }
    for(const auto & d : usbVector){
        cout <<"Device "<<d.devnum<<": ID "<<d.vendor<<":"<<d.product<<" "<<d.manufacture<<" "<<d.product_name<<" MaxPower: "<<d.power<<endl;
    }



}




SmallShell::SmallShell() {
    // TODO: add your implementation
}

SmallShell::~SmallShell() {
    // TODO: add your implementation
}



/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command *SmallShell::CreateCommand(const char *cmd_line) {
    // For example:

    std::map<std::string,std::string> &tempAliasMap = SmallShell::getInstance().aliasMap ;
    if(cmd_line == nullptr){
        return nullptr;
    }
    original_cmd_line = std::string(cmd_line);
    string cmd_s = _trim(string(cmd_line));
    if (cmd_s.empty()) {
        return nullptr;
    }
    std::string firstcmdAlias,lastcmdAlias,resCmdAlias;

    string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));

    //if the first word exist in the alias map
    if(tempAliasMap.find(firstWord)!= tempAliasMap.end()){
        firstcmdAlias = aliasMap[firstWord];
        lastcmdAlias = cmd_s.substr(firstWord.length());
        resCmdAlias = firstcmdAlias + lastcmdAlias;
        cmd_s = resCmdAlias;
        firstWord = firstcmdAlias.substr(0,firstcmdAlias.find_first_of(" \n"));
    }
    char *temp = new char[cmd_s.length() + 1];
    strcpy(temp,cmd_s.c_str());
    bool BackGroundCmd = _isBackgroundComamnd(cmd_s.c_str());
    _removeBackgroundSign(temp);

    cmd_s = _trim(string(temp));
    firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));
    cmd_line = cmd_s.c_str();

    delete [] temp;


    if (firstWord.compare("alias") == 0) {
        return new AliasCommand(cmd_line);
    }

    else if (cmd_s.find("|") != std::string::npos) {
        return new PipeCommand(cmd_line);
    }

    else if (cmd_s.find("|") != std::string::npos) {
        return new PipeCommand(cmd_line);
    }

    else if (cmd_s.find(">") != std::string::npos) {
        return new RedirectionCommand(cmd_line);
    }

    else if(firstWord.compare("chprompt") == 0){
        return new changePrompt(cmd_line);
    }
    else if (firstWord.compare("pwd") == 0) {
      return new GetCurrDirCommand(cmd_line);
    }

    else if (firstWord.compare("showpid") == 0) {
        return new ShowPidCommand(cmd_line);
    }

    else if (firstWord.compare("cd") == 0) {
        return new ChangeDirCommand(cmd_line, &plastPwd);
    }

    else if (firstWord.compare("fg") == 0) {
        return new ForegroundCommand(cmd_line, &ShellJobList);
    }

    else if (firstWord.compare("kill") == 0) {
        return new KillCommand(cmd_line, &ShellJobList);
    }

    else if (firstWord.compare("unalias") == 0) {
        return new UnAliasCommand(cmd_line);
    }

    else if (firstWord.compare("sysinfo") == 0) {
        return new SysInfoCommand(cmd_line);
    }

    else if (firstWord.compare("whoami") == 0) {
        return new WhoAmICommand(cmd_line);
    }

    else if(firstWord.compare("jobs") == 0){
        return new JobsCommand(cmd_line);
    }

    else if(firstWord.compare("quit") == 0){
        return new QuitCommand(cmd_line);
    }

      //return new ShowPidCommand(cmd_line);


    else if (firstWord.compare("unsetenv") == 0){
        return new UnSetEnvCommand(cmd_line);
    }



    else {
      return new ExternalCommand(cmd_line,BackGroundCmd);
    }

}

void SmallShell::setCurrPid(pid_t pid) {
    this->currPid = pid;
}

pid_t SmallShell::getCurrPid() {
    return this->currPid;
}

void SmallShell::executeCommand(const char *cmd_line) {
    // TODO: Add your implementation here
    // for example:
    Command* cmd = CreateCommand(cmd_line);
    ShellJobList.removeFinishedJobs();
    if(cmd == nullptr){
         return;
     }
     else if (!cmd->needFork){
         setCurrPid(getpid());
         cmd->execute();
         setCurrPid(0);
         delete cmd;
         return;
     }else {
         pid_t my_pid = fork();
         if(my_pid == -1){
             perror("smash error: fork failed\"");
             delete cmd;
             return;
         }
         // still needs to handle the signal CTRL+C + adding the jobs
         if(my_pid == 0){
             setpgrp();
             cmd->execute();
             exit(0);
         }else{
             setCurrPid(my_pid);
             if(!cmd->isBackGround) {
                 if (waitpid(my_pid, NULL, 0) == -1){
                    perror("smash error: waitpid failed");
                 }
                 delete cmd;
             }
             else {
                 ShellJobList.addJob(cmd, my_pid);
             }
             setCurrPid(0);
         }
     }


    // Please note that you must fork smash process for some commands (e.g., external commands....)
}
