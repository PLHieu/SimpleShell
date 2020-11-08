#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <errno.h>

#define MAX_LENGTH_COMMAND  256
#define MAX_LENGTH_PATH     256
#define NUM_ARGUMENT        50
#define ARGUMENT_SIZE       200
#define MIN(a, b)   (a<b?a:b)
#define MAX(a, b)   (a>b?a:b)
static char historyCmd[MAX_LENGTH_COMMAND]={0};
static char workingDir[MAX_LENGTH_PATH]={0};
static int num_backgr_process=0;

//clear screen
void clearScreen();

//change the name of the terminal
void changeTerminalName(const char*name);

//print new prompt
void newPrompt();

//change working directory
//list of argument put into cd command ["cd", "./.../folder", ...]
void changeDir(char** dirList);

//stay until read a legal input from user (simple error-filter)
//inputString:
//  in: not null
//  out: non empty
void getInputString(char* const inputString);

//if it exists history caller (!!), add history to every position caller appears
//return: 1: if exists caller (!!) and history is not empty
//        0: if not exists caller
//        -1: if exists caller but history is empty
int addHistoryToInputString(char* const inputString);

//insert src string to dst string at position
void insert(char* dst, char*src, int position);

//split input string into two list of tokens/arguments
//return:
//0: normal command, tokens1: list arguments for command, tokens2: empty
//1: Output redirect, tokens1: command, tokens2: file output
//2: Input redirect, tokens1: command, tokens2: file input
//3: Command next to command, tokens1: list for command1, tokens2: list for command2
//4: cd command, like normal command but return different
int splitTokens(char*inpStr, char**tokens1, char**tokens2, int* is_parentwait);

//get list of arguments/tokens from a single argument string (input of user)
//input of command <= input of user (error-filtered)
void getArgList(char** const argList, const char* const argString);

//process command with input redirection
//return -1 if error
//end entire this child process
int processRedirectInputCmd(char** const cmdtokens, char*filename);

//process command with output redirection
//return -1 if error
//end entire this child process
int processRedirectOutputCmd(char** const cmdtokens, char*filename);

//Process two command separate by a |
//return -1 if error
//if success, end entire this child process
int processCmdNextToCmd(char**argumentscmd1, char**argumentscmd2);

//Every command will be put in here
//with a specific command, call suitable process function depend on it case
//return -1 if error
//if success, end entire this child process
int processCmd(int type, char**argumentscmd1, char**argumentscmd2);

//create an two dimensional char array-argument list
//return char**
char** newArgumentList();

//free an two dimensional char array-argument list
//return NULL
char** freeArgumentList(char** argList);

//detect command not found
//no such file or directory
//or is a directory
void printWhenExecFailed(char* args0);


int main()
{
    //change the name of terminal window to SimpleShell
    changeTerminalName("SimpleShell");
    getcwd(workingDir, MAX_LENGTH_PATH);
    clearScreen();
    
    int is_parentwait = 0;
    char *inputString = malloc(MAX_LENGTH_COMMAND);
    char** arg1List = newArgumentList();
    char** arg2List = newArgumentList();
    int mainpf[2];
    if (pipe(mainpf) < 0)
    {
        perror("pipe setup failed");
        return 0; 
    }
    int child_pid;
    while (1)
    {                                        
        getInputString(inputString);
        if (strcmp(inputString, "exit") == 0)
        {
            break;
        }
        
        int type = splitTokens(inputString, arg1List, arg2List, &is_parentwait);
        if(type==4) //change direction
        {
            changeDir(arg1List);
            continue;
        }

        child_pid = fork();                             //split into parent and child
        if (child_pid == -1) {
            perror("fork failed");
        } else if (child_pid == 0) {                      //child
            free(inputString);
            return processCmd(type, arg1List, arg2List);
        } else //parent
        {
            if (is_parentwait) {
                waitpid(child_pid, NULL, 0);
                newPrompt();
            } else {
                num_backgr_process += 1;
                printf("[%d] %d\n", num_backgr_process, child_pid);

                int c2_pid = fork();
                if (c2_pid < 0) { //fork failed
                    newPrompt();
                }
                else if (c2_pid == 0) {//continue reading new input, and processing, while parent wait for child_run
                    //run while loop 
                }
                else
                {//pid > 0, parent process 
                    int wret = waitpid(child_pid, NULL, 0);//wait for child_run
                    free(inputString);
                    arg1List = freeArgumentList(arg1List);
                    arg2List = freeArgumentList(arg2List);
                    newPrompt();
                    fflush(stdout);
                    
                    //wait for mainflow, this is ashame 'cause parent thread still run.
                    //But it's neccessary to hold the terminal window for the child
                    //otherwise the terminal will end though the child is running or not
                    waitpid(c2_pid, NULL, 0);
                    return 0;               //end parent, so the above child_2 will become parent
                }
            }
        }
    };

    free(inputString);
    arg1List = freeArgumentList(arg1List);
    arg2List = freeArgumentList(arg2List);
    return 0;
}

void clearScreen() {
    fflush(stdout);
    system("clear");
    newPrompt();
}
void changeTerminalName(const char* name) {
    printf("\033]0;%s\007", name);
}

