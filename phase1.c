/* ------------------------------------------------------------------------
   phase1.c

   University of Arizona
   Computer Science 452
   Fall 2015

   ------------------------------------------------------------------------ */

#include "phase1.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/time.h> 

#include "kernel.h"

/* ------------------------- Prototypes ----------------------------------- */
void illegalInstructionHandler(int dev, void *arg);

int sentinel (char *);
void dispatcher(void);
void launch();
static void checkDeadlock();

void clockHandler(int dev, void *arg);


/* -------------------------- Globals ------------------------------------- */

// Patrick's debugging global variable...
int debugflag = 1;

// the process table
procStruct ProcTable[MAXPROC];

// Process lists
static procPtr ReadyList;

// current process ID
procPtr Current;

procPtr tree;

// the next pid to be assigned
unsigned int nextPid = SENTINELPID;


int isZapped(){
	return -1;
}

// ------------------------------------------------------------------------------
// functions added
//

void enableInterrupts(){
	if ((USLOSS_PsrGet() & 1) == 0){
       printf("user mode\n");
       USLOSS_Halt(1);
    }
	
	unsigned int newPsr = (USLOSS_PsrGet() | 0x2);
	unsigned int temp = USLOSS_PsrSet(newPsr);
}

/*
 * Disables the interrupts.
 */
void disableInterrupts()
{
	if ((USLOSS_PsrGet() & 1) == 0){
       printf("user mode\n");
       USLOSS_Halt(1);
    }
    // turn the interrupts OFF iff we are in kernel mode
    // if not in kernel mode, print an error message and
    // halt USLOSS
	
	unsigned int newPsr = (USLOSS_PsrGet() & ~0x2);
	unsigned int temp = USLOSS_PsrSet(newPsr);

} /* disableInterrupts */

void enqueue(procPtr cur){
	if ((USLOSS_PsrGet() & 1) == 0){
       printf("user mode\n");
       USLOSS_Halt(1);
    }
	
	if (cur->status != READY){
		USLOSS_Console("cannot enqueue proccess that is not ready\n");
		return;
	}
	
	int i = 0;
	procPtr q = ReadyList;
	while (q != NULL){
		USLOSS_Console("q--q %d = %s prior = %d\n", i, q->name, q->priority);
		q = q->nextProcPtr;
		i++;
	}
	
	procPtr queue = ReadyList;
	if (queue == NULL){
		queue = cur;
		USLOSS_Console("queue null: priority = %d and readyList.priority = %d\n", cur->priority, queue->priority);
	} else {
		if (queue->nextProcPtr == NULL){
			if (cur->priority >= queue->priority){
				procPtr temp = queue->nextProcPtr;
				queue->nextProcPtr = cur;
				cur->nextProcPtr = temp;
			} else {
				// check here possible when order is messed up
				// USLOSS_Console("this is cause\n");
				// procPtr temp = cur->nextProcPtr;
				cur->nextProcPtr = queue;
				// queue->nextProcPtr = temp;
				queue = cur;
			}
		} else {
			USLOSS_Console("problem this time\n");
			while (queue->nextProcPtr->nextProcPtr != NULL){
				// move down the list
				if (cur->priority >= queue->nextProcPtr->priority){
					USLOSS_Console("priority = %d and readyList.priority = %d\n", cur->priority, queue->priority);
					break;
				}
				queue = queue->nextProcPtr;
			}
			
			if (cur->priority >= queue->priority){
				// if nextProcPtr NULL then end of list else insert slot
				procPtr temp = queue->nextProcPtr;
				queue->nextProcPtr = cur;
				cur->nextProcPtr = temp;
			} else {
				// check here possible when order is messed up
				// USLOSS_Console("this is cause\n");
				// procPtr temp = cur->nextProcPtr;
				cur->nextProcPtr = queue;
				// queue->nextProcPtr = temp;
				queue = cur;
			}
		}
	}
	
	ReadyList = queue;
	USLOSS_Console("ReadyList.name = %s and queue.name = %s\n", ReadyList->name, queue->name);
	
	
	i = 0;
	q = ReadyList;
	while (q != NULL){
		USLOSS_Console("--q %d = %s prior = %d\n", i, q->name, q->priority);
		q = q->nextProcPtr;
		i++;
	}
	
	 // USLOSS_Console("should be sentinel: %s\n", ReadyList->name);
}

