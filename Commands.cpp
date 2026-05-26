#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include "Commands.h"
#include <regex>

using namespace std;

const std::string WHITESPACE = " \n\r\t\f\v";

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

    // bool BackGroundCmd = _isBackgroundComamnd(cmd_line);


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
    _removeBackgroundSign(temp);

    cmd_s = _trim(string(temp));
    firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));
    cmd_line = cmd_s.c_str();

    delete [] temp;

    if(firstWord.compare("chprompt") == 0){
        return new changePrompt(cmd_line);
    }
    if (firstWord.compare("pwd") == 0) {
      return new GetCurrDirCommand(cmd_line);
    }

    else if (firstWord.compare("alias") == 0) {
            return new AliasCommand(cmd_line);

      //return new ShowPidCommand(cmd_line);
    }
      /*
    else if ...
    .....
    else {
      return new ExternalCommand(cmd_line);
    }
    */
    return nullptr;
}

void SmallShell::executeCommand(const char *cmd_line) {
    // TODO: Add your implementation here
    // for example:
     Command* cmd = CreateCommand(cmd_line);
     if(cmd != nullptr){
         cmd->execute();
     }


    // Please note that you must fork smash process for some commands (e.g., external commands....)
}