void newPrompt()
{
    printf("ssh: %s>", workingDir);
}

//list of argument put into cd command ["cd", "./.../folder", ...]
void changeDir(char** dirList)
{
    if(dirList[2]!=NULL)
    {
        printf("bash: cd: too many arguments\n");
    }
    else if(dirList[1]!=NULL && chdir(dirList[1])==-1)
    {
        perror(dirList[1]);
    }
    getcwd(workingDir, MAX_LENGTH_PATH);
    newPrompt();
}

//get input string from stdin
void getInputString(char* const inputString)
{
    do {
        size_t lenInputStr = 0;
        char * inpStrTmp;
        //must read to another buffer because the getline may change the address of buffer
        getline(&inpStrTmp, &lenInputStr, stdin); 
        memmove(inputString, inpStrTmp, strlen(inpStrTmp));
        inputString[strlen(inpStrTmp)-1] = 0;
        free(inpStrTmp);

        //delete last space
        int idLenInpStr = strlen(inputString)-1;
        while (idLenInpStr>=0 && inputString[idLenInpStr] == ' ') {
            inputString[idLenInpStr--] = 0;
        }
        if(idLenInpStr<0){ continue; }//if empty string
        switch(addHistoryToInputString(inputString))
        {
            case 1: case 0: return;
            case -1: continue;
        }

    } while (1);
}

//if it exists history caller (!!), add history to every position caller appears
//return: 1: if exists caller (!!) and history is not empty
//        0: if not exists caller
//        -1: if exists caller but history is empty
int addHistoryToInputString(char* const inputString)
{
    //check if !! is entered
    int statusHistoryCall = 0;      //not call
    int i = 0;
    while (i < strlen(inputString)) {
        if (inputString[i] == '!' && inputString[i+1] == '!') {
            statusHistoryCall = 1;    //caller exists
            if (historyCmd[0] == 0) {
                statusHistoryCall = -1;//false, history is not exist
                break;
            }
            int lenmove = strlen(inputString)-i-1;
            if (strlen(inputString) == MAX_LENGTH_COMMAND) {
                lenmove -= 1;
            }
            memmove(inputString+i, inputString+i+2, strlen(inputString)-i-1);
            insert(inputString, historyCmd, i);
        }
        i++;
    }

    //show to screen
    if (statusHistoryCall == -1) {//fail
        printf("No commands in history\n");
        newPrompt();
    } else {
        if (statusHistoryCall == 1) {
            printf("%s\n", inputString);
        }

        //save to history
        if (inputString[0] != ' ') {
            memmove(historyCmd, inputString, strlen(inputString)+1);
        }
    }
    return statusHistoryCall;
}


//insert src string to dst string at position
void insert(char* dst, char*src, int position)
{
    int lendst = strlen(dst);
    int lensrc = strlen(src);
    if (position >= MAX_LENGTH_COMMAND) 
    {
        return;
    }
    int maxlen = MIN(MAX_LENGTH_COMMAND, lendst+lensrc);
    int lenMoveAfter = MAX(0, maxlen-(position+lensrc));
    int lenMove = MIN(lensrc, MAX_LENGTH_COMMAND);

    memmove(dst+position+lensrc, dst+position, lenMoveAfter);
    memmove(dst+position, src, lenMove);
}

//return:
//0: normal command
//1: Output redirect
//2: Input redirect
//3: Command next to command
//4: cd command
int splitTokens(char*inpStr, char**tokens1, char**tokens2, int* is_parentwait)
{
    int res = 0;
    *is_parentwait = 1;
    //delete last space in inpStr
    int idlen = strlen(inpStr)-1;
    while (idlen >= 0 && inpStr[idlen] == ' ') { 
        inpStr[idlen--] = 0;
    }

    //check if the last char is & then is_parentwait=true
    if (idlen >= 0 && inpStr[idlen] == '&') {
        *is_parentwait = 0;
        inpStr[idlen] = 0;
    }
    
    //split first command and the rest: (tokens2)
    int i = 0;
    while (i < strlen(inpStr) && !strchr("|<>", inpStr[i]))
    {
        i+=1;   //look for special position or end
    }
    if (i < strlen(inpStr)) { //separate character is in ["|<>"]
        if (inpStr[i] == '>') {
            res = 1;       
        } else if(inpStr[i] == '<') {
            res = 2;      
        } else                 // '|'
            res=3;          

        inpStr[i]=0;
        getArgList(tokens2, inpStr+i+1);
    }
    getArgList(tokens1, inpStr);
    
    if(strcmp(tokens1[0], "cd")==0) //cd command: type4
    {
        res=4;
    }

    return res;
}