void cpyProc(procPtr from, procPtr to){
	to->nextProcPtr = from->nextProcPtr;
	to->childProcPtr = from->childProcPtr;
	to->nextSiblingPtr = from->nextSiblingPtr;
    strcpy(to->name, from->name);
    strcpy(to->startArg, from->startArg);
	to->state = from->state;
	to->pid = from->pid;
	to->priority = from->priority;
	to->startFunc = from->startFunc;
	to->stack = from->stack;
	to->stackSize = from->stackSize;
	to->status = from->status;  
	to->parentPtr = from->parentPtr;
	to->time = from->time;
}

procStruct dequeue(){
	if ((USLOSS_PsrGet() & 1) == 0){
       printf("user mode\n");
       USLOSS_Halt(1);
    }
	
	if (strcmp(ReadyList->name, "sentinel") == 0){
		USLOSS_Halt(1);
	}
	// if (ReadyList == NULL){
		// return procStruct;
	// }
	
	int i = 0;
	procPtr q = ReadyList;
	while (q != NULL){
		USLOSS_Console("dq---dq %d = %s prior = %d\n", i, q->name, q->priority);
		q = q->nextProcPtr;
		i++;
	}
	
	// do a actual data transfer
	procStruct queue;
	cpyProc(ReadyList, &queue);
	ReadyList = ReadyList->nextProcPtr;
	
	i = 0;
	q = ReadyList;
	while (q != NULL){
		USLOSS_Console("---dq %d = %s prior = %d\n", i, q->name, q->priority);
		q = q->nextProcPtr;
		i++;
	}
	
	return queue;
}

void clean(procPtr releasedProc){
	releasedProc->nextProcPtr = NULL;
	releasedProc->childProcPtr = NULL;
	releasedProc->nextSiblingPtr = NULL;
    strcpy(releasedProc->name, "");
    strcpy(releasedProc->startArg, "");
	// releasedProc->state = 0;
	releasedProc->pid = -1;
	releasedProc->priority = 0;
	releasedProc->startFunc = NULL;
	releasedProc->stack = NULL;
	releasedProc->stackSize = 0;
	releasedProc->status = OPEN;  
	releasedProc->parentPtr = NULL;
	releasedProc->time = 0;
}

void unblockMe(procPtr parent){
	parent->status = READY;
	enqueue(parent);
}

//
// function added
// ------------------------------------------------------------------------------

/* -------------------------- Functions ----------------------------------- */
/* ------------------------------------------------------------------------
   Name - startup
   Purpose - Initializes process lists and clock interrupt vector.
             Start up sentinel process and the test process.
   Parameters - argc and argv passed in by USLOSS
   Returns - nothing
   Side Effects - lots, starts the whole thing
   ----------------------------------------------------------------------- */
