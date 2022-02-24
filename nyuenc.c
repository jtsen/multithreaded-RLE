#include "nyuenc.h"

/*pthread reference: https://www.cs.cmu.edu/afs/cs/academic/class/15492-f07/www/pthreads.html*/

unsigned char *compress(char* block){
    /*initialize stack variables used to track and store intermediate compressed results*/
    char* input = block;
    unsigned char counts[BLOCK_SIZE];
    unsigned char compressed[BLOCK_SIZE];
    unsigned char previous; int tracker=0;
    /*initialize counts to 0*/
    for(int i=0; i<BLOCK_SIZE;++i){counts[i]=(unsigned char)0;};
    /*loop until BLOCK_SIZE reached or EOF*/
    int x=0;
    while(input[x]){
        if(x==BLOCK_SIZE){
            break;
        }
        if(x==0){//if this is the first byte read
            previous=input[x];
            ++counts[tracker];
            x++;
            continue;
        }
        if(input[x]==previous){//if current byte is the same as the previous
            ++counts[tracker];
            x++;
            continue;
        }
        if(input[x]!=previous){//if current byte is not the same as the previous
            compressed[tracker]=previous;
            previous = input[x];
            ++tracker;
            ++counts[tracker];
            x++;
            continue;
        }
    }
    //at the end, make sure that the last char and count is accounted for
    compressed[tracker]=previous;
    ++tracker;
    ++counts[tracker];

    /*store the compressed results into an array on the heap*/
    unsigned char *output = malloc ((size_t) tracker*2+1);
    for(int i=0; i<tracker;i++){
        output[i*2]=compressed[i];
        output[i*2+1]=counts[i];
    }
    return output;
}

void * start_working(){
    while(1){
        sem_wait(&jobs_count); //wait for available jobs
        sem_wait(&jobs_mutex); //down access to job queue
        sem_wait(&current_mutex); //down access to job index
        char * input_job = jobs[current_jobs_listing]; //get the job
        sem_post(&jobs_mutex); //release job queue
        int current_job_index = current_jobs_listing; //store the job index on the stack
        current_jobs_listing++; //increment the job index
        sem_post(&current_mutex); //release job index

        unsigned char * res;
        res = compress(input_job); //get the compressed output from our input

        sem_wait(&result_mutex); //down access to result list
        results[current_job_index]=res; //place the results at its correct position
        sem_post(&collect_count); //up a counting semaphore notifying the collection process that a result is available
        sem_post(&result_mutex); // release result list
    }
}

void free_results(unsigned char *res[]){
    /*iterate through the result list and free all heap allocated compressed results*/
    int res_iterator=0;
    while(res[res_iterator]){
        free(res[res_iterator]);
        res_iterator++;
    }
}

