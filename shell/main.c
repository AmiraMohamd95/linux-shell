// https://gist.github.com/parse/966049
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/wait.h>
#define MAX_LINE 80
#define MAX_HISTORY_SIZE 10
#define MAX_COMMAND_LEN 500
#define MAX_ARG_LEN 200
#define Max_PATHS 20
#define MAX_PATH_LEN 500
/*
the parent must reap the children to avoid leaving zombies in the system
but we also want the parent to be free to do other work while the children are running, so
we decide to reap the children with a SIGCHILD handler, instead of explicitly waiting for the children
to terminate
(Recall that the kernel sends a SIGCHILD signal to the parent whenever one of its children terminates or stops)
*/
typedef struct History History;
    struct History {
        char*  data[MAX_HISTORY_SIZE];
        int     size;
        int     top;
    };
    void History_init (History *h){
        h->size = 0;
        h->top = -1;
        // allocate data in the history
        int i;
        for (i=0;i<MAX_HISTORY_SIZE;i++){
            h->data[i] = (char* )malloc(MAX_COMMAND_LEN);
        }
    }
    void free_History (History *h){
        int i;
        for (i=0;i<MAX_HISTORY_SIZE;i++){
            free(h->data[i]);
        }
    }
    void History_Push(History *h,char* line)
    {
        if (h->size < MAX_HISTORY_SIZE)
            h->size++;
        h->top = (h->top + 1)%MAX_HISTORY_SIZE;
        h->data[h->top] = line;
    }
    char* History_get(History *h,int index){
        if (h->top+1 == h->size){
            return h->data[index-1];
        }
        else {
            return h->data[(h->top+1+index)%MAX_HISTORY_SIZE - 1];
        }
    }
    void print_history (History *h, FILE* outStream){
        int st;
        if (h->top+1 == h->size){
            st=0;
        }else{
            st = (h->top+1)%MAX_HISTORY_SIZE;
        }
        int c=0;
        while (c<h->size){
            c++;
            if (outStream == stdout){
                fprintf(outStream,"%.2d  ",c);
            }
            fprintf(outStream,"%s",h->data[st]);
            st = (st+1)%MAX_HISTORY_SIZE;
        }
    }
    // History
    int split (char line[81],char *args[MAX_LINE/2 + 1], char delimiters[3]){
        char *token = malloc(MAX_ARG_LEN*sizeof(char));
        token = strtok(line, delimiters);
        int i=0;
        args[i] = token;
        while (token != NULL){
            i++;
            token = strtok(NULL, delimiters);
            args[i] = token;
        }
        free(token);
        return i+1;
    }
    char* strClone (char source[MAX_COMMAND_LEN]){
        char* dest = (char*) malloc(MAX_COMMAND_LEN);
        memcpy(dest,source,MAX_COMMAND_LEN);
        return dest;
    }
    void handle_sigchld (int sig){
        pid_t pid;
        int status;
        while (1) {
            pid = waitpid(-1, &status, WNOHANG);
            if (pid <= 0) /* No more zombie children to reap. */
                break;
            //printf("Reaped child %d\n", pid);
        }
    }

    int constructPaths (char **paths){
        char *env_paths = malloc(Max_PATHS*MAX_PATH_LEN*sizeof(char));
        env_paths = (char* )getenv ("PATH");
        int len = split(env_paths,paths,":");
        paths[len-1] = "";
        paths[len] = NULL;
        return len;
    }
//   void cleanMemory (char** paths,char** args, char** data, char* line, char* path, FILE* batch){
//        int i;
//        for (i=0;i<Max_PATHS;i++){
//            free(paths[i]);
//        }
//        free(paths);
//        for (i=0;i<MAX_HISTORY_SIZE;i++){
//            free(data[i]);
//        }
//        free(data);
//        for (i=0;i<(MAX_LINE/2 + 1);i++){
//            free(args[i]);
//        }
//        free(args);
//        free(line);free(path);
//        if (batch != NULL)
//            fclose(batch);
 //   }

