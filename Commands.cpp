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

std::string SmallShell::Prompt = "smash>";

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

BuiltInCommand::BuiltInCommand(const char *cmd_line): Command(cmd_line) {
    //_removeBackgroundSign(this->cmd_line);
   argsNum = _parseCommandLine(this->cmd_line,this->args);
}

changePrompt::changePrompt(const char *cmd_line): BuiltInCommand(cmd_line) {
}

void changePrompt::execute() {
    if(argsNum >1){
        SmallShell::Prompt = std::string(args[1]) + ">";
        return;
    }
    SmallShell::Prompt = "smash>";
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
            pid_t jobPid = jobToBring->getPid();
            std::cout << jobToBring->getCmdLine() << " " << int(jobPid) << endl;
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
            pid_t jobPid = jobToBring->getPid();
            std::cout << jobToBring->getCmdLine() << " " << int(jobPid) << endl;
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
            pid_t jobPid = jobs->getJobById(jobId)->getPid();
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
    struct sysinfo sysInfo;
    if (sysinfo(&sysInfo) == -1){
        perror("smash error: sysinfo failed");
        return;
    }
    time_t upTime = sysInfo.uptime;
    time_t currTime = time(NULL);
    time_t bootTime = currTime - upTime;
    struct tm *sysBootTime = localtime(&bootTime);
    char bufferTime[30];
    strftime(bufferTime, 30, "%Y-%m-%d %H:%M:%S", sysBootTime);
    cout << "System: " << uts.sysname << endl;
    cout << "Hostname: " << uts.nodename << endl;
    cout << "Kernel: " << uts.release << endl;
    cout << "Architecture: " << uts.machine << endl;
    cout << "Boot Time: " << bufferTime << endl;

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
    if(argsNum == 0){
        cerr<<"smash error: unsetenv: not enough arguments"<<endl;
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
            jobList.erase(it);
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
            cout<<jobPid<<": "<<cmd.cmd->cmd_line<<endl;
            tempJobList.removeJobById(cmd.jobId);
            if(kill(jobPid,9) == -1){
                perror(" smash error: kill failed");
            }

        }
        exit(0);
    }
}

ExternalCommand::ExternalCommand(const char *cmd_line, bool isBackground) : Command(cmd_line){
    this->isBackGround = isBackground;
    this->needFork = true;
    argsNum = _parseCommandLine(this->cmd_line,this->args);
}

void ExternalCommand::execute() {
    if(execvp(args[0],args) == -1){
        perror("smash error: execvp failed\"");
        exit(1);
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
    original_cmd_line = cmd_line;

    string cmd_s = _trim(string(cmd_line));

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

    if(firstWord.compare("chprompt") == 0){
        return new changePrompt(cmd_line);
    }
    if (firstWord.compare("pwd") == 0) {
        cout<<"i am ";
      return new GetCurrDirCommand(cmd_line);
    }

    else if (firstWord.compare("alias") == 0) {
        return new AliasCommand(cmd_line);
    }

    else if (firstWord.compare("showpid") == 0) {
        return new ShowPidCommand(cmd_line);
    }

    else if (firstWord.compare("cd") == 0) {
        return new ChangeDirCommand(cmd_line, plastPwd);
    }

    else if (firstWord.compare("fg") == 0) {
        return new ForegroundCommand(cmd_line, jobsList);
    }

    else if (firstWord.compare("kill") == 0) {
        return new KillCommand(cmd_line, jobsList);
    }

    else if (firstWord.compare("unalias") == 0) {
        return new UnaliasCommand(cmd_line);
    }

    else if (firstWord.compare("sysinfo") == 0) {
        return new SysInfoCommand(cmd_line);
    }

    //  return to this line

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

void SmallShell::executeCommand(const char *cmd_line) {
    // TODO: Add your implementation here
    // for example:
    Command* cmd = CreateCommand(cmd_line);

    if(cmd == nullptr){
         return;
     }
     else if (!cmd->needFork){
         cmd->execute();
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
         }else{
             ShellJobList.removeFinishedJobs();
             if(!cmd->isBackGround) {
                 wait(NULL);
                 delete cmd;
             }else {
                 ShellJobList.addJob(cmd, my_pid);
             }
         }
     }


    // Please note that you must fork smash process for some commands (e.g., external commands....)
}
