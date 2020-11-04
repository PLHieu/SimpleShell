#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <memory.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include<sys/stat.h>
#include <errno.h>
#include <poll.h>

#define MAX_LENGTH_COMMAND  256
#define MAX_PIPE_INPUT_SIZE 512
#define NUM_ARGUMENT        100
#define ARGUMENT_SIZE       50
#define MIN(a, b)   (a<b?a:b)
#define MAX(a, b)   (a>b?a:b)
static char historyCmd[MAX_LENGTH_COMMAND]={0};
static int num_backgr_process=0;
void clearScreen();
void changeTerminalName(const char*name);
void newPrompt();
void getInputString(char* const inputString);//__attribute__((nonnull(1)));
void insert(char* dst, char*src, int position);
void concatToArgList(char**const argList, const char*const str);
//return:
//0: normal command
//1: Output redirect
//2: Input redirect
//3: Command next to command
int splitTokens(char*inpStr, char**tokens1, char**tokens2, int* is_parentwait);

//get list of argument/tokens from a single argument string
void getArgList(char** const argList, const char* const argString);
int processRedirectInputCmd(char** const cmdtokens, char*filename);
int processRedirectOutputCmd(char** const cmdtokens, char*filename);
int processCmdNextToCmd(char**argumentscmd1, char**argumentscmd2);
int processCmd(int type, char**argumentscmd1, char**argumentscmd2);
char** newArgumentList();
//return NULL
char** freeArgumentList(char** argList);
int main()
{
    //change the name of terminal window to SimpleShell
    changeTerminalName("SimpleShell");
    //clear screen
    clearScreen();

    int is_parentwait=0;
    char *inputString=malloc(MAX_LENGTH_COMMAND);
    char** arg1List=newArgumentList();
    char** arg2List=newArgumentList();
    int mainpf[2];
    if(pipe(mainpf)<0){ perror("pipe setup failed"); return 0; }
    int child_pid;
    while(1){                                       //do while not input "exit"       
        getInputString(inputString);
        if(strcmp(inputString, "exit")==0){break;}  //if wanna exit then exit
        
        int type = splitTokens(inputString, arg1List, arg2List, &is_parentwait);

        child_pid=fork();                             //split into parent and child
        if (child_pid == -1) {
            perror("fork failed");
        } else if (child_pid == 0) {                      //child
            free(inputString);
            return processCmd(type, arg1List, arg2List);
        }else//parent
        {
            if(is_parentwait){
                waitpid(child_pid, NULL, 0);
                newPrompt();
            }else{//parent does not wait, so create new process to wait for the child done to print out newPrompt()
                num_backgr_process+=1;
                //new process run along with the parent to check if child has end.
                //if child still run then it don't print out newPrompt();
                int c2_pid=fork();
                if(c2_pid<0){ //fork failed
                    newPrompt();
                }
                else if(c2_pid==0){//continue reading new input, and processing, while parent wait for child_run
                    //run while loop 
                }else{//pid>0, parent process 
                    int wret=waitpid(c2_pid, NULL, 0);//wait for child_run
                    newPrompt();
                    return 0;               //end parent, so the above child_2 will become parent
                    }
                is_parentwait=0;
            }
        }
    };

    free(inputString);
    arg1List=freeArgumentList(arg1List);
    arg2List=freeArgumentList(arg2List);
    return 0;
}

void clearScreen(){
    fflush(stdout);
    system("clear");
    newPrompt();
}
void changeTerminalName(const char* name){
    printf("\033]0;%s\007", name);
}
//new prompt line
void newPrompt(){
    printf("ssh>");
}

//get input string from stdin
void getInputString(char* const inputString){

    do{
        size_t lenInputStr=0;
        char * inpStrTmp;
        getline(&inpStrTmp, &lenInputStr, stdin);
        memmove(inputString, inpStrTmp, strlen(inpStrTmp));
        inputString[strlen(inpStrTmp)-1]=0;
        free(inpStrTmp);
        //check if !! is entered
        int statusHistoryCall = -1;      //not call
        int i=0;
        while(i < strlen(inputString)){
            if(inputString[i] == '!' && inputString[i+1] == '!'){
                statusHistoryCall=1;    //caller exists
                if(historyCmd[0]==0){
                    statusHistoryCall=0;//false, history is not exist
                    break;
                }
                int lenmove=strlen(inputString)-i-1;
                if(strlen(inputString)==MAX_LENGTH_COMMAND){lenmove-=1;}
                memmove(inputString+i, inputString+i+2, strlen(inputString)-i-1);
                insert(inputString, historyCmd, i);
            }
            i++;
        }


        //show to screen
        if(statusHistoryCall==0){//fail
            printf("No commands in history\n");
            newPrompt();
            continue;
        }else{
            if(statusHistoryCall==1){
                printf("%s\n", inputString);
            }

            //save to history
            if(inputString[0]!=' '){
                memmove(historyCmd, inputString, strlen(inputString)+1);
            }
            return;
        }

    }while(1);
}

