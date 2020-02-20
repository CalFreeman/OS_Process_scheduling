#ifndef OSS_H_
#define OSS_H_

#define MAX 18

  // system clock
typedef struct system_clock{
  unsigned int seconds;
  unsigned int nanoSeconds;
} system_clock;

  // processes control block
typedef struct pcb{
  long pid;
  long index;
  int process_time;
  int burst_time;
  int rem_bt;
  int ready; // oss.c initilize -1 ready state
  int queue; // queue position
  int done; //if process finished
  int state;
  int r;
  int s;
} pcb;

  //message queue
struct msgbuf{
  long mtype;
  char mtext[256];
};

#endif