void startup(int argc, char *argv[])
{
	if ((USLOSS_PsrGet() & 1) == 0){
       printf("user mode\n");
       USLOSS_Halt(1);
    }
	
	disableInterrupts();
	
    int result; /* value returned by call to fork1() */

    /* initialize the process table */
    if (DEBUG && debugflag)
        USLOSS_Console("startup(): initializing process table, ProcTable[]\n");
	int i = 0;
	for (i = 0; i < MAXPROC; i++){
		clean(&(ProcTable[i]));
		// if (ProcTable[i].status == OPEN){
			// USLOSS_Console("// OPEN = %d i = %d\n", OPEN, i);
		// }
	}

    // Initialize the Ready list, etc.
    if (DEBUG && debugflag)
        USLOSS_Console("startup(): initializing the Ready list\n");
    ReadyList = NULL;

    // Initialize the illegalInstruction interrupt handler
    USLOSS_IntVec[USLOSS_ILLEGAL_INT] = illegalInstructionHandler;

    // Initialize the clock interrupt handler
	USLOSS_IntVec[USLOSS_CLOCK_INT] = clockHandler;
	USLOSS_IntVec[USLOSS_CLOCK_DEV] = clockHandler;

    // startup a sentinel process
    if (DEBUG && debugflag)
        USLOSS_Console("startup(): calling fork1() for sentinel\n");
    result = fork1("sentinel", sentinel, NULL, USLOSS_MIN_STACK,
                    SENTINELPRIORITY);
    if (result < 0) {
        if (DEBUG && debugflag) {
            USLOSS_Console("startup(): fork1 of sentinel returned error, ");
            USLOSS_Console("halting...\n");
        }
        USLOSS_Halt(1);
    }
    // start the test process
    if (DEBUG && debugflag)
        USLOSS_Console("startup(): calling fork1() for start1\n");
    result = fork1("start1", start1, NULL, 2 * USLOSS_MIN_STACK, 1);
    if (result < 0) {
        USLOSS_Console("startup(): fork1 for start1 returned an error, ");
        USLOSS_Console("halting...\n");
        USLOSS_Halt(1);
    }
	
	enableInterrupts();
	
	// dispatcher();
	
    USLOSS_Console("startup(): Should not see this message! ");
    USLOSS_Console("Returned from fork1 call that created start1\n");

    return;
} /* startup */

/* ------------------------------------------------------------------------
   Name - finish
   Purpose - Required by USLOSS
   Parameters - none
   Returns - nothing
   Side Effects - none
   ----------------------------------------------------------------------- */
void finish(int argc, char *argv[])
{
	if ((USLOSS_PsrGet() & 1) == 0){
       printf("user mode\n");
       USLOSS_Halt(1);
    }
	
    if (DEBUG && debugflag)
        USLOSS_Console("in finish...\n");
} /* finish */

/* ------------------------------------------------------------------------
   Name - fork1
   Purpose - Gets a new process from the process table and initializes
             information of the process.  Updates information in the
             parent process to reflect this child process creation.
   Parameters - the process procedure address, the size of the stack and
                the priority to be assigned to the child process.
   Returns - the process id of the created child or -1 if no child could
             be created or if priority is not between max and min priority.
   Side Effects - ReadyList is changed, ProcTable is changed, Current
                  process information changed
   ------------------------------------------------------------------------ */