//insert src string to dst string at position
void insert(char* dst, char*src, int position){
    int lendst=strlen(dst);
    int lensrc=strlen(src);
    if(position>=MAX_LENGTH_COMMAND){
        return;
    }
    int maxlen=MIN(MAX_LENGTH_COMMAND, lendst+lensrc);
    int lenMoveAfter=MAX(0, maxlen-(position+lensrc));
    int lenMove=MIN(lensrc, MAX_LENGTH_COMMAND);

    memmove(dst+position+lensrc, dst+position, lenMoveAfter);
    memmove(dst+position, src, lenMove);
}

//concat tokens in a string to an argument list
void concatToArgList(char**const argList, const char*const str){
    char** addList=newArgumentList();

    getArgList(addList, str);
    int id=0, in=0;
    while(argList[id]!=NULL){id+=1;}
    while(addList[in]!=NULL){
        if(argList[id]!=NULL){ free(argList[id]); }
        argList[id++]=addList[in];
        addList[in++]=NULL;
    }
    
    if(argList[id]!=NULL){ free(argList[id]); }
    argList[id]=NULL;

    addList = freeArgumentList(addList);
}

//return:
//0: normal command
//1: Output redirect
//2: Input redirect
//3: Command next to command
int splitTokens(char*inpStr, char**tokens1, char**tokens2, int* is_parentwait)
{
    int res=0;
    *is_parentwait = 1;
    //delete last space in inpStr
    int idlen=strlen(inpStr)-1;
    while(idlen>=0 && inpStr[idlen]==' '){ inpStr[idlen--] = 0;}

    //check if the last char is & then is_parentwait=true
    if (idlen >= 0 && inpStr[idlen] == '&') {
        *is_parentwait = 0;
        inpStr[idlen]=0;
    }
    
    //split first command and the rest: (tokens2 in simple case)
    int i=0;
    while(i<strlen(inpStr) && !strchr("|<>", inpStr[i])){i+=1;} //look for special position or end
    if(i<strlen(inpStr)){
        if (inpStr[i]=='>'){res=1;}         //'>'  
        else if(inpStr[i]=='<'){res=2;}     //'<'
        else res=3;                         //'|'

        inpStr[i]=0;
        getArgList(tokens2, inpStr+i+1);
    }
    getArgList(tokens1, inpStr);

    return res;
}

//get list of argument/tokens from a single argument string
void getArgList(char** const argList, const char* const argString)
{
    char* token=malloc(50);
    int i=0, ti=0, id=0;
    while(i<strlen(argString))
    {
        token[ti]=argString[i];
        if(token[ti]==' '){
            token[ti]=0;//end token
            if(token[0]!=0){//not null token
                if(argList[id]==NULL){
                    argList[id]=malloc(ARGUMENT_SIZE);
                }
                memmove(argList[id], token, strlen(token)+1);
                id++;
            }
            
            ti=-1;//new token
        }
        i++;
        ti++;
    }
    token[ti]=0;
    if(token[0]!=0){//not null token
        if(argList[id]==NULL){
            argList[id]=malloc(ARGUMENT_SIZE);
        }
        memmove(argList[id], token, strlen(token)+1);
        id++;
    }
    
    free(argList[id]);
    argList[id]=NULL;
    free(token);
}

