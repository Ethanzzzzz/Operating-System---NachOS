// scheduler.cc
//	Routines to choose the next thread to run, and to dispatch to
//	that thread.
//
// 	These routines assume that interrupts are already disabled.
//	If interrupts are disabled, we can assume mutual exclusion
//	(since we are on a uniprocessor).
//
// 	NOTE: We can't use Locks to provide mutual exclusion here, since
// 	if we needed to wait for a lock, and the lock was busy, we would
//	end up calling FindNextToRun(), and that would put us in an
//	infinite loop.
//
// 	Very simple implementation -- no priorities, straight FIFO.
//	Might need to be improved in later assignments.
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "scheduler.h"

#include "copyright.h"
#include "debug.h"
#include "main.h"

//----------------------------------------------------------------------
// Scheduler::Scheduler
// 	Initialize the list of ready but not running threads.
//	Initially, no ready threads.
//----------------------------------------------------------------------

Scheduler::Scheduler() {
    // readyList = new List<Thread *>;
    readyList = new MLFQ;
    toBeDestroyed = NULL;
}

//----------------------------------------------------------------------
// Scheduler::~Scheduler
// 	De-allocate the list of ready threads.
//----------------------------------------------------------------------

Scheduler::~Scheduler() {
    delete readyList;
}

//----------------------------------------------------------------------
// Scheduler::ReadyToRun
// 	Mark a thread as ready, but not running.
//	Put it on the ready list, for later scheduling onto the CPU.
//
//	"thread" is the thread to be put on the ready list.
//----------------------------------------------------------------------

void Scheduler::ReadyToRun(Thread *thread) {
    ASSERT(kernel->interrupt->getLevel() == IntOff);
    DEBUG(dbgThread, "Putting thread on ready list: " << thread->getName());
    // cout << "Putting thread on ready list: " << thread->getName() << endl ;
    thread->setStatus(READY);
    readyList->Append(thread);
}

//----------------------------------------------------------------------
// Scheduler::FindNextToRun
// 	Return the next thread to be scheduled onto the CPU.
//	If there are no ready threads, return NULL.
// Side effect:
//	Thread is removed from the ready list.
//----------------------------------------------------------------------

Thread* Scheduler::FindNextToRun() {
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    if (readyList->IsEmpty()) {
        return NULL;
    } else {
        return readyList->RemoveFront();
    }
}

//----------------------------------------------------------------------
// Scheduler::Run
// 	Dispatch the CPU to nextThread.  Save the state of the old thread,
//	and load the state of the new thread, by calling the machine
//	dependent context switch routine, SWITCH.
//
//      Note: we assume the state of the previously running thread has
//	already been changed from running to blocked or ready (depending).
// Side effect:
//	The global variable kernel->currentThread becomes nextThread.
//
//	"nextThread" is the thread to be put into the CPU.
//	"finishing" is set if the current thread is to be deleted
//		once we're no longer running on its stack
//		(when the next thread starts running)
//----------------------------------------------------------------------

void Scheduler::Run(Thread *nextThread, bool finishing) {
    Thread *oldThread = kernel->currentThread;

    ASSERT(kernel->interrupt->getLevel() == IntOff);

    if (finishing) {  // mark that we need to delete current thread
        ASSERT(toBeDestroyed == NULL);
        toBeDestroyed = oldThread;
    }

    if (oldThread->space != NULL) {  // if this thread is a user program,
        oldThread->SaveUserState();  // save the user's CPU registers
        oldThread->space->SaveState();
    }

    oldThread->CheckOverflow();  // check if the old thread
                                 // had an undetected stack overflow

    kernel->currentThread = nextThread;  // switch to the next thread
    nextThread->setStatus(RUNNING);      // nextThread is now running

    DEBUG(dbgThread, "Switching from: " << oldThread->getName() << " to: " << nextThread->getName());

    // MP3
    DEBUG(dbgScheduler, "[E] Tick [" << kernel->stats->totalTicks << "]: Thread [" << nextThread->getID() << "] is now selected for execution, thread [" << oldThread->getID() << "] is replaced, and it has executed [" << oldThread->CPUBurstTime << "] ticks");

    // This is a machine-dependent assembly language routine defined
    // in switch.s.  You may have to think
    // a bit to figure out what happens after this, both from the point
    // of view of the thread and from the perspective of the "outside world".

    SWITCH(oldThread, nextThread);

    // we're back, running oldThread

    // interrupts are off when we return from switch!
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    DEBUG(dbgThread, "Now in thread: " << oldThread->getName());

    CheckToBeDestroyed();  // check if thread we were running
                           // before this one has finished
                           // and needs to be cleaned up

    if (oldThread->space != NULL) {     // if there is an address space
        oldThread->RestoreUserState();  // to restore, do it.
        oldThread->space->RestoreState();
    }
}

