
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "smpl.h"
#include "cisj.c"
#include "vcube.h"
#include "rand_init.h"


static const char* CORRECT_STR = "CORRECT";
static const char* FAULTY_STR = "FAULTY";
static const char* FALSE_NEGATIVE_STR = "FALSE_NEGATIVE";

#define SCHEDULE_TEST 1

/*
 * run_simm acts as the simulator's event loop.
 * It sequentially consumes the events previously schedule in the smpl library and processes them accordingly.
 */
SimmResult* run_simm(int process_count, double false_negative_probability,
              double test_period, double deadline)
{
    Ctx ctx = initialize(process_count, false_negative_probability, test_period, deadline);
    ProcessFacility *processes = ctx.processes;

    int token; // signals the process being currently executed
    int event; // last emitted event

    while(time() < deadline) {
        cause(&event, &token);
        switch(event) {
            case SCHEDULE_TEST: 
                // terminated process cannot perform tests
                if (processes[token].has_terminated) {
                    break;
                }

                vcube_test(ctx, token);
                schedule(SCHEDULE_TEST, test_period, token);

                eprintf("%4.1f: Process %d state: ", time(), token);
                for (int j = 0; j < process_count; j++) {
                    eprintf("[%d]: %d, ", j, processes[token].states[j]);
                }
                eprintf("\n");
                break;
        }
    }
    // TODO free processes and their state vectors
    ctx.result->remaining_processes -= ctx.result->termination_count;
    return ctx.result;
}




/*
 * Initialize smpl library, build facilities and processes.
 * Return pointer to allocated array of processes
 */
Ctx initialize(int process_count, double fail_probability, double test_period, double max_time) {

    // smpl init
    smpl(0, "Simm. name");
    reset();
    stream(1);
    
    rand_init();

    ProcessFacility *processes = (ProcessFacility*) malloc(sizeof(ProcessFacility)*process_count);
    if(processes == NULL) {
        eprintf("failed to allocate processes\n");
        exit(1);
    }

    char fa_name[5]; // facility name
    for(int i=0; i<process_count; i++) {
        memset(fa_name, '\0', 5);
        sprintf(fa_name, "%d", i);
        processes[i].id = facility(fa_name, 1);

        int *states = (int*) malloc(sizeof(int)*process_count);
        if (states == NULL) {
            eprintf("could not allocate states\n");
            exit(1);
        }

        processes[i].states = states;

        // initialize states to 0 in order to stabalize system
        for (int j =0; j<process_count; j++) {
            // this could probably be optimized with calloc
            processes[i].states[j] = 0;
        }
    }

    for(int i=0; i<process_count; i++) {
        schedule(SCHEDULE_TEST, 0, i);
    }

    // Allocates one event per process per test interval
    // This could be severely optimizaed by on demand allocation but it's good enough for now.
    Event *events = (Event*) malloc(sizeof(Event)* process_count * 100 * (max_time / test_period));
    if(events == NULL) {
        eprintf("failed to allocate events\n");
        exit(1);
    }

    SimmResult *result = (SimmResult*) malloc(sizeof(SimmResult));
    if (result == NULL) {
        eprintf("could not allocate SimmResult\n");
        exit(1);
    }

    result->event_count = 0;
    result->false_negative_count = 0;
    result->test_count = 0;
    result->termination_count = 0;
    result->remaining_processes = process_count;
    result->events = events;

    Ctx ctx = {};
    ctx.processes = processes;
    ctx.process_count = process_count;
    ctx.fail_probability = fail_probability;
    ctx.result = result;

    return ctx;
}


int is_tester_suspected(int process_count, int tester_id, int *testee_states) {
    return IS_EVEN(testee_states[tester_id]) ? 0 : 1;
}

/*
 * update_states updates tester's states vector with
 * all information more up to date from testee.
 */
void update_states(int process_count, int tester_id, int *tester_states, int *testee_states) {
    for (int i=0; i < process_count; i++) {
        int ours = tester_states[i];
        int theirs = testee_states[i];
        if (theirs > ours) {
            tester_states[i] = theirs;
        }
    }
}


/*
 * next_timestamp return the updated timestamp for the given test result.
 * the returned timestamp is updated if a new event was detected.
 */