//get list of argument/tokens from a single argument string
void getArgList(char** const argList, const char* const argString)
{
    char* token = (char*)malloc(50);
    int i = 0, ti = 0, id = 0;  
    int havingquote = 0;

    while (i < strlen(argString))
    {
        token[ti] = argString[i];
        if (token[ti] == ' ' && havingquote == 0) {
            if (ti == 0 || token[ti - 1] != '\\') { // if belong to  \[ ] -> no parse
                token[ti] = 0;//end token
                if (token[0] != 0) {//not null token
                    if (argList[id] == NULL) {
                        argList[id] = (char*)malloc(ARGUMENT_SIZE);
                    }
                    memmove(argList[id], token, strlen(token) + 1);
                    id++;
                }

                  ti = -1;//new token
            }
            else if (token[ti-1] == '\\') {
                token[ti - 1] = ' ';
                ti = ti-1;
            }
        }
        else if (token[ti] == '\"' || token[ti] == '\'') { 

            if (!havingquote) { 
                havingquote = 1;
                ti = -1;
            }
            else { 
                // if token[0] is ", ending -> parse
                token[ti] = 0;//end token
                if (token[0] != 0) {//not null token
                    if (argList[id] == NULL) {
                        argList[id] = (char*)malloc(ARGUMENT_SIZE);
                    }
                    memmove(argList[id], token, strlen(token) + 1);
                    id++;
                }

                ti = -1;//new token
                havingquote = 0;
            }
        }
        i++;
        ti++;
    }
    token[ti] = 0;
    if (token[0] != 0) {//not null token
        if (argList[id] == NULL) {
            argList[id] = (char*)malloc(ARGUMENT_SIZE);
        }
        memmove(argList[id], token, strlen(token) + 1);
        id++;
    }

    for(int i=id; i<NUM_ARGUMENT; i++)
    {
        free(argList[i]);
        argList[i]=NULL;
    }

    free(token);
}

//return error (<0) or not (>=0)
int processRedirectInputCmd(char** const cmdtokens, char*filename)
{
    int fd = open(filename, O_RDONLY);
    if (fd < 0)
    {
        printf("bash: %s: No such file or directory\n", filename);
        return -1;
    }
    dup2(fd, STDIN_FILENO);
    close(fd);
    int et = execvp(cmdtokens[0], cmdtokens);
    printWhenExecFailed(cmdtokens[0]);
    return et;
}
//return error (<0) or not (>=0)
int processRedirectOutputCmd(char** const cmdtokens, char*filename)
{
    int fd = open(filename, O_WRONLY | O_TRUNC | O_CREAT);
    if (fd < 0)
    {
        printf("bash: %s: No such file or directory\n", filename);
        return -1;
    }
    dup2(fd, STDOUT_FILENO);
    close(fd);

    int et = execvp(cmdtokens[0], cmdtokens);
    printWhenExecFailed(cmdtokens[0]);
    return et;
}
int processCmdNextToCmd(char**argumentscmd1, char**argumentscmd2)
{
    int pfsub[2];
    if (pipe(pfsub) < 0)
    {
        perror("pipe setup failed"); 
        return 0;
    }

    int ftsub = fork();                   //child split into this child and grandchild
    if (ftsub < 0)
    {
        perror("fork failed");
        return 0;
    } else if (ftsub == 0) {                 //grandchild
        dup2(pfsub[1], STDOUT_FILENO);
        close(pfsub[0]);
        close(pfsub[1]);
        execvp(argumentscmd1[0], argumentscmd1);//success or not, end grandchild, back to child
        printWhenExecFailed(argumentscmd1[0]);
        return 0;
    } else {                              //back to child
        waitpid(ftsub, NULL, 0);
        dup2(pfsub[0], STDIN_FILENO);
        close(pfsub[0]);
        close(pfsub[1]);
        execvp(argumentscmd2[0], argumentscmd2);
        printWhenExecFailed(argumentscmd2[0]);
        return 0;
    }
}
int processCmd(int type, char**argumentscmd1, char**argumentscmd2)
{    
    if (type == 1) {
        processRedirectOutputCmd(argumentscmd1, argumentscmd2[0]);
    } else if(type == 2) {
        processRedirectInputCmd(argumentscmd1, argumentscmd2[0]);
    } else if(type == 3) {                      //cmd next to cmd
        processCmdNextToCmd(argumentscmd1, argumentscmd2);
    } else {    //type==0, normal case
        execvp(argumentscmd1[0], argumentscmd1);//make sure in case exec* error, it still return 
        printWhenExecFailed(argumentscmd1[0]);
    }

    return 0;
}

char** newArgumentList()
{
    char** newList = malloc(NUM_ARGUMENT*sizeof(char*));
    for (int i = 0; i < NUM_ARGUMENT; i++)
    {
        newList[i] = malloc(ARGUMENT_SIZE);
    }
    return newList;
}
char** freeArgumentList(char** argList)
{
    for(int i = 0; i < NUM_ARGUMENT; i++)
    {
        free(argList[i]);
    }
    free(argList);

    return NULL;
}

//detect command not found
//no such file or directory
//or is a directory
void printWhenExecFailed(char* args0)
{
    if(!strchr("~./", args0[0]))
    {
        printf("%s: command not found\n", args0);
        return;
    }

    struct stat statbuf;
    if(stat(args0, &statbuf)==-1)
    {
        printf("bash: %s: No such file or directory\n", args0);
    }else if((statbuf.st_mode & S_IFMT) == S_IFDIR)
    {
        printf("bash: %s: Is a directory\n", args0);
    }

    return;
}