int fork1(char *name, int (*startFunc)(char *), char *arg,
          int stacksize, int priority)
{
    int procSlot = -1;
    int i;
	int markClose = -1;
	
	if (ReadyList == NULL){
		if (priority != 6){
			return -1;
		}
	}
	else if (priority < 1 || priority > 5){
		return -1;
	}

    if (DEBUG && debugflag)
        USLOSS_Console("fork1(): creating process %s\n", name);

    // test if in kernel mode; halt if in user mode
    if ((USLOSS_PsrGet() & 1) == 0){
       printf("user mode\n");
       USLOSS_Halt(1);
    }

	disableInterrupts();
	
    // Return if stack size is too small
    if (stacksize < USLOSS_MIN_STACK){
       printf("stack too small\n");
       return -2;
    }
	
    // Is there room in the process table? What is the next PID?
	// procSlot = getpid() % MAXPROC;
    for (i = 0; i < MAXPROC; i++){
        if (ProcTable[i].status == OPEN){
           procSlot = i;
		   markClose = 0;
           break;
        } else {
			markClose = -1;
		}
    }
	
	USLOSS_Console("// procSlot = %d i = %d\n", procSlot, i);
	
	// all slot in procTable are closed
	if (markClose == -1){
		return -1;
	}
	
	// if this happen there is a problem with the loop above
	if (procSlot == -1){
		USLOSS_Console("procSlot is still -1?\n");
	}
	
    // fill-in entry in process table */
	
    if ( strlen(name) >= (MAXNAME - 1) ) {
        USLOSS_Console("fork1(): Process name is too long.  Halting...\n");
        USLOSS_Halt(1);
    }
    strcpy(ProcTable[procSlot].name, name);
    ProcTable[procSlot].startFunc = startFunc;
    if ( arg == NULL )
        ProcTable[procSlot].startArg[0] = '\0';
    else if ( strlen(arg) >= (MAXARG - 1) ) {
        USLOSS_Console("fork1(): argument too long.  Halting...\n");
        USLOSS_Halt(1);
    }
    else
        strcpy(ProcTable[procSlot].startArg, arg);

    ProcTable[procSlot].stackSize = stacksize;
	ProcTable[procSlot].priority = priority;
	ProcTable[procSlot].stack = malloc(stacksize);
	ProcTable[procSlot].pid = procSlot + 1;
	ProcTable[procSlot].status = READY;
	USLOSS_DeviceInput(0, 0, &(ProcTable[procSlot].time));
	
	// Current = &(ProcTable[procSlot]);
	
	if (Current == NULL){
		ProcTable[procSlot].parentPtr = NULL;
		// Current = &(ProcTable[procSlot]);
	} else {
		ProcTable[procSlot].parentPtr = Current;
		
		if (Current->childProcPtr == NULL){
			Current->childProcPtr = &ProcTable[procSlot];
		} else {
			// I would do it the other way around
			ProcTable[procSlot].nextSiblingPtr = Current->childProcPtr;
			Current->childProcPtr = &ProcTable[procSlot];
		}
	}
	
	USLOSS_Console("proccess to enqueue: %s\n", ProcTable[procSlot].name);
	
	// implement priority queue with ReadyList
	enqueue(&(ProcTable[procSlot]));
	
	// Current = &(ProcTable[procSlot]);
		
	i = 0;
	procPtr q = ReadyList;
	while (q != NULL){
		if (q->childProcPtr != NULL){
			USLOSS_Console("q = %s, q.child = %s\n", q->name, q->childProcPtr->name);
		} else {
			USLOSS_Console("no child\n");
		}
		q = q->nextProcPtr;
	}
	
	USLOSS_Console("enqueued: %s\n", ProcTable[procSlot].name);
	
	// Current = &(ProcTable[procSlot]);

    // Initialize context for this process, but use launch function pointer for
    // the initial value of the process's program counter (PC)
	
    USLOSS_ContextInit(&(ProcTable[procSlot].state),
                       ProcTable[procSlot].stack,
                       ProcTable[procSlot].stackSize,
                       NULL,
                       launch);

    // for future phase(s)
    p1_fork(ProcTable[procSlot].pid);

    // More stuff to do here...
	if (ProcTable[procSlot].priority != 6){
		dispatcher();
	}
	
	// enable interrupt
	enableInterrupts();
	
	// dispatcher();
	
	USLOSS_Console("before returning that -1, ProcTable[procSlot] = %s pid = %d\n", ProcTable[procSlot].name, ProcTable[procSlot].pid);
	
    return ProcTable[procSlot].pid;  // -1 is not correct! Here to prevent warning.
} /* fork1 */

/* ------------------------------------------------------------------------
   Name - launch
   Purpose - Dummy function to enable interrupts and launch a given process
             upon startup.
   Parameters - none
   Returns - nothing
   Side Effects - enable interrupts
   ------------------------------------------------------------------------ */
