
#include <stdbool.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#include <kos/thread.h>
#include <dc/fs_dcload.h>

static char OUTPUT_FILENAME[128];
static kthread_t* THREAD;
static volatile bool PROFILER_RUNNING = false;
static volatile bool PROFILER_RECORDING = false;

#define BUCKET_SIZE 10000

/* Simple hash table of samples. An array of Samples
 * but, each sample in that array can be the head of
 * a linked list of other samples */
typedef struct Arc {
    uint32_t pc;
    uint32_t pr; // Caller return address
    uint32_t count;
    struct Arc* next;
} Arc;

static Arc ARCS[BUCKET_SIZE];

/* Hashing function for two uint32_ts */
#define HASH_PAIR(x, y) ((x * 0x1f1f1f1f) ^ y)

#define BUFFER_SIZE (1024 * 1024 * 8)  // 8K buffer

const static size_t MAX_ARC_COUNT = BUFFER_SIZE / sizeof(Arc);
static size_t ARC_COUNT = 0;

static bool WRITE_TO_STDOUT = false;

static bool write_samples(const char* path);
static bool write_samples_to_stdout();
static void clear_samples();

static Arc* new_arc(uint32_t PC, uint32_t PR) {
    Arc* s = (Arc*) malloc(sizeof(Arc));
    s->count = 1;
    s->pc = PC;
    s->pr = PR;
    s->next = NULL;

    ++ARC_COUNT;

    return s;
}

static void record_thread(uint32_t PC, uint32_t PR) {
    uint32_t bucket = HASH_PAIR(PC, PR) % BUCKET_SIZE;

    Arc* s = &ARCS[bucket];

    if(s->pc) {
        /* Initialized sample in this bucket,
         * does it match though? */
        while(s->pc != PC || s->pr != PR) {
            if(s->next) {
                s = s->next;
            } else {
                s->next = new_arc(PC, PR);
                return; // We're done
            }
        }

        ++s->count;
    } else {
        /* Initialize this sample */
        s->count = 1;
        s->pc = PC;
        s->pr = PR;
        s->next = NULL;
        ++ARC_COUNT;
    }
}

static int thd_each_cb(kthread_t* thd, void* data) {
    (void) data;


    /* Only record the main thread (for now) */
    if(strcmp(thd->label, "[kernel]") != 0) {
        return 0;
    }

    /* The idea is that if this code right here is running in the profiling
     * thread, then all the PCs from the other threads are
     * current. Obviouly thought between iterations the
     * PC will change so it's not like this is a true snapshot
     * in time across threads */
    uint32_t PC = thd->context.pc;
    uint32_t PR = thd->context.pr;
    record_thread(PC, PR);
    return 0;
}

static void record_samples() {
    /* Go through all the active threads and increase
     * the sample count for the PC for each of them */

    size_t initial = ARC_COUNT;

    /* Note: This is a function added to kallistios-nitro that's
     * not yet available upstream */
    thd_each(&thd_each_cb, NULL);

    if(ARC_COUNT >= MAX_ARC_COUNT) {
        /* TIME TO FLUSH! */
        if(!write_samples(OUTPUT_FILENAME)) {
            fprintf(stderr, "Error writing samples\n");
        }
    }

    /* We log when the number of PCs recorded hits a certain increment */
    if((initial != ARC_COUNT) && ((ARC_COUNT % 1000) == 0)) {
        printf("-- %d arcs recorded...\n", ARC_COUNT);
    }
}

/* Declared in KOS in fs_dcload.c */
int fs_dcload_detected();
extern int dcload_type;


#define GMON_COOKIE "gmon"
#define GMON_VERSION 1

typedef struct {
    char cookie[4];  // 'g','m','o','n'
    int32_t version; // 1
    char spare[3 * 4]; // Padding
} GmonHeader;

typedef struct {
    unsigned char tag; // GMON_TAG_TIME_HIST = 0, GMON_TAG_CG_ARC = 1, GMON_TAG_BB_COUNT = 2
    size_t ncounts; // Number of address/count pairs in this sequence
} GmonBBHeader;

typedef struct {
    uint32_t from_pc;	/* address within caller's body */
    uint32_t self_pc;	/* address within callee's body */
    uint32_t count;			/* number of arc traversals */
} GmonArc;

static bool init_sample_file(const char* path) {
    printf("Detecting dcload... ");

    if(!fs_dcload_detected() && dcload_type != DCLOAD_TYPE_NONE) {
        printf("[Not Found]\n");
        WRITE_TO_STDOUT = true;
        return false;
    } else {
        printf("[Found]\n");
    }

    FILE* out = fopen(path, "w");
    if(!out) {
        WRITE_TO_STDOUT = true;
        return false;
    }

    /* Write the GMON header */

    GmonHeader header;
    memcpy(&header.cookie[0], GMON_COOKIE, sizeof(header.cookie));
    header.version = 1;
    memset(header.spare, '\0', sizeof(header.spare));

    fwrite(&header, sizeof(header), 1, out);

    fclose(out);
    return true;
}

