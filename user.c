#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "oss.h"

#define MAX 18
#define TERMINATION 2 // % chance to terminate
#define QUANTUM 1000000


void signalHandler();

  int msgid;
  int shmid, shmbid;

int main(int argc, char *argv){
  printf("child/user process launched. . .\n");
  pcb* processControl;
  system_clock* shmClockPtr;

  int doneFlag = 0;
  int index = 0;
  int timeSlice = 0;
  int termTest = 0;
  int timeUsed = 0;

  // msg queue struct
  struct msgbuf message;

  // signal for quitting
  signal(SIGINT, signalHandler);

        // KEYS
  // key for shmid
  key_t key = ftok("oss.c", 50);
  if(key == -1){
    perror("Error: shared memory key generation");
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
    perror("Error: message queue key generation");
    return EXIT_FAILURE;
  }

        //CREATE SHARED MEMORY
  // shmid
  shmid = shmget(key, sizeof(system_clock), 0666 | IPC_CREAT);
  if(shmid == -1){
    perror("Error user.c: shmget shmid");
    exit(EXIT_FAILURE);
  }
  // shmbid
  shmbid = shmget(bkey, MAX * sizeof(pcb), 0666 | IPC_CREAT);
  if(shmbid == -1){
    perror("Error user.c: shmget shmbid");
    exit(EXIT_FAILURE);
  }


  // attach the segment to our data space
  shmClockPtr = (system_clock *) shmat(shmid, NULL, 0);
  if(shmClockPtr == (void *)-1){
    perror("Error user.c: shmat shmid");
    exit(EXIT_FAILURE);
  }

  processControl = (pcb *) shmat(shmbid, NULL, 0);
  if(processControl == (void *)-1){
    perror("Error user.c: shmat shmbid");
    exit(EXIT_FAILURE);
  }


  // create message queue
  if ((msgid = msgget(msgkey, 0666 | IPC_CREAT)) < 0){
    perror("Error user.c: msgget");
    return EXIT_FAILURE;
  }

  long PID = getpid();
  message.mtype = PID;

  // child process process time code
  while(doneFlag == 0){

    int done = 1;

    //reciving message from master
    if(msgrcv(msgid, &message, sizeof(message), PID, 0) == -1){
      perror("Error user.c: msgrcv failed");
      break;
    }
    message.mtype = 111;
        // PROCESS WORK BELOW
    // assigned pcb index to index var
    index = atoi(message.mtext);
    //printf("child process work begin\n");

    // handling time slice reduction
    timeSlice = processControl[index].process_time;

    // termination chance
    srand((unsigned)time(NULL));
    termTest = rand()%100 + 1;
    if(termTest <= TERMINATION){
      //terminated
    } else {

    }

    // mark process as done
    processControl[index].done = 1;
    printf("user.c index %d n pc[index]: %d\n", index, processControl[index].index);

    //sprintf(message.mtext, "%d", index);
    //send message
    if (msgsnd(msgid, &message, sizeof(message), 0) == -1){
      perror("Error user.c: msgsnd failed");
      break;
    }

    // if procses is done flag to exit
    if(1<2){
      doneFlag == 1;
      break;
    }

  }

  //CLEAN UP
  shmdt(shmClockPtr);
  shmdt(processControl);
  printf("child done\n");
  return EXIT_SUCCESS;
}


  // crl-c signal handler
void signalHandler() {
  //exitFlag = 1;
  printf("ctrl-c signal handler executing.\n");
  int i;
  //for(i = 0; i < processCount; i++){
  //  kill(CHILDPIDS[i], SIGKILL);
  //}

  // destroy message queue
  if (msgctl(msgid, IPC_RMID, NULL) == -1) {
    perror("Error: msgctl");
  }

  // delete shared memory
  shmctl(shmid, IPC_RMID,NULL);

  signal(SIGINT, signalHandler);

}