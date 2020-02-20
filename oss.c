#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "oss.h"

#define MAX 18
#define maxTimeBetweenNewProcsNS 100000000
#define maxTimeBetweenNewProcsSecs 1
#define QUANTUM 1000000
#define BILLION 1000000000

  //prototypes
void signalHandler();
void sigAlarmHandler(int sig_num);

  // ctrl-c handler variables
int CHILDPIDS[MAX];
int processCount = 0;
int exitFlag = 0;

  // shm pointers and ids
pcb* processControl;
static system_clock* shmClockPtr;
int shmid;  // shared memory id
int shmbid; // shared memory block idea
int msgid;

        // MAIN
int main(int argc, char *argv[]){

  // file variables
  FILE *ofp = NULL;
  char *ofname = NULL;
  ofname = "logFile.dat";

  // variables for launching child processes
  unsigned int timeToNextProcsSecs;
  unsigned int timeToNextProcsNS;
  unsigned int launchNextProcsSecs;
  unsigned int launchNextProcsNS;

  // processes id variables
  pid_t pid;
  pid_t waitPID;
  CHILDPIDS[0] = -2; // DEFAULT FLAG -2 used for default CHILDPIDS[] flag
  int status;
  int totalp = 0;
  int i = 0;

  int index = 0; // available index of current pcb
  int queue = 0; // queue level, working with only one at the moment
  int timeSlice = 0;

  // vars for passing integers with exec*
  char execIndex[18];

  // msg queue struct
  struct msgbuf message;

  // opening output log file to clear it
  ofp = fopen(ofname, "w");
  if(ofp == NULL){
    perror(argv[0]);
    perror("Error oss: Logfile failed to open.");
    return EXIT_FAILURE;
  }
  fclose(ofp);


  // setting up logfile for appending
  ofp = fopen(ofname, "a");
  if(ofp == NULL){
    perror(argv[0]);
    perror("Error oss: Logfile failed to open.");
    return EXIT_FAILURE;
  }


  // master time termination
  alarm(3);


  // catch for ctrl-c signal
  signal(SIGINT, signalHandler);


  // catch alarm signal
  signal(SIGALRM, sigAlarmHandler);

        // KEYS
  // key for shmid
  key_t key = ftok("oss.c", 50);
  if(key == -1){
    perror("Error oss: key shared memory key generation");
    return EXIT_FAILURE;
  }

  // key for shmbid
  key_t bkey = ftok("oss.c", 75);
  if(bkey== -1){
    perror("Error oss: bkey shared memory key generation");
    return EXIT_FAILURE;
  }

  // key for msg queue
  key_t msgkey = ftok("oss.c", 25);
  if(msgkey == -1){
    perror("Error oss: message queue key generation");
    return EXIT_FAILURE;
  }
        // CREATE SHARED MEMORY
  // might not be large enough
  // shmid system_clock
  shmid = shmget(key, sizeof(system_clock), 0666 | IPC_CREAT);
  if(shmid == -1){
    perror("Error oss: shmget shmid");
    exit(EXIT_FAILURE);
  }
  //mght not be large enough
  // shmbid pcb
  shmbid = shmget(bkey, sizeof(pcb) * 18, 0666 | IPC_CREAT);
  if(shmbid == -1){
    perror("Error oss: shmget shmbid");
    exit(EXIT_FAILURE);
  }

        // ATTACH SHARED MEMORY SEGMENTS
  // attach the segment to our data space system clock
  shmClockPtr = (system_clock *) shmat(shmid, NULL, 0);
  if(shmClockPtr == (void *)-1){
    perror("Error oss: shmat shmClockPtr");
    exit(EXIT_FAILURE);
  } else {
    shmClockPtr->seconds = 0;
    shmClockPtr->nanoSeconds = 0;
  }

  // attach the segment to data space process control block
  processControl = (pcb *) shmat(shmbid, NULL, 0);
  if(processControl == (void *)-1){
    perror("Error oss: shmat processControl");
    exit(EXIT_FAILURE);
  }
  for(i = 0; i < MAX; i++){
      processControl[i].ready = -1; // DEFAULT FLAG -1 for default ready state
      processControl[i].queue = -1; // DEFAULT FLAG -1 for defautl queue state
  }

  // create nessage queue
  if ((msgid = msgget(msgkey, 0666 | IPC_CREAT)) < 0){
    perror("Error oss: msgget");
    return EXIT_FAILURE;
  }
  printf("STARTING SCHEDULING PROCESS. . .");

  // random time for next process
  srand((unsigned)time(NULL));
  timeToNextProcsSecs = rand() % maxTimeBetweenNewProcsSecs;
  timeToNextProcsNS = rand() % maxTimeBetweenNewProcsNS;
  launchNextProcsSecs = launchNextProcsSecs + timeToNextProcsSecs;
  launchNextProcsNS = launchNextProcsNS + timeToNextProcsNS;
  if( launchNextProcsNS >= BILLION){
    launchNextProcsNS = launchNextProcsNS - BILLION;
    launchNextProcsSecs = launchNextProcsSecs + 1;
  }


        // PRIMARY LOOP
  while (exitFlag == 0){
    // printf("inside while...\n");
    // termination flag for breaking outof loop
    if(exitFlag == 1){
      break;
    }
    // increment the clock using shared memory timer
    shmClockPtr->nanoSeconds += 100000;
    if(shmClockPtr->nanoSeconds >= 1000000000){
      shmClockPtr->seconds++;
      shmClockPtr->nanoSeconds -= 1000000000;
    }

    printf("current %d:%d\n", shmClockPtr->seconds, shmClockPtr->nanoSeconds);
    printf("launching at %d:%d\n", launchNextProcsSecs, launchNextProcsNS);

    printf("oss index %d\n", index);
    // launch new processes if necessary
    if(processCount < MAX && shmClockPtr->seconds >= launchNextProcsSecs
         && shmClockPtr->nanoSeconds >= launchNextProcsNS){
      printf("pcb index...\n");
      // attach pcb index
      for (i = 0; i < MAX; i++){
        if(processControl[i].ready == -1){
          index = i;
          break;
        }
      }



      // fork child process
      pid = fork();
      printf("child forked...\n");
      if(pid == -1){
        perror("ForkError: Error: failed to fork child_pid = -1");
        break;
      } else if(pid == 0){
        // converting index to string to pass to child
        snprintf(execIndex, sizeof(execIndex), "%d", index);
        processControl[index].index = index; //passing not working using index in pcb
        printf("passing index %d\n", processControl[index].index);
        execlp("./user", execIndex, NULL);
        perror("Execl Error: failed to exec child from the fork of oss");
        exit(EXIT_FAILURE); //child prog failed to exec
      } else {
        // PARENT PROCESSES
        CHILDPIDS[processCount] = pid;
        processCount++;
        totalp++;
        // pcb variable assignment
        processControl[index].pid = (long)pid;
        processControl[index].ready = 1; // flaged for ready from defualt -1
        processControl[index].queue = 1; // default queue value of 0
        processControl[index].done = 0;
        processControl[index].process_time = rand()%3000000 + 1;
        processControl[index].burst_time = QUANTUM;
        processControl[index].rem_bt = 0;
        // log generation message
        fprintf(ofp, "OSS: Generating process with PID %d and putting it in queue %d  at time %d:%d\n"
                , index, processControl[index].queue , shmClockPtr->seconds, shmClockPtr->nanoSeconds);

      }

      // random time for next process
      srand((unsigned)time(NULL));
      timeToNextProcsSecs = rand() % maxTimeBetweenNewProcsSecs;
      timeToNextProcsNS = rand() % maxTimeBetweenNewProcsNS;
      launchNextProcsSecs = launchNextProcsSecs + timeToNextProcsSecs;
      launchNextProcsNS = launchNextProcsNS + timeToNextProcsNS;
       if( launchNextProcsNS >= BILLION){
        launchNextProcsNS = launchNextProcsNS - BILLION;
        launchNextProcsSecs = launchNextProcsSecs + 1;
      }

    } // end if (processCount < MAX)
    // SENDING CHILD PROCESSES MSG
    if(processControl[index].queue > 0){
      printf("launching child!\n");
      // log dispatch message
      fprintf(ofp, "OSS: Dispatching pocess with PID %d from queue %d at time %d:%d\n"
      , index, processControl[index].queue , shmClockPtr->seconds, shmClockPtr->nanoSeconds);

      //timeSlice for round robin queue

      // send message to let child process execute
      message.mtype = processControl[index].pid;
      //using mtext for index
      sprintf(message.mtext, "%d", index);
      int msgSize = sizeof(message);
      if(msgsnd(msgid, &message, msgSize, 0) == -1){
        perror("Error oss: msgsnd failed");
        break;
      }

      // receive message
      if(msgrcv(msgid, &message, sizeof(message), 111, 0) == -1){
        perror("Error oss: msgrcv failed");
        break;
      }

      // if process finished
      if(processControl[index].done == 1){
        fprintf(ofp, "OSS: before %d shmClockPtr.NanoSeconds\n", shmClockPtr->nanoSeconds);
        // added time to clock
        shmClockPtr->nanoSeconds+=processControl[index].process_time;
        if(shmClockPtr->nanoSeconds >= BILLION){
          shmClockPtr->seconds++;
          shmClockPtr->nanoSeconds -= BILLION;
        }

        fprintf(ofp, "OSS: after %d shmClockPtr.NanoSeconds\n", shmClockPtr->nanoSeconds);
        fprintf(ofp, "OSS: Receiving that process with PID %d ran for %d nanoseconds\n", index, processControl[index].process_time);
        while((waitPID =wait(&status)) >0);
        processCount--;
        processControl[index].ready = -1;
        processControl[index].done = 0;
        processControl[index].queue = 0;
      }

    }
    printf("goagain... & totalp = %d\n", totalp);
  } // end while(exitFlag == 0)

        // CLEAN UP
  // wait on child procs
  while((waitPID = wait(&status)) > 0);

  //remove msg queue
  if (msgctl(msgid, IPC_RMID, NULL) == -1) {
    perror("Error oss: msgctl");
  }

  // detach shared memory
  shmdt(shmClockPtr);
  shmdt(processControl);

  // delete shared memory
  if(shmctl(shmid, IPC_RMID, NULL) == -1){
    perror("Error oss: mshmctl shmid");
  }
  if(shmctl(shmbid, IPC_RMID, NULL) == -1){
    perror("Error oss: shmctl shmbid");
  }

  // clean up
  fclose(ofp);
  return EXIT_SUCCESS;
}


        // CTRL-C EXIT HANDLER
void signalHandler() {
  exitFlag = 1;
  printf("ctrl-c signal handler executing.\n");
  int i;
  for(i = 0; i < processCount; i++){
    kill(CHILDPIDS[i], SIGKILL);
  }

  // destroy message queue
  if (msgctl(msgid, IPC_RMID, NULL) == -1) {
    perror("Error oss: sighandler msgctl");
  }

  // delete shared memory
  shmctl(shmid, IPC_RMID,NULL);
  shmctl(shmbid, IPC_RMID, NULL);
  signal(SIGINT, signalHandler);
}


        // ALARM EXIT HANDLER
void sigAlarmHandler(int sig_num){
  printf("signal alarm exit.\n");
  exitFlag = 1;
  int i;
  printf("-t time exceeded\n");
  for(i = 0; i < processCount; i++){
    kill(CHILDPIDS[i], SIGKILL);
  }

  // destroy message queue
  if (msgctl(msgid, IPC_RMID, NULL) == -1) {
    perror("Error oss: sigAlarm msgctl");
  }

  // delete shared memory
  shmctl(shmid, IPC_RMID, NULL);
  shmctl(shmbid, IPC_RMID, NULL);

  signal(SIGALRM, sigAlarmHandler);
}