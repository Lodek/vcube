/* Simulador Vcube
 * Autor: Bruno Gomes
 * Data da ultima atualizacao: 2023-02-08
 * Funcionalidade: Simulador do algoritimo VCube usando biblioteca SMPL com falsas suspeitas
 */

#include <stdlib.h>
#include <stdio.h>

#include "vcube.h"

#define DEFAULT_MAX_TIME 200.0
#define DEFAULT_TEST_PERIOD 10.0

typedef struct Args {
    int process_count;
    double false_negative_probability;
    double max_time;
    double test_period;
} Args;

Args parse_args(int argc, char *argv[]);

int main(int argc, char *argv[]) {
    Args args = parse_args(argc, argv);

    printf("=========================================================================================\n");
    printf("Starting VCube simmulation:\n");
    printf("%d processes; false negative probability = %.2f; test period = %.2f; deadline = %.2f\n",
            args.process_count, args.false_negative_probability, args.test_period, args.max_time);
    printf("=========================================================================================\n");

    SimmResult *result = run_simm(args.process_count, args.false_negative_probability, args.test_period, args.max_time);


    printf("=========================================================================================\n");
    printf("Simmulation Results:\n");
    printf("\tcorrect process count: %d\n", result->remaining_processes);
    printf("\ttermination count: %d\n", result->termination_count);
    printf("\ttest count: %d\n", result->test_count);
    printf("\tfalse negative test count: %d\n", result->false_negative_count);

    printf("=========================================================================================\n");
    printf("Events:\n");
    for (int i=0; i < result->event_count; i++) {
        Event *event = &result->events[i];
        printf("\t%.2f\t%d\t%s\n", event->time, event->type, event->msg);
    }

    return 0;
}

/*
 * Parse command line arguments.
 */
Args parse_args(int argc, char *argv[]) {
    if (argc < 3) goto error;

    double probability = atof(argv[2]);
    if (probability < 0 || probability > 1) goto error;

    Args args = {};
    args.process_count = atoi(argv[1]);
    args.false_negative_probability = probability;
    args.max_time = argc >= 4 ? atof(argv[3]) : DEFAULT_MAX_TIME;
    args.test_period = argc >= 5 ? atof(argv[4]) : DEFAULT_TEST_PERIOD;
    return args;

    error:
        printf("Usage: [process count] [false_negative_probability (from 0 to 1)] [max_time=%lf] [test_period=%lf]\n", DEFAULT_MAX_TIME, DEFAULT_TEST_PERIOD);
        exit(1);
}