void launch()
{
	// USLOSS_Console("launched\n");
	if ((USLOSS_PsrGet() & 1) == 0){
       printf("user mode\n");
       USLOSS_Halt(1);
    }
	
    int result;

    if (DEBUG && debugflag)
        USLOSS_Console("launch(): started\n");

    // Enable interrupts
	enableInterrupts();

    // Call the function passed to fork1, and capture its return value
    result = Current->startFunc(Current->startArg);

    if (DEBUG && debugflag)
        USLOSS_Console("Process %d returned to launch\n", Current->pid);

    quit(result);

} /* launch */


/* ------------------------------------------------------------------------
   Name - join
   Purpose - Wait for a child process (if one has been forked) to quit.  If 
             one has already quit, don't wait.
   Parameters - a pointer to an int where the termination code of the 
                quitting process is to be stored.
   Returns - the process id of the quitting child joined on.
             -1 if the process was zapped in the join
             -2 if the process has no children
   Side Effects - If no child process has quit before join is called, the 
                  parent is removed from the ready list and blocked.
   ------------------------------------------------------------------------ */
int join(int *status)
{
	USLOSS_Console("Current that called join: %s\n", Current->name);
	// test if in kernel mode; halt if in user mode
    if ((USLOSS_PsrGet() & 1) == 0){
       printf("user mode\n");
       USLOSS_Halt(1);
    }
	
	disableInterrupts();
	
	if (Current->childProcPtr == NULL && Current->quitChild == NULL){
		enableInterrupts();
		return -2;
	}
	
	if (Current->quitChild != NULL){
		if (Current->quitChild->status != QUIT){
			Current->status = BLOCKED;
			short temppid = Current->pid;
			procPtr queue = ReadyList;
			
			while (queue->nextProcPtr != NULL){
				if (strcmp(queue->nextProcPtr->name, Current->name) == 0){
					queue->nextProcPtr = queue->nextProcPtr->nextProcPtr;
					break;
				}
				queue = queue->nextProcPtr;
			}
			
			// dispatcher();
			
			if (Current->status == BLOCKED){
				// wait
			}
			// should be unblocked
			if (Current->quitChild->status == QUIT){
				dispatcher();
			}
		}
	}
	
	// while(1){
		// if (Current->childProcPtr->status == QUIT){
			// USLOSS_Console("quitting\n");
			// break;
		// }
	// }
	
	*status = Current->childProcPtr->status;
	
	enableInterrupts();
	
	// if (isZapped()){
		// return -1;
	// }
	
	USLOSS_Console("Join before returning pid: %s\n", Current->name);
	
    return Current->pid;  // -1 is not correct! Here to prevent warning.
} /* join */

/* ------------------------------------------------------------------------
   Name - quit
   Purpose - Stops the child process and notifies the parent of the death by
             putting child quit info on the parents child completion code
             list.
   Parameters - the code to return to the grieving parent
   Returns - nothing
   Side Effects - changes the parent of pid child completion status list.
   ------------------------------------------------------------------------ */
