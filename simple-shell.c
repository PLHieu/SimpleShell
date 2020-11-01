#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <memory.h>
#include <sys/wait.h>

#define MAX_LENGTH_COMMAND  80
#define MIN(a, b)   (a<b?a:b)
#define MAX(a, b)   (a>b?a:b)
static char historyCmd[MAX_LENGTH_COMMAND]={0};
void clearScreen();
void changeTerminalName(const char*name);
void newPrompt();
void getInputString(char* const inputString);//__attribute__((nonnull(1)));
void insert(char* dst, char*src, int position);
void childProcess();

int main()
{
    //change the name of terminal window to SimpleShell
    changeTerminalName("SimpleShell");
    //clear screen
    clearScreen();
    char *inputString=malloc(MAX_LENGTH_COMMAND);
    int pf[2];
    if(pipe(pf)<0){
        printf("fail to setup pipe!\n");
        return 0;
    }
    char* argst[]={"something", NULL};
    do{
        getInputString(inputString);
        int tmp=fork();
        if(tmp==-1){
            printf("fail to fork\n");
        }else if(tmp==0){//child
            int e=execvp("ls", argst);
            if(e<0){
                close(pf[1]);//write
                close(pf[0]);//read
                free(inputString);
                return 0;
            }
        }else//parent
        {
            wait(NULL);
            newPrompt();
        }
    }
    while(strcmp(inputString, "exit")!=0);

    //char* args[]={"lskdfhiosd", NULL};
    //execv("/bin/mkdir", args);
    //char* args2[]={"ls", "/home/hw/Desktop/abc",  NULL};
    //char* args3[]={"kjghyujkjgiufdytsr", "/home/hw/Desktop/abc/tt", "/home/hw/Desktop", NULL};
    //execv("/bin/ls -l", args2);
    //execv("/bin/less", args);
    //getchar();
    close(pf[1]);//write
    close(pf[0]);//read
    free(inputString);
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

void newPrompt(){
    printf("ssh>");
}
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
                memmove(historyCmd, inputString, strlen(inputString));
            }
            return;
        }

    }while(1);
}

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

void childProcess()
{

}