int main(int argc, char* const argv[])
{
    History history;
    History_init(&history);
    FILE *inpStream;
    FILE *commandsHistory;
    FILE *batch=NULL;
    int numOfArgs;
    int backgroundModeFlag;
    int i;
    int pathsLen;
    char *envPaths[Max_PATHS];
    char *args[MAX_LINE/2 + 1]; /* command line arguments */
    char *pathOfCommand;
    char *inputLine;
    if (argc == 2){
        batch = fopen(argv[1],"r");
        if (batch!=NULL){
            inpStream = batch;
        }else {
            /** error**/
            perror("Error: ");
            exit(0);
        }
    }else if (argc == 1){
        inpStream = stdin;
    }else {
        fprintf(stderr,"Error: Extra Arguments\n ");
        exit(0);
    }
    //
    for (i=0;i<(MAX_LINE/2 + 1);i++){
        args[i] = (char* )malloc(MAX_ARG_LEN * sizeof (char));
    }
    for (i=0;i<Max_PATHS;i++){
        envPaths[i] = (char* )malloc(MAX_PATH_LEN * sizeof (char));
    }
    inputLine = malloc(MAX_COMMAND_LEN);
    pathsLen = constructPaths(envPaths);
    pathOfCommand = malloc((MAX_PATH_LEN) * sizeof(char));
    commandsHistory = fopen("history","r");
    signal(SIGCHLD, handle_sigchld);
    //
    while (fgets(inputLine,MAX_COMMAND_LEN,commandsHistory) != NULL){
        History_Push(&history,strClone(inputLine));
    }
    while (1)
    {
        backgroundModeFlag=0;
        if (batch == NULL)
            printf("shell>");
        fflush(stdout);
       if (fgets(inputLine,MAX_COMMAND_LEN,inpStream) == NULL){
            print_history(&history,fopen("history","w"));
            free_History(&history);
            free(inputLine);
            free(pathOfCommand);
            exit(0);
        }
        if (batch!=NULL){
            printf("%s",inputLine);
        }
        numOfArgs = split(strClone(inputLine),args," \t\n");
        if (numOfArgs == 1) {
            continue;
        }
        if (strcmp(args[0],"history")==0&&numOfArgs==2){
            print_history(&history,stdout);
            continue;
        }else if (strcmp(args[0],"exit")==0&&numOfArgs==2){
            print_history(&history,fopen("history","w"));
            free_History(&history);
            free(inputLine);
            free(pathOfCommand);
            exit(0);
        }else if (strcmp(args[0],"!!")==0&&numOfArgs==2){
            if(history.size == 0){
                fprintf(stderr,"History is empty\n");
                continue;
            }else{
                inputLine = strClone(History_get(&history,history.size));
                numOfArgs = split(strClone(inputLine),args," \t\n");
            }
        }else if (args[0][0]=='!' && numOfArgs==2){
            int index;
            if (args[0][1]=='-'&&isdigit(args[0][2])){
                index = 10 + (args[0][2]-'0')*-1 + 1;
            }
            else if (isdigit(args[0][1])){
                if (isdigit(args[0][2])){
                    index = (args[0][1]-'0')*10 + args[0][2] - '0';
                }else{
                    index = args[0][1]-'0';
                }
            }
            if (index > history.size){
                /**error **/
                fprintf(stderr,"Error: No such command in history.\n");
                continue;
            }
            else {
                inputLine = strClone(History_get(&history,index));
                numOfArgs = split(strClone(inputLine),args," \t\n");
            }
        }
        if (history.top > -1){
            // check that
            if (strcmp(inputLine,history.data[history.top])!=0){
                History_Push(&history,strClone(inputLine));
            }
        }else{
            History_Push(&history,strClone(inputLine));
        }
        if (strlen(inputLine) > MAX_LINE){
            fprintf(stderr,"command exceeds maximum length\n");
            continue;
        }

        int length = strlen(args[numOfArgs-2]);
        if (strcmp(args[numOfArgs-2],"&")==0){
            backgroundModeFlag=1;
            args[numOfArgs-2] = NULL;
        }else if (args[numOfArgs-2][length-1]=='&'){
            backgroundModeFlag=1;
            char *temp = (char* )malloc(MAX_ARG_LEN);
            memcpy(temp,args[numOfArgs-2],length-1);
            strcpy(args[numOfArgs-2],temp);
            free(temp);
        }
        // new process
        pid_t pid = fork();
        // child
        int found=0;
        if (pid == 0){
            int i;
            for (i=0;i<pathsLen;i++){
                if(i==pathsLen-1){
                    pathOfCommand = args[0];
                }else {
                    pathOfCommand = strcat(strcat(strClone(envPaths[i]),"/"),args[0]);
                }
                if (fopen(pathOfCommand,"r")!=NULL){
                    found = 1;
                    execv(pathOfCommand,args);
                    perror("Error: ");
                    //return 0;
                    exit(0);
                }
            }
            if (found == 0){
                fprintf(stderr,"Error: Command Not Exist\n");
            }
        }
        // parent
        else if (pid>0){
            if (!backgroundModeFlag){
                // wait
                int status;
     //           wait(NULL);
    //            waitpid(pid, &status, 0);
              waitpid (-1,&status,0);

                //wait(&status);
//               if (status == -1) {
//                }
            }
        }
    }
    return 0;
}