int main (int argc, char* argv[]){
    if (argc == 1)
    {
        //https://stackoverflow.com/questions/39002052/how-i-can-print-to-stderr-in-c
        fprintf(stderr, "Please provide a file\n");
        exit(0);
    }
    //https://stackoverflow.com/questions/22575940/getopt-not-included-implicit-declaration-of-function-getopt/22576057
    int opt = getopt(argc, argv, "j");
    if (opt==-1){//sequential solver
        /*initialize bookkeeping variables*/
        int file_descriptors[argc-1];
        char* file_contents[argc-1];
        int file_sizes[argc-1];
        struct stat buf;
        int total_size=0;
        int job_index=0;
        /*read in all the files provided*/
        for(int i=0;i<argc-1;++i)
        {
            file_descriptors[i] = open(argv[i+1], O_RDONLY);
            if(file_descriptors[i]==-1){
                fprintf(stderr, "Error: Issue with opening provided file");
                exit(1);
            }
            fstat(file_descriptors[i],&buf);
            file_contents[i] = (char *) mmap(NULL, buf.st_size, PROT_READ, MAP_PRIVATE, file_descriptors[i], 0);
            if(file_contents[i]== MAP_FAILED){
                fprintf(stderr, "Error: Issue with virtual address mapping creation");
                exit(1);
            }
            file_sizes[i]=buf.st_size;
            total_size=total_size+buf.st_size;
        }
        /*split each file into 4KB blocks and store them in a job list*/
        for(int i=0;i<argc-1;++i){
            int current_byte_location = 0;
            do{
                jobs[job_index]=&file_contents[i][current_byte_location*BLOCK_SIZE];
                file_sizes[i]-=BLOCK_SIZE;
                current_byte_location++;
                job_index++;
            } while (file_sizes[i]>0);
        }
        /*compress each block sequentially and output the results*/
        unsigned char* curr_result;
        unsigned char * prev = NULL;
        for(int i=0;i<job_index;i++){
            curr_result=compress(jobs[i]);
            /*if this is not the first block*/
            if(prev){
                /*if the leadings char is the same as the ending char of previous job*/
                if(prev[0]==curr_result[0]){
                    curr_result[1] += prev[1];
                }else{
                    printf("%c%c",prev[0],prev[1]);
                }
                prev=NULL;
            }
            /*print the result to stdout while withholding the last character*/
            int result_iterator=0;
            while(curr_result[result_iterator]){
                /*last character and count*/
                if(!curr_result[result_iterator+2]){
                    /*previous points to second to last location of output*/
                    prev=&curr_result[result_iterator];
                    break;
                }
                else{
                    /*print compressed output until last character and count*/
                    printf("%c%c",curr_result[result_iterator],curr_result[result_iterator+1]);
                    result_iterator+=2;
                }
            }
        }
        /*print the last char and count when at the last block*/
        printf("%c%c",prev[0],prev[1]);
    }else{//parallel solver
        /*Initialize and declare bookkeeping information*/
        int num_of_threads = atoi(argv[optind]);
        int read_start_index = optind+1;
        int file_descriptors[argc-3]; struct stat buf;
        char* file_contents[argc-3];
        int total_size = 0;
        int job_index=0;

        /*Initialize semaphores*/
        if(sem_init(&jobs_mutex,0,1)==-1){
            /*handle error here*/
            fprintf(stderr, "Error: Job queue mutex initialization failed");
            exit(1);
        }
        if(sem_init(&current_mutex,0,1)==-1){
            /*handle error here*/
            fprintf(stderr, "Error: Job queue mutex initialization failed");
            exit(1);
        }
        if(sem_init(&jobs_count,0,0)==-1){
            /*handle error here*/
            fprintf(stderr, "Error: Job queue mutex initialization failed");
            exit(1);
        }
        if(sem_init(&result_mutex,0,1)==-1){
            /*handle error here*/
            fprintf(stderr, "Error: Job queue mutex initialization failed");
            exit(1);
        }
        if(sem_init(&collect_count,0,0)==-1){
            /*handle error here*/
            fprintf(stderr, "Error: Job queue mutex initialization failed");
            exit(1);
        }

        /*create thread pool*/
        pthread_t thread_id[num_of_threads];
        for(int i=0; i<num_of_threads;++i){
            if(pthread_create(&thread_id[i], NULL, start_working, NULL)){
                fprintf(stderr, "Error: Thread pool creation failed.");
                exit(1);
            }
        }

        /*mmap all the files provided, split into 4kb chunks, then submit to job queue*/
        int index=0;
        /*reference:https://stackoverflow.com/questions/20460670/reading-a-file-to-string-with-mmap/20460969*/
        for(int i=read_start_index; i<argc;++i){
            file_descriptors[index]=open(argv[i], O_RDONLY);
            if(file_descriptors[index]==-1){
                fprintf(stderr, "Error: Issue with opening provided file");
                exit(1);
            }
            fstat(file_descriptors[index], &buf);
            file_contents[index]=(char*) mmap(NULL,buf.st_size,PROT_READ,MAP_PRIVATE, file_descriptors[index],0);
            if(file_contents[index]== MAP_FAILED){
                fprintf(stderr, "Error: Issue with virtual address mapping creation");
                exit(1);
            }
            int location = 0; int fsize=(int)buf.st_size;
            /*submitting files in blocks to the job queue*/
            do{
                sem_wait(&jobs_mutex);
                jobs[job_index]=&file_contents[index][location*BLOCK_SIZE];
                job_index++;
                sem_post(&jobs_count);
                sem_post(&jobs_mutex);
                location++;
                fsize-=BLOCK_SIZE;
            } while (fsize>0);
            /*iterate to next file*/
            index++;
            total_size+=buf.st_size;
        }
        /*start collection process*/
        unsigned char * temp = NULL; unsigned char prev[2] = {0};
        int collected = 0;
        /*while there exist a block that needs to be collected*/
        while(collected<job_index){
            /*check if there is completed jobs to be collected*/
            sem_wait(&collect_count);
        
            /*get access to result array*/
            sem_wait(&result_mutex);

            /*if the target block to collect does not exist yet, release lock and try again*/
            if(!results[collected]){
                sem_post(&result_mutex);
                continue;
            }else{ /*if the target block to be collected is completed, output the block and everyone block thereafter if completed*/
                /*while the target block is completed*/
                while(results[collected]){
                    /*get the location of the completed job that is on the heap*/
                    temp = &results[collected][0];
                    /*check if previous job has already completed*/
                    if(prev[0]){
                        /*if the leadings char is the same as the ending char of previous job*/
                        if(prev[0]==temp[0]){
                            temp[1] += prev[1];
                        }else{
                            printf("%c%c",prev[0],prev[1]); // output the previous last char and count otherwise
                        }
                        prev[0]=(unsigned char) 0;
                    }
                    /*print the result to stdout while withholding the last character*/
                    int result_iterator=0;
                    while(temp[result_iterator]){
                        /*last character and count*/
                        if(!temp[result_iterator+2] && collected!=job_index-1){
                            /*previous points to second to last location of output*/
                            prev[0] = temp[result_iterator];
                            prev[1] = temp[result_iterator+1];
                            break;
                        }
                        else{
                            /*print compressed output until last character and count*/
                            printf("%c%c",temp[result_iterator],temp[result_iterator+1]);
                            result_iterator+=2;
                        }
                    }
                    /*Iterate the next block to be collected*/
                    collected++;
                }
                sem_post(&result_mutex); //release result list lock as we already have the address
            }
        }
    }
    free_results(results);
    fflush(stdout);
    return 0;
}