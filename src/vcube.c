/* Simulador Vcube
 * Autor: Bruno Gomes
 * Data da ultima atualizacao: 2023-01-26
 * Funcionalidade: Simulador do algoritimo VCube usando biblioteca SMPL
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "smpl.h"
#include "cisj.c"

#define test 1
#define fault 2
#define recovery 3

#define IS_EVEN(num) ((num % 2) == 0)

#define IS_CORRECT(timestamp) ((timestamp % 2) == 0)
#define IS_FAULTY(timestamp) (!IS_CORRECT(timestamp))

typedef struct {
    int id; // process id, eg simulated entity (aka facility)
    int *states; // processes state vector
    // indicates if process missed a round while crashed
    int has_missed_test; 
} ProcessFacility;

typedef struct Args {
    int process_count;
    int scenario;
} Args;


void test_cluster(int id, int s, ProcessFacility *processes, int process_count);
void vcube_test(int id, ProcessFacility *processes, int process_count);
int next_timestamp(int timestamp, int is_correct);
void update_states(int process_count, int tester_id, int *tester_states, int *testee_states);
int is_first_correct_process_in_cis(int tester, int target, int s, int *states);
Args parse_args(int argc, char *argv[]);
ProcessFacility* initialize(int process_count);
void run_simm(ProcessFacility *processes, int process_count, float test_period, float deadline);
int is_process_correct(ProcessFacility *processes, int id);


int main(int argc, char *argv[]) {
    Args args = parse_args(argc, argv);
    ProcessFacility *processes = initialize(args.process_count);

    switch (args.scenario) {
        case 0:
            schedule_scenario_0(args.process_count);
            break;
        case 1:
            schedule_scenario_1(args.process_count);
            break;
        case 2:
            schedule_scenario_2(args.process_count);
            break;
        default:
            printf("unkown scenario %d!", args.scenario);
            exit(1);
    }

    run_simm(processes, args.process_count, 10, 40);
}

void schedule_scenario_0(int process_count) {
    for(int i=process_count/2; i<process_count; i++)
        schedule(test, 0.0, i);
}

void schedule_scenario_1(int process_count) {
    for(int i=0; i<process_count; i++)
        schedule(test, 0.0, i);

    // schedule process 2 to fail then crash every
    // 10 units of time
    for(int i=1; i<=5; i++) {
        if (!IS_EVEN(i))
            schedule(fault, 9.0*i, 2);
        else
            schedule(recovery, 9.0*i, 2);
    }
}

void schedule_scenario_2(int process_count) {
    // indefinitely crashes half the system
    for(int i=process_count/2; i<process_count; i++)
        schedule(fault, 0.0, i);

    for(int i=0; i<process_count; i++)
        schedule(test, 0.0, i);
}


/*
 * run_simm acts as the simulator's event loop.
 * It sequentially consumes the events previously schedule in the smpl library and processes them accordingly.
 */
void run_simm(ProcessFacility *processes, int process_count, float test_period, float deadline) {
    printf("Starting vCube simmulation:\n");
    printf("%d processes; test period = %.2f; deadline = %.2f\n", process_count, test_period, deadline);
    printf("========================================================\n");

    int token; // signals the process being currently executed
    int event; // last emitted event

    while(time() < deadline) {
        cause(&event, &token);
        switch(event) {
            case test: 
                // break out of the switch as a crashed process cannot perform tests
                if (!is_process_correct(processes, token)) {
                    processes[token].has_missed_test = 1;
                    break;
                }

                vcube_test(token, processes, process_count);
                schedule(test, test_period, token);

                printf("%4.1f: Process %d state: ", time(), token);
                for (int j = 0; j < process_count; j++) {
                    printf("[%d]: %d, ", j, processes[token].states[j]);
                }
                printf("\n");
                break;
            case fault:
                request(processes[token].id, token, 0);
                printf("%4.1f: Proccess %d failed!\n", time(), token);
                break;
            case recovery:
                release(processes[token].id, token);
                // if the process has missed a test, make it test
                if (processes[token].has_missed_test) {
                    processes[token].has_missed_test = 0;
                    schedule(test, 0.0, token); 
                }
                printf("%4.1f: Process %d recovered!\n", time(), token);
                break;
        }
    }
}