void quit(int status)
{
	USLOSS_Console("Current that called quit: %s\n", Current->name);
	// Keep track of what is currently being looked at
	// Grab current process Remove process from the queue
	// test if in kernel mode; halt if in user mode
    if ((USLOSS_PsrGet() & 1) == 0){
       printf("user mode\n");
       USLOSS_Halt(1);
    }
	
	disableInterrupts();
	
	// USLOSS_Console("Current = %s Current->childProcPtr = %s\n", Current->name, Current->childProcPtr->name);
	
	if (Current->childProcPtr != NULL){
		USLOSS_Halt(1);
	}
	
	procPtr queue = ReadyList;
	// procPtr releasedProc;
	
	procPtr parent = Current->parentPtr;
	
	clean(Current);
	Current->status = QUIT;
	
	if (Current->parentPtr == NULL){
		USLOSS_Console("parent is nul is that a problem?\n");
	} else {
		procPtr temp = parent;
		
		while (temp->childProcPtr != NULL){
			if (strcmp(temp->childProcPtr->name, Current->name) == 0){
				temp->childProcPtr = temp->childProcPtr->childProcPtr;
				break;
			}
			temp = temp->childProcPtr;
		}
		
		temp = parent;
		
		// add to quitChild list
		if (temp->quitChild == NULL){
			temp->quitChild = Current;
		} else {
			while(temp->nextQuitChild != NULL){
				temp = temp->nextQuitChild;
			}
			temp->nextQuitChild = Current;
		}
	}
	
	if (parent->status == BLOCKED){
		unblockMe(parent);
	}
	
	// dispatcher();
	// if(queue == NULL){
		// USLOSS_Console("queue is null, switch to sentinel\n");
	// } else {
		// if (queue->nextProcPtr == NULL){
			// USLOSS_Console("there is only sentinel: %s error?\n", queue->name);
		// }
		// if (queue->pid == Current->pid){
			// Current = queue;
			// Current->status = QUIT;
			// queue = queue->nextProcPtr;
		// } else {
			// while (queue->nextProcPtr != NULL){
				// if(queue->nextProcPtr->pid == Current->pid){
					// // releasedProc = queue->nextProcPtr;
					// Current = queue->nextProcPtr;
					// Current->status = QUIT;
					// queue->nextProcPtr = queue->nextProcPtr->nextProcPtr;
				// }
				// queue = queue->nextProcPtr;
			// }
		// }
		// ReadyList = queue;
	// }
	
	// clean(releasedProc);
	
    p1_quit(Current->pid);
	dispatcher();
	// enableInterrupts();
} /* quit */


/* ------------------------------------------------------------------------
   Name - dispatcher
   Purpose - dispatches ready processes.  The process with the highest
             priority (the first on the ready list) is scheduled to
             run.  The old process is swapped out and the new process
             swapped in.
   Parameters - none
   Returns - nothing
   Side Effects - the context of the machine is changed
   ----------------------------------------------------------------------- */
