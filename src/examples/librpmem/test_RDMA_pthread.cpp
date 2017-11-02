/*
 * Copyright 2016, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * manpage.c -- example for librpmem manpage
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

/* For multi-thread clients */
#include <pthread.h>

#include <librpmem.h>

/* For getting time */
#include <sys/time.h>
#include <time.h>

#define POOL_SIZE	(1024 * 1024)
//#define NLANES		4

struct thread_data{
    RPMEMpool *rpp;
    void* readbuf;
    int total_thread;
    int thread_id;
    int transfer_size;
    bool isWrite;
};

unsigned long timestamp()
{
    struct timeval tv;
    if(gettimeofday(&tv, NULL) == -1)
    {
        fprintf(stderr, "Error getting timestamp");
        return 0UL;
    }
    return tv.tv_sec*1000000 + tv.tv_usec;
}

void *issue_request(void *threadarg)
{
    int ret;
    struct thread_data *t_data;
    t_data = (struct thread_data *)threadarg;


    /* write operation */
    if(t_data->isWrite)
    {
        /* make local data persistent on remote node */
        ret = rpmem_persist_test(t_data->rpp, 
                t_data->thread_id*(POOL_SIZE/t_data->total_thread), POOL_SIZE/t_data->total_thread,
                t_data->transfer_size, t_data->thread_id);
        if (ret) {
            fprintf(stderr, "rpmem_persist: %s\n", rpmem_errormsg());
        }
    }
    /* read operation */
    else
    {
        ret = rpmem_read_test(t_data->rpp, t_data->readbuf, 
                t_data->thread_id*(POOL_SIZE/t_data->total_thread), POOL_SIZE/t_data->total_thread,
                t_data->transfer_size, t_data->thread_id);
        if (ret) {
            fprintf(stderr, "rpmem_read: %s\n", rpmem_errormsg());
        }

    }

    return NULL;   
}

int
main(int argc, char *argv[])
{
	int ret;
	//unsigned nlanes = NLANES;
    

	if (argc < 6) {
		fprintf(stderr, "usage: %s [create|open|remove] <target> <pool_set> <client_numthread> <transfer_size>\n", argv[0]);
		return 1;
	}

    /* arguement intialization */
    char *op = argv[1];
    char *target = argv[2];
    char *pool_set = argv[3];
    char *numthread_string = argv[4];
    int num_threads = atoi(numthread_string);
    char *transfer_string = argv[5];
    int transfer_size = atoi(transfer_string);
    unsigned nlanes = num_threads;

    RPMEMpool *rpp;

    /* For memory alignment for pool address */
    void* pool;

	size_t align = (size_t)sysconf(_SC_PAGESIZE);
	errno = posix_memalign(&pool, align, POOL_SIZE);
	if (errno) {
		perror("posix_memalign");
		return -1;
	}

    /* For read local buffer for client*/
    void* readbuf;
	
    errno = posix_memalign(&readbuf, align, POOL_SIZE);
	if (errno) {
		perror("posix_memalign");
		return -1;
	}

	/* fill pool_attributes */
	struct rpmem_pool_attr pool_attr;
	memset(&pool_attr, 0, sizeof(pool_attr));

    if(strcmp(op, "create") == 0)
    {
        /* create a remote pool */
        rpp = rpmem_create(target, pool_set,
                pool, POOL_SIZE, &nlanes, &pool_attr);
        if (!rpp) {
            fprintf(stderr, "rpmem_create: %s\n", rpmem_errormsg());
            return 1;
        }
    }
    else if(strcmp(op, "open") == 0)
    {
        /* open a remote pool */
        rpp = rpmem_open(target, pool_set,
                pool, POOL_SIZE, &nlanes, &pool_attr);
        if (!rpp) {
            fprintf(stderr, "rpmem_open: %s\n", rpmem_errormsg());
        }

    }
    else
    {
        fprintf(stderr, "check the argument [create|open]\n");
        return 1;
    }

	/* store data on local pool */
	memset(pool, 0, POOL_SIZE);
	memset(readbuf, 0, POOL_SIZE);

    /* timer variables */
    unsigned long T1, T2;
    unsigned long R1, R2;

    pthread_t* threads;
    struct thread_data* td;
    void* status;

    threads = (pthread_t*)malloc(sizeof(pthread_t)*num_threads);
    td = (struct thread_data*)malloc(sizeof(struct thread_data)*num_threads);

    for(int i = 0 ; i < num_threads; i++)
    {
        td[i].thread_id = i;
        td[i].transfer_size = transfer_size;
        td[i].rpp = rpp;
        td[i].readbuf = NULL;
        td[i].total_thread = num_threads;
        td[i].isWrite = true;
    }

    /* write performance test */
    T1 = timestamp();

    for(int i = 0 ; i < num_threads; i++)
    {
        pthread_create(&threads[i], NULL, issue_request, (void *)&td[i]);
    }

    for(int i = 0 ; i < num_threads;i++)
    {
        pthread_join(threads[i], (void**)&status);
    }
    
    T2 = timestamp();


    /* read performance test */
    void* readbuf_idx;

    for(int i = 0 ; i < num_threads; i++)
    {
        readbuf_idx = (char*)readbuf + POOL_SIZE/num_threads;
        readbuf_idx = (void*)readbuf_idx;
        td[i].thread_id = i;
        td[i].transfer_size = transfer_size;
        td[i].rpp = rpp;
        td[i].readbuf = readbuf_idx;
        td[i].total_thread = num_threads;
        td[i].isWrite = false;
    }
        
    R1 = timestamp();

    for(int i = 0 ; i < num_threads; i++)
    {
        pthread_create(&threads[i], NULL, issue_request, (void *)&td[i]);
    }

    for(int i = 0 ; i < num_threads;i++)
    {
        pthread_join(threads[i], (void**)&status);
    }
    R2 = timestamp();

    unsigned long total_write_time = T2-T1;
    unsigned long total_read_time = R2-R1;
    /* print a result */
    fprintf(stdout, "Transfer size : %d, Pool size: %d, Number of RDMA: %d\n", 
            transfer_size, POOL_SIZE, POOL_SIZE/transfer_size);

    /* read performance */
    //fprintf(stdout, "Time for RDMA is %ldus\n", total_read_time);
    fprintf(stdout, "Read_Throughput(MB/s): %lf\n", double(POOL_SIZE)/double(total_read_time));
    fprintf(stdout, "Read_Operation_Throughput(MOPS): %lf\n", (double)(POOL_SIZE/transfer_size)/(double)total_read_time);

    /* write performance */
    //fprintf(stdout, "Time for RDMA is %ldus\n", total_write_time);
    fprintf(stdout, "Write_Throughput(MB/s): %lf\n", double(POOL_SIZE)/double(total_write_time));
    fprintf(stdout, "Write_Operation_Throughput(MOPS): %lf\n", (double)(POOL_SIZE/transfer_size)/(double)total_write_time);
	
    /* close the remote pool */
	ret = rpmem_close(rpp);
	if (ret) {
		fprintf(stderr, "rpmem_close: %s\n", rpmem_errormsg());
		return 1;
	}

    pthread_exit(NULL);

    free(threads);
    free(td);

	return 0;
}