//----------------------------------------------------------------------
// Scheduler::CheckToBeDestroyed
// 	If the old thread gave up the processor because it was finishing,
// 	we need to delete its carcass.  Note we cannot delete the thread
// 	before now (for example, in Thread::Finish()), because up to this
// 	point, we were still running on the old thread's stack!
//----------------------------------------------------------------------

void Scheduler::CheckToBeDestroyed() {
    if (toBeDestroyed != NULL) {
        delete toBeDestroyed;
        toBeDestroyed = NULL;
    }
}

//----------------------------------------------------------------------
// Scheduler::Print
// 	Print the scheduler state -- in other words, the contents of
//	the ready list.  For debugging.
//----------------------------------------------------------------------
void Scheduler::Print() {
    cout << "Ready list contents:\n";
    readyList->Apply(ThreadPrint);
}

// Mp3 implementation for Multi-Level Feedback Queue

double getRemainTime(Thread* t) {
    if(t->getStatus() == RUNNING)
        return t->apxBurstTime - (t->CPUBurstTime + kernel->stats->totalTicks - t->cacheBurstTime);
    else
        return t->apxBurstTime - t->CPUBurstTime;
}

int cmpRemainTime(Thread* t1, Thread* t2) {
    double time1, time2;
    time1 = getRemainTime(t1);
    time2 = getRemainTime(t2);
    if(time1 == time2) return t1->getID() - t2->getID(); // if same remain time, compare with id
    else return static_cast<int>(time1 - time2); // convert type from float to int
}
int cmpPriority(Thread* t1, Thread* t2) {
   if(t1->priority == t2->priority) return t1->getID() - t2->getID(); // if same priority, compare with id
   else return t2->priority - t1->priority;
}

void aging(Thread* t){
    if(kernel->stats->totalTicks - t->beReadyTime > 1500){
        DEBUG(dbgScheduler, "[C] Tick [" << kernel->stats->totalTicks << "]: Thread [" << t->getID() << "] changes its priority from [" << t->priority << "] to [" << min(t->priority + 10, 149) << "]");
        t->priority = min(t->priority + 10, 149);
        t->beReadyTime = kernel->stats->totalTicks;
    }
}

void Scheduler::updatePriority() {
    kernel->scheduler->readyList->Apply(aging);
}

bool Scheduler::shouldPreempt() {
    Thread* cur = kernel->currentThread;
    Thread* t;
    MLFQ* readyQueue = kernel->scheduler->readyList;
    const int timeQuantum = 100;
    float remainTime, curRemainTime, runningBurstTime;
    Scheduler* a;
    MLFQ* b;
    SortedList<Thread*>* c;
    switch (cur->getLevel()) {
        case 1:
            if(kernel->scheduler->readyList->L1->IsEmpty()) return false;
            t = kernel->scheduler->readyList->L1->Front();
            if(!t) return false;
            remainTime = getRemainTime(t);
            curRemainTime = getRemainTime(cur);
            if(remainTime < curRemainTime)
                return true;
            break;
        case 2:
            if(!kernel->scheduler->readyList->L1->IsEmpty())
                return true;
            break;
        case 3:
            runningBurstTime = kernel->stats->totalTicks - cur->cacheBurstTime;
            if(!readyQueue->L1->IsEmpty() || !readyQueue->L2->IsEmpty() || runningBurstTime > timeQuantum)
                return true;
            break;
        default:
            break;
    }
    return false;
}

MLFQ::MLFQ() {
    L1 = new SortedList<Thread*>(cmpRemainTime);
    L2 = new SortedList<Thread*>(cmpPriority);
    L3 = new List<Thread*>;
}

MLFQ::~MLFQ() {
    delete L1;
    delete L2;
    delete L3;
}

void MLFQ::Append(Thread* t) {
    DEBUG(dbgScheduler, "[A] Tick [" << kernel->stats->totalTicks << "]: Thread [" << t->getID() << "] is inserted into queue L[" << t->getLevel() << "]");

    int p = t->priority;

    if(p >= 100) L1->Insert(t);
    else if(p >= 50) L2->Insert(t);
    else L3->Append(t);
}

Thread* MLFQ::RemoveFront() {
    Thread* t;
    if(!L1->IsEmpty()) t = L1->RemoveFront();
    else if(!L2->IsEmpty()) t = L2->RemoveFront();
    else if(!L3->IsEmpty()) t = L3->RemoveFront();
    else t = NULL;
    
    DEBUG(dbgScheduler, "[B] Tick [" << kernel->stats->totalTicks << "]: Thread [" << t->getID() << "] is removed from queue L[" << t->getLevel() << "]");

    return t;
}

void MLFQ::Apply(void (*f)(Thread*)) {
    L1->Apply(f);
    L2->Apply(f);
    L3->Apply(f);
}

bool MLFQ::IsEmpty() {
    return L1->IsEmpty() && L2->IsEmpty() && L3->IsEmpty();
}