//return error (<0) or not (>=0)
int processRedirectInputCmd(char** const cmdtokens, char*filename){
    int fd=open(filename, O_RDONLY);
    if(fd<0){
        printf("bash: %s: No such file or directory", filename);
        return -1;
    }
    dup2(fd, STDIN_FILENO);
    close(fd);
    int et=execvp(cmdtokens[0], cmdtokens);
    return et;
}
//return error (<0) or not (>=0)
int processRedirectOutputCmd(char** const cmdtokens, char*filename){
    int fd=open(filename, O_WRONLY | O_TRUNC | O_CREAT);
    if(fd<0){
        printf("bash: %s: No such file or directory", filename);
        return -1;
    }
    dup2(fd, STDOUT_FILENO);
    close(fd);

    int et=execvp(cmdtokens[0], cmdtokens);
    return et;
}
int processCmdNextToCmd(char**argumentscmd1, char**argumentscmd2){
    int pfsub[2];
    if(pipe(pfsub) < 0){ perror("pipe setup failed"); return 0; }//return inside child=>end child, don't worry

    int ftsub=fork();                   //child split into this child and grandchild
    if(ftsub<0){
        perror("fork failed");
        return 0;
    }else if(ftsub==0){                 //grandchild
        dup2(pfsub[1], STDOUT_FILENO);
        execvp(argumentscmd1[0], argumentscmd1);//success or not, end grandchild, back to child
        return 0;
    }else{                              //back to child
        waitpid(ftsub, NULL, 0);
        char* strTmp=malloc(MAX_PIPE_INPUT_SIZE);
        read(pfsub[0], strTmp, MAX_PIPE_INPUT_SIZE);
        concatToArgList(argumentscmd2, strTmp);
        free(strTmp);
        execvp(argumentscmd2[0], argumentscmd2);
        return 0;
    }
}
int processCmd(int type, char**argumentscmd1, char**argumentscmd2){

    if(type==1){
        return processRedirectOutputCmd(argumentscmd1, argumentscmd2[0]);
    }else if(type==2){
        return processRedirectInputCmd(argumentscmd1, argumentscmd2[0]);
    }else if(type==3){                      //cmd next to cmd
        return processCmdNextToCmd(argumentscmd1, argumentscmd2);
    }else{
        return execvp(argumentscmd1[0], argumentscmd1);//make sure in case exec* error, it still return
    }

}
char** newArgumentList()
{
    char** newList=malloc(NUM_ARGUMENT*sizeof(char*));
    for(int i=0; i<NUM_ARGUMENT; i++){
        newList[i]=malloc(ARGUMENT_SIZE);
    }
    return newList;
}
char** freeArgumentList(char** argList){
    for(int i=0; i<NUM_ARGUMENT; i++)
    {
        free(argList[i]);
    }
    free(argList);

    return NULL;
}#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <memory.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include<sys/stat.h>
#include <errno.h>
#include <poll.h>

#define MAX_LENGTH_COMMAND  256
#define MAX_PIPE_INPUT_SIZE 512
#define NUM_ARGUMENT        100
#define ARGUMENT_SIZE       50
#define MIN(a, b)   (a<b?a:b)
#define MAX(a, b)   (a>b?a:b)
static char historyCmd[MAX_LENGTH_COMMAND]={0};
static int num_backgr_process=0;
void clearScreen();
void changeTerminalName(const char*name);
void newPrompt();
void getInputString(char* const inputString);//__attribute__((nonnull(1)));
void insert(char* dst, char*src, int position);
void concatToArgList(char**const argList, const char*const str);
//return:
//0: normal command
//1: Output redirect
//2: Input redirect
//3: Command next to command
int splitTokens(char*inpStr, char**tokens1, char**tokens2, int* is_parentwait);

//get list of argument/tokens from a single argument string
void getArgList(char** const argList, const char* const argString);
int processRedirectInputCmd(char** const cmdtokens, char*filename);
int processRedirectOutputCmd(char** const cmdtokens, char*filename);
int processCmdNextToCmd(char**argumentscmd1, char**argumentscmd2);
int processCmd(int type, char**argumentscmd1, char**argumentscmd2);
int** newArgumentList();
//return NULL
int** freeArgumentList(char** argList);
int main()
{
    //change the name of terminal window to SimpleShell
    changeTerminalName("SimpleShell");
    //clear screen
    clearScreen();

    int is_parentwait=0;
    char *inputString=malloc(MAX_LENGTH_COMMAND);
    char** arg1List=newArgumentList();
    char** arg2List=newArgumentList();
    int mainpf[2];
    if(pipe(mainpf)<0){ perror("pipe setup failed"); return 0; }
    int child_pid;
    while(1){                                       //do while not input "exit"       
        getInputString(inputString);
        if(strcmp(inputString, "exit")==0){break;}  //if wanna exit then exit
        
        int type = splitTokens(inputString, arg1List, arg2List, &is_parentwait);

        child_pid=fork();                             //split into parent and child
        if (child_pid == -1) {
            perror("fork failed");
        } else if (child_pid == 0) {                      //child
            free(inputString);
            return processCmd(type, arg1List, arg2List);
        }else//parent
        {
            if(is_parentwait){
                wait(child_pid);
                newPrompt();
            }else{//parent does not wait, so create new process to wait for the child done to print out newPrompt()
                num_backgr_process+=1;
                //new process run along with the parent to check if child has end.
                //if child still run then it don't print out newPrompt();
                int waitstatus;
                int c2_pid=fork();
                if(c2_pid<0){ //fork failed
                    newPrompt();
                }
                else if(c2_pid==0){//continue reading new input, and processing, while parent wait for child_run
                    //run while loop 
                }else{//pid>0, parent process 
                    int wret=waitpid(c2_pid, &waitstatus, 0);//wait for child_run
                    newPrompt();
                    return 0;               //end parent, so the above child_2 will become parent
                    }
                is_parentwait=0;
            }
        }
    };

    free(inputString);
    arg1List=freeArgumentList(arg1List);
    arg2List=freeArgumentList(arg2List);
    return 0;
}