int next_timestamp(int timestamp, TestResult test_result) {
    int is_correct = test_result == CORRECT ? 1 : 0;
    int inc = (IS_EVEN(timestamp) && is_correct) || (!IS_EVEN(timestamp) && !is_correct) ? 0 : 1;
    return timestamp + inc;
}


/*
 * vcube_test is the public interface function for the vcube implementation.
 * it receives the tester's id, the list of processes and the process_count.
 * vcube_test sequentially tests all clusters for the given process.
 */
void vcube_test(Ctx ctx, int id) {
    eprintf("%4.1f: Test round for process %d\n", time(), id);
    int cluster_count = (int) ceill(log2(ctx.process_count));

    for (int s=1; s <= cluster_count; s++) {
        if (ctx.processes[id].has_terminated) break;

        eprintf("%4.1f: Testing process %d, cluster %d\n", time(), id, s);
        test_cluster(ctx, id, s);
    }
}

/*
 * test_cluster executes the associated test for cluster `s` and process `id`.
 * For all correct processes tested, test_cluster updates the tester's state vector
 * by fetch missing events from the testee's event vector.
 */
void test_cluster(Ctx ctx, int id, int s) {
    for (int target=0; target < ctx.process_count; target++) {
        if (is_first_correct_process_in_cis(id, target, s, ctx.processes[id].states)) {
            int current = ctx.processes[id].states[target];

            TestResult result = test(ctx, ctx.processes[target].id);
            ctx.result->test_count++;
            if (result == FALSE_NEGATIVE) {
                ctx.result->false_negative_count++;
                log_event(ctx, FALSE_NEGATIVE_EVENT, "Process %d got a false negative for %d", id, target);
            }

            eprintf("%4.1f: %d -> %d: %s\n", time(), id, target, to_str(result));

            ctx.processes[id].states[target] = next_timestamp(current, result);

            if (result == CORRECT) {
                if (is_tester_suspected(ctx.process_count, id, ctx.processes[target].states)) {
                    eprintf("%4.1f: Process %d suspected as failed by process %d: Terminating\n", time(), id, target);
                    log_event(ctx, SELF_TERMINATION, "Process %d terminated itself because process %d suspected it", id, target);
                    ctx.processes[id].has_terminated = 1;
                    ctx.result->termination_count++;
                    request(ctx.processes[id].id, id, 0);
                    return;
                }
                update_states(ctx.process_count, id, ctx.processes[id].states, ctx.processes[target].states);
            }
        }
    }
}

const char* to_str(TestResult test_result) {
    switch (test_result) {
        case CORRECT:
            return CORRECT_STR;
        case FAULTY:
            return FAULTY_STR;
        case FALSE_NEGATIVE:
            return FALSE_NEGATIVE_STR;
        default:
            return NULL;
    }
}

/*
 * return wheter process `tester` is the first correct process for cis(target, s).
 * The tester's states vector is checked in in order to determine which is the first correct process.
 */
int is_first_correct_process_in_cis(int tester, int target, int s, int *states) {
    node_set *nodes = cis(target, s);
    int result = 0;
    for (int i=0; i < nodes->size; i++) {
        int pid = nodes->nodes[i];

        if (pid == tester) {
            result = 1;
            break;
        }
        else if (IS_FAULTY(states[pid])) {
            // if process is faulty we keep going
            // as we must determine whether tester is the
            // first *correct* process in the cis(target, s)
            continue;
        }
        else {
            break;
        }
    }
    set_free(nodes);
    return result;
}

// Tests process `pid`.
// test return a TestResult, indicating a correct process, an incorrect process of a false negative.
// false negative probability is determined by simmulation parameters.
TestResult test(Ctx ctx, int pid) {
    // smpl status function returns 0 if the facility has been released
    if (status(pid) == 1) {
        return FAULTY;
    } 

    if ((float)(rand() % 100) < ctx.fail_probability * 100.0) {
        return FALSE_NEGATIVE;
    }
    else {
        return CORRECT;
    }
}

void log_event(Ctx ctx, EventType type, char *fmt, ...) {
    char *msg = (char*) malloc(sizeof(char)*255);
    if (msg == NULL) {
        eprintf("malloc error");
        exit(1);
    }

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, 255, fmt, ap);
    va_end(ap);

    Event event = {};
    event.type = type;
    event.time = time();
    event.msg = msg;

    int *count = &ctx.result->event_count;
    ctx.result->events[*count] = event;
    *count = *count + 1;
}
