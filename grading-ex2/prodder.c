/**
 * This runner tests ex1 by creating a shared heap,
 * allocating one object in it, and sending it to
 * `num_receiver_processes` other processes via a pipe.
 */

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "shmheap.h"

static char shm_name_store[20]="/shmheap";

static const char *find_good_shm_name(int *i) {
    for (; true; ++*i){
        sprintf(shm_name_store+8, "%d", *i);
        //itoa(i, shm_name_store+8, 10);
        int fd;
        if ((fd = shm_open(shm_name_store, O_RDWR, 0)) == -1) {
            if (errno == ENOENT) return shm_name_store;
            else if (errno == EINVAL || errno == EMFILE || errno == ENAMETOOLONG || errno == ENFILE) {
                printf("Unexpected error\n");
                exit(EXIT_FAILURE);
            }
        }
        else {
            close(fd);
        }
    }
}

int main (int argc, char** argv) {
    assert(argc == 2);
    
    FILE *out = fopen(argv[1], "w");
    
    int i = 0;
    
    // find a name for our shm heap
    const char *const mem_name = find_good_shm_name(&i);
    
    // init the shm heap
    shmheap_memory_handle mem = shmheap_create(mem_name, 4096);
    
    // get base
    void *base = shmheap_underlying(mem);
    
    // allocate first object
    void *first_obj = shmheap_alloc(mem, 32);
    
    // allocate first object
    void *second_obj = shmheap_alloc(mem, 32);
    
    fprintf(out, "%d %d\n", first_obj - base, second_obj - (first_obj + 32));
    
    // destroy shm
    shmheap_destroy(mem_name, mem);

    return EXIT_SUCCESS;
}