void clearScreen(){
    fflush(stdout);
    system("clear");
    newPrompt();
}
void changeTerminalName(const char* name){
    printf("\033]0;%s\007", name);
}
//new prompt line
void newPrompt(){
    printf("ssh>");
}

//get input string from stdin
void getInputString(char* const inputString){

    do{
        size_t lenInputStr=0;
        char * inpStrTmp;
        getline(&inpStrTmp, &lenInputStr, stdin);
        memmove(inputString, inpStrTmp, strlen(inpStrTmp));
        inputString[strlen(inpStrTmp)-1]=0;
        free(inpStrTmp);
        //check if !! is entered
        int statusHistoryCall = -1;      //not call
        int i=0;
        while(i < strlen(inputString)){
            if(inputString[i] == '!' && inputString[i+1] == '!'){
                statusHistoryCall=1;    //caller exists
                if(historyCmd[0]==0){
                    statusHistoryCall=0;//false, history is not exist
                    break;
                }
                int lenmove=strlen(inputString)-i-1;
                if(strlen(inputString)==MAX_LENGTH_COMMAND){lenmove-=1;}
                memmove(inputString+i, inputString+i+2, strlen(inputString)-i-1);
                insert(inputString, historyCmd, i);
            }
            i++;
        }


        //show to screen
        if(statusHistoryCall==0){//fail
            printf("No commands in history\n");
            newPrompt();
            continue;
        }else{
            if(statusHistoryCall==1){
                printf("%s\n", inputString);
            }

            //save to history
            if(inputString[0]!=' '){
                memmove(historyCmd, inputString, strlen(inputString)+1);
            }
            return;
        }

    }while(1);
}

//insert src string to dst string at position
void insert(char* dst, char*src, int position){
    int lendst=strlen(dst);
    int lensrc=strlen(src);
    if(position>=MAX_LENGTH_COMMAND){
        return;
    }
    int maxlen=MIN(MAX_LENGTH_COMMAND, lendst+lensrc);
    int lenMoveAfter=MAX(0, maxlen-(position+lensrc));
    int lenMove=MIN(lensrc, MAX_LENGTH_COMMAND);

    memmove(dst+position+lensrc, dst+position, lenMoveAfter);
    memmove(dst+position, src, lenMove);
}

//concat tokens in a string to an argument list
void concatToArgList(char**const argList, const char*const str){
    char** addList=newArgumentList();

    getArgList(addList, str);
    int id=0, in=0;
    while(argList[id]!=NULL){id+=1;}
    while(addList[in]!=NULL){
        if(argList[id]!=NULL){ free(argList[id]); }
        argList[id++]=addList[in];
        addList[in++]=NULL;
    }
    
    if(argList[id]!=NULL){ free(argList[id]); }
    argList[id]=NULL;

    addList = freeArgumentList(addList);
}