static bool write_samples(const char* path) {
    /* Write samples to the given path as a CSV file
     * for processing */

    if(WRITE_TO_STDOUT) {
        write_samples_to_stdout();
        return true;
    }

    FILE* out = fopen(path, "r+");  /* Append, as init_sample_file would have created the file */
    if(!out) {
        fprintf(stderr, "-- Error writing samples to output file\n");
        return false;
    }

    // Seek to the end of the file
    fseek(out, 0, SEEK_END);

    printf("-- Writing %d arcs\n", ARC_COUNT);

    size_t tag = 1;

#ifndef NDEBUG
    size_t written = 0;
#endif


    Arc* root = ARCS;
    for(int i = 0; i < BUCKET_SIZE; ++i) {        
        if(root->pc) {
            GmonArc arc;
            arc.from_pc = root->pr;
            arc.self_pc = root->pc;
            arc.count = root->count;

            /* Write the root sample if it has a program counter */
            fwrite(&tag, sizeof(size_t), 1, out);
            fwrite(&arc, sizeof(GmonArc), 1, out);

#ifndef NDEBUG
            ++written;
#endif

            /* If there's a next pointer, traverse the list */
            Arc* s = root->next;
            while(s) {
                arc.from_pc = s->pr;
                arc.self_pc = s->pc;
                arc.count = s->count;

                /* Write the root sample if it has a program counter */
                fwrite(&tag, sizeof(size_t), 1, out);
                fwrite(&arc, sizeof(GmonArc), 1, out);

#ifndef NDEBUG
                ++written;
#endif

                s = s->next;
            }
        }

        root++;
    }

    printf("\n");

    fclose(out);

    /* We should have written all the recorded samples */
    assert(written == ARC_COUNT);

    clear_samples();

    return true;
}

static bool write_samples_to_stdout() {
    /* Write samples to stdout as a CSV file
     * for processing */

    printf("--------------\n");
    printf("\"PC\", \"PR\", \"COUNT\"\n");

    Arc* root = ARCS;
    for(int i = 0; i < BUCKET_SIZE; ++i) {
        Arc* s = root;
        while(s->next) {
            printf("\"%x\", \"%x\", \"%d\"\n", (unsigned int) s->pc, (unsigned int) s->pr, (unsigned int) s->count);
            s = s->next;
        }

        root++;
    }

    printf("--------------\n");

    return true;
}


static void* run(void* args) {
    printf("-- Entered profiler thread!\n");

    while(PROFILER_RUNNING){
        if(PROFILER_RECORDING) {
            record_samples();
            usleep(10);
        }
    }

    printf("-- Profiler thread finished!\n");

    return NULL;
}

void profiler_init(const char* output) {
    /* Store the filename */
    strncpy(OUTPUT_FILENAME, output, sizeof(OUTPUT_FILENAME));

    /* Initialize the file */
    printf("Creating samples file...\n");
    if(!init_sample_file(OUTPUT_FILENAME)) {
        printf("Read-only filesytem. Writing samples to stdout\n");
    }

    printf("Creating profiler thread...\n");
    // Initialize the samples to zero
    memset(ARCS, 0, sizeof(ARCS));

    PROFILER_RUNNING = true;
    THREAD = thd_create(0, run, NULL);

    printf("Thread started.\n");
}

void profiler_start() {
    assert(PROFILER_RUNNING);

    if(PROFILER_RECORDING) {
        return;
    }

    PROFILER_RECORDING = true;
    printf("Starting profiling...\n");
}

static void clear_samples() {
    /* Free the samples we've collected to start again */
    Arc* root = ARCS;
    for(int i = 0; i < BUCKET_SIZE; ++i) {
        Arc* s = root;
        Arc* next = s->next;

        // While we have a next pointer
        while(next) {
            s = next; // Point S at it
            next = s->next; // Store the new next pointer
            free(s); // Free S
        }

        // We've wiped the chain so we can now clear the root
        // which is statically allocated
        root->next = NULL;
        root++;
    }

    // Wipe the lot
    memset(ARCS, 0, sizeof(ARCS));
    ARC_COUNT = 0;
}

bool profiler_stop() {
    if(!PROFILER_RECORDING) {
        return false;
    }

    printf("Stopping profiling...\n");

    PROFILER_RECORDING = false;
    if(!write_samples(OUTPUT_FILENAME)) {
        printf("ERROR WRITING SAMPLES (RO filesystem?)! Outputting to stdout\n");
        return false;
    }


    return true;
}

void profiler_clean_up() {
    profiler_stop(); // Make sure everything is stopped

    PROFILER_RUNNING = false;
    thd_join(THREAD, NULL);
}