void dispatcher(void)
{
	if (Current != NULL){
		USLOSS_Console("Current that called dispatcher: %s\n", Current->name);
	} else {
		USLOSS_Console("Current that called dispatcher: NULL\n");
	}
	// sentinel shouldn't have to call dispatcher
    procPtr nextProcess = NULL;

	if ((USLOSS_PsrGet() & 1) == 0){
       printf("user mode\n");
       USLOSS_Halt(1);
    }
	
	int t1;
	USLOSS_DeviceInput(0, 0, &t1);
	
	if (Current == NULL || Current->status == QUIT){
		USLOSS_Console("in if\n");
		procStruct temp = dequeue();
		Current = &temp;
		USLOSS_DeviceInput(0, 0, &(Current->time));
		Current->status = RUNNING;
		p1_switch(-1, Current->pid);
		USLOSS_ContextSwitch(NULL, &(Current->state));
	} else {
		USLOSS_Console("in else\n");
		procStruct temp = dequeue();
		nextProcess = &temp;
		
		if (Current->status == RUNNING){
			Current->status = READY;
			enqueue(Current);
		}
		
		nextProcess->status = RUNNING;
		
		USLOSS_Console("Current = %s, nextProc = %s\n", Current->name, nextProcess->name);
		p1_switch(Current->pid, nextProcess->pid);
		enableInterrupts();
		USLOSS_ContextSwitch(&(Current->state), &(nextProcess->state));
		// return;
	}
	// else if (Current->status == BLOCKED){
		// procPtr queue = ReadyList;
		// while (queue != NULL){
			// if (queue->nextProcPtr == NULL){
				// return;
			// } else {
				// if (queue->nextProcPtr->pid == Current->pid){
					// queue->nextProcPtr = queue->nextProcPtr->nextProcPtr;
					// break;
				// }
			// }
			// queue = queue->nextProcPtr;
		// }
		
		// procStruct temp = dequeue();
		// nextProcess = &temp;
		
		// USLOSS_ContextSwitch(&(Current->state), &(nextProcess->state));
		// // return;
	// } else {
	
			// int i = 0;
		// procPtr queue = ReadyList;
		// while (queue != NULL){
			// USLOSS_Console("before dequeue %d = %s prior = %d\n", i, queue->name, queue->priority);
			// queue = queue->nextProcPtr;
			// i++;
		// }
		
		// // quit didn't happen, so maybe we need to dequeue it ourselves
		// // if (Current != NULL){
			// // if (Current->status != QUIT){
				// // // first one comes out is the current process
				// // procStruct temp = dequeue();
				// // Current = &temp;
			// // }
		// // } else {
			// // procStruct temp = dequeue();
			// // Current = &temp;
		// // }
		// // then next process gets dequeued
		// procStruct temp = dequeue();
		// nextProcess = &temp;
		
		// // if (ReadyList == NULL || nextProcess->status != QUIT){
			// // USLOSS_Console("readyList empty enqueue back the sentinel\n");
			// // USLOSS_Console("is nextProcess sentinel: %s\n", nextProcess->name);
			// // enqueue(nextProcess);
		// // }
		
		// // if (strcmp(Current->name, nextProcess->name) != 0){
			// if (Current->status != QUIT){
				// enqueue(Current);
			// }
		// // }
		
		// if (nextProcess == NULL){
		   // return;
		// }
		
		// USLOSS_Console("should be sentinel when switching: %s current: %s\n", nextProcess->name, Current->name);
		
		// i = 0;
		// queue = ReadyList;
		// while (queue != NULL){
			// USLOSS_Console("queue %d = %s\n", i, queue->name);
			// queue = queue->nextProcPtr;
			// i++;
		// }
		
		// p1_switch(Current->pid, nextProcess->pid);
		
		// USLOSS_ContextSwitch(&(Current->state), &(nextProcess->state));
	// }
	// Current should also switch
	// Current = nextProcess;
} /* dispatcher */


/* ------------------------------------------------------------------------
   Name - sentinel
   Purpose - The purpose of the sentinel routine is two-fold.  One
             responsibility is to keep the system going when all other
             processes are blocked.  The other is to detect and report
             simple deadlock states.
   Parameters - none
   Returns - nothing
   Side Effects -  if system is in deadlock, print appropriate error
                   and halt.
   ----------------------------------------------------------------------- */
int sentinel (char *dummy)
{
	if ((USLOSS_PsrGet() & 1) == 0){
       printf("user mode\n");
       USLOSS_Halt(1);
    }
	
    if (DEBUG && debugflag)
        USLOSS_Console("sentinel(): called\n");
    while (1)
    {
        checkDeadlock();
        USLOSS_WaitInt();
    }
} /* sentinel */


/* check to determine if deadlock has occurred... */
static void checkDeadlock()
{
	if ((USLOSS_PsrGet() & 1) == 0){
       printf("user mode\n");
       USLOSS_Halt(1);
    }
} /* checkDeadlock */

void illegalInstructionHandler(int dev, void *arg)
{
	if ((USLOSS_PsrGet() & 1) == 0){
       printf("user mode\n");
       USLOSS_Halt(1);
    }
	
    if (DEBUG && debugflag)
        USLOSS_Console("illegalInstructionHandler() called\n");
} /* illegalInstructionHandler */

void clockHandler(int dev, void *arg){
	if ((USLOSS_PsrGet() & 1) == 0){
       printf("user mode\n");
       USLOSS_Halt(1);
    }
	
    if (DEBUG && debugflag)
        USLOSS_Console("clockHandler() called\n");
	
	disableInterrupts();
	int t1;
	USLOSS_DeviceInput(0, 0, &t1);
	
	if (t1 - ReadyList->time >= 80000){
		dispatcher();
	}
	
	enableInterrupts();
}