//return:
//0: normal command
//1: Output redirect
//2: Input redirect
//3: Command next to command
int splitTokens(char*inpStr, char**tokens1, char**tokens2, int* is_parentwait)
{
    int res=0;
    *is_parentwait = 1;
    //delete last space in inpStr
    int idlen=strlen(inpStr)-1;
    while(idlen>=0 && inpStr[idlen]==' '){ inpStr[idlen--] = 0;}

    //check if the last char is & then is_parentwait=true
    if (idlen >= 0 && inpStr[idlen] == '&') {
        *is_parentwait = 0;
        inpStr[idlen]=0;
    }
    
    //split first command and the rest: (tokens2 in simple case)
    int i=0;
    while(i<strlen(inpStr) && !strchr("|<>", inpStr[i])){i+=1;} //look for special position or end
    if(i<strlen(inpStr)){
        if (inpStr[i]=='>'){res=1;}         //'>'  
        else if(inpStr[i]=='<'){res=2;}     //'<'
        else res=3;                         //'|'

        inpStr[i]=0;
        getArgList(tokens2, inpStr+i+1);
    }
    getArgList(tokens1, inpStr);

    return res;
}

//get list of argument/tokens from a single argument string
void getArgList(char** const argList, const char* const argString)
{
    char* token=malloc(50);
    int i=0, ti=0, id=0;
    while(i<strlen(argString))
    {
        token[ti]=argString[i];
        if(token[ti]==' '){
            token[ti]=0;//end token
            if(token[0]!=0){//not null token
                if(argList[id]==NULL){
                    argList[id]=malloc(ARGUMENT_SIZE);
                }
                memmove(argList[id], token, strlen(token)+1);
                id++;
            }
            
            ti=-1;//new token
        }
        i++;
        ti++;
    }
    token[ti]=0;
    if(token[0]!=0){//not null token
        if(argList[id]==NULL){
            argList[id]=malloc(ARGUMENT_SIZE);
        }
        memmove(argList[id], token, strlen(token)+1);
        id++;
    }
    
    free(argList[id]);
    argList[id]=NULL;
    free(token);
}

//return error (<0) or not (>=0)
int processRedirectInputCmd(char** const cmdtokens, char*filename){
    int fd=open(filename, O_RDONLY);
    if(fd<0){
        printf("bash: %s: No such file or directory", filename);
        return -1;
    }
    dup2(fd, STDIN_FILENO);
    close(fd);
    int et=execvp(cmdtokens[0], cmdtokens);
    return et;
}
//return error (<0) or not (>=0)
int processRedirectOutputCmd(char** const cmdtokens, char*filename){
    int fd=open(filename, O_WRONLY | O_TRUNC | O_CREAT);
    if(fd<0){
        printf("bash: %s: No such file or directory", filename);
        return -1;
    }
    dup2(fd, STDOUT_FILENO);
    close(fd);

    int et=execvp(cmdtokens[0], cmdtokens);
    return et;
}
int processCmdNextToCmd(char**argumentscmd1, char**argumentscmd2){
    int pfsub[2];
    if(pipe(pfsub) < 0){ perror("pipe setup failed"); return 0; }//return inside child=>end child, don't worry

    int ftsub=fork();                   //child split into this child and grandchild
    if(ftsub<0){
        perror("fork failed");
        return 0;
    }else if(ftsub==0){                 //grandchild
        dup2(pfsub[1], STDOUT_FILENO);
        execvp(argumentscmd1[0], argumentscmd1);//success or not, end grandchild, back to child
        return 0;
    }else{                              //back to child
        wait(ftsub);
        char* strTmp=malloc(MAX_PIPE_INPUT_SIZE);
        read(pfsub[0], strTmp, MAX_PIPE_INPUT_SIZE);
        concatToArgList(argumentscmd2, strTmp);
        free(strTmp);
        execvp(argumentscmd2[0], argumentscmd2);
        return 0;
    }
}
int processCmd(int type, char**argumentscmd1, char**argumentscmd2){

    if(type==1){
        return processRedirectOutputCmd(argumentscmd1, argumentscmd2[0]);
    }else if(type==2){
        return processRedirectInputCmd(argumentscmd1, argumentscmd2[0]);
    }else if(type==3){                      //cmd next to cmd
        return processCmdNextToCmd(argumentscmd1, argumentscmd2);
    }else{
        return execvp(argumentscmd1[0], argumentscmd1);//make sure in case exec* error, it still return
    }

}
int** newArgumentList()
{
    char** newList=malloc(NUM_ARGUMENT*sizeof(char*));
    for(int i=0; i<NUM_ARGUMENT; i++){
        newList[i]=malloc(ARGUMENT_SIZE);
    }
    return newList;
}
int** freeArgumentList(char** argList){
    for(int i=0; i<NUM_ARGUMENT; i++)
    {
        free(argList[i]);
    }
    free(argList);

    return NULL;
}