/*
 * Parse command line arguments.
 * Expect a single integer argument representing the process count.
 * Return process count
 */
Args parse_args(int argc, char *argv[]) {
    if (argc < 2) {
        puts("Usage: [process count] [scenario=0]");
        exit(1);
    }

    int n = atoi(argv[1]);
    int scenario = 0;
    if (argc == 3)
        scenario = atoi(argv[2]);
    Args args = {n, scenario};
    return args;
}


/*
 * Initialize smpl library, build facilities and processes.
 * Return pointer to allocated array of processes
 */
ProcessFacility* initialize(int process_count) {

    // smpl init
    smpl(0, "Simm. name");
    reset();
    stream(1);

    ProcessFacility *processes = (ProcessFacility*) malloc(sizeof(ProcessFacility)*process_count);
    if(processes == NULL) {
        printf("failed to allocate processes\n");
        exit(1);
    }

    char fa_name[5]; // facility name
    for(int i=0; i<process_count; i++) {
        memset(fa_name, '\0', 5);
        sprintf(fa_name, "%d", i);
        processes[i].id = facility(fa_name, 1);

        int *states = (int*) malloc(sizeof(int)*process_count);
        if (states == NULL) {
            printf("could not allocate states\n");
            exit(1);
        }

        processes[i].states = states;

        // initialize states to -1 for all processes other than self
        for (int j =0; j<process_count; j++) {
            processes[i].states[j] = j == i ? 0 : -1;
        }
    }
    return processes;
}


/*
 * update_states updates tester's states vector with
 * all information more up to date from testee.
 */
void update_states(int process_count, int tester_id, int *tester_states, int *testee_states) {
    for (int i=0; i < process_count; i++) {
        if (i == tester_id) continue;

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
int next_timestamp(int timestamp, int is_correct) {
    // -1 means not initialized
    if (timestamp == -1) {
        // even numbers indicate the process is correct in the timestamp system
        return is_correct ? 0 : 1;
    }

    int inc = (IS_EVEN(timestamp) && is_correct) || (!IS_EVEN(timestamp) && !is_correct) ? 0 : 1;
    return timestamp + inc;
}

/*
 * Return 1 is process is up, 0 otherwise
 */
int is_process_correct(ProcessFacility *processes, int id) {
    // smpl status function returns 0 if the facility has been released
    return status(processes[id].id) == 0;
}

/*
 * vcube_test is the public interface function for the vcube implementation.
 * it receives the tester's id, the list of processes and the process_count.
 * vcube_test sequentially tests all clusters for the given process.
 */
void vcube_test(int id, ProcessFacility *processes, int process_count) {
    printf("%4.1f: Test round for process %d\n", time(), id);
    int cluster_count = (int) ceill(log2(process_count));

    for (int s=1; s <= cluster_count; s++) {
        printf("%4.1f: Testing process %d, cluster %d\n", time(), id, s);
        test_cluster(id, s, processes, process_count);
    }
}

/*
 * test_cluster executes the associated test for cluster `s` and process `id`.
 * For all correct processes tested, test_cluster updates the tester's state vector
 * by fetch missing events from the testee's event vector.
 */
void test_cluster(int id, int s, ProcessFacility *processes, int process_count) {
    for (int target=0; target < process_count; target++) {
        if (is_first_correct_process_in_cis(id, target, s, processes[id].states)) {
            int current = processes[id].states[target];

            int is_correct = is_process_correct(processes, target);
            processes[id].states[target] = next_timestamp(current, is_correct);
            if (is_correct) {
                printf("%4.1f: %d -> %d: CORRECT\n", time(), id, target);
                update_states(process_count, id, processes[id].states, processes[target].states);
            }
            else {
                printf("%4.1f: %d -> %d: FAULTY\n", time(), id, target);
            }
        }
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
