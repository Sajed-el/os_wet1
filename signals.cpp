#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"

using namespace std;

void ctrlCHandler(int sig_num) {
    if (sig_num == SIGINT) {
        cout << "smash: got ctrl-C" << endl;
        pid_t pid = SmallShell::getInstance().getCurrPid();
        if (pid == 0) {
            return;
        }
        if (kill(pid, SIGINT) == -1){
            perror("smash error: kill failed");
        }
        else{
        cout << "smash: process " << pid << " was killed" << endl;
        }
    }
}
