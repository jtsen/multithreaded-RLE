#ifndef _NYUENC_H_
#define _NYUENC_H_

/*include required libraries*/
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/mman.h>

/*4KB block size, max 100 files, max 1GB length for files*/
#define BLOCK_SIZE (4096)

/*declare semaphores*/
/*counting semaphores -> jobs_count, collect_count*/
/*binary semaphores -> jobs_mutex, current_mutex, result_mutex*/
sem_t jobs_mutex, current_mutex, jobs_count, collect_count, result_mutex;

/*shared resources: Job queue, Job_to_do_index (current_jobs_listing), Result list*/
char* jobs[1000000] = {NULL};
int current_jobs_listing=0;
unsigned char * results[1000000] = {NULL};

/*function declarations*/
unsigned char *compress(char* block);
void * start_working();
void free_results(unsigned char * res[]);

#endif