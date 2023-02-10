typedef struct {
    int id; // process id, eg simulated entity (aka facility)
    int *states; // processes state vector
    int has_terminated;
} ProcessFacility;

typedef enum {
    CORRECT,
    FAULTY,
    FALSE_NEGATIVE,
} TestResult;

typedef enum {
    FALSE_NEGATIVE_EVENT,
    SELF_TERMINATION,
} EventType;

typedef struct {
    double time;
    EventType type;
    char *msg;
} Event;

typedef struct {
    Event *events;
    int event_count;
    int false_negative_count;
    int test_count;
    int termination_count;
    int remaining_processes;
} SimmResult;

typedef struct {
    ProcessFacility *processes;
    int process_count;
    double fail_probability;
    SimmResult *result;
} Ctx;

#define IS_EVEN(num) ((num % 2) == 0)

#define IS_CORRECT(timestamp) ((timestamp % 2) == 0)
#define IS_FAULTY(timestamp) (!IS_CORRECT(timestamp))
#define eprintf(...) fprintf(stderr, __VA_ARGS__)


void test_cluster(Ctx ctx, int id, int s);
void vcube_test(Ctx ctx, int id);
int next_timestamp(int timestamp, TestResult result);
void update_states(int process_count, int tester_id, int *tester_states, int *testee_states);
int is_first_correct_process_in_cis(int tester, int target, int s, int *states);
Ctx initialize(int process_count, double fail_probability, double test_period, double max_time);
SimmResult* run_simm(int process_count, double false_negative_probability, double test_period, double deadline);
const char* to_str(TestResult test_result);
TestResult test(Ctx ctx, int pid);
void log_event(Ctx ctx, EventType type, char *fmt, ...);
