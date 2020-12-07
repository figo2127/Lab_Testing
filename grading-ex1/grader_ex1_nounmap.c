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

typedef struct {
    int fd[2];
} pipe_pair;

static const char *shm_prefix = "/shmheap";

static const char *find_good_shm_name(int *i) {
    char *ret = malloc(20 * sizeof(char));
    memcpy(ret, shm_prefix, strlen(shm_prefix));
    while (true) {
        sprintf(ret + strlen(shm_prefix), "%d", (*i)++);
        int fd;
        if ((fd = shm_open(ret, O_RDWR, 0)) == -1) {
            if (errno == ENOENT) return ret;
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

static int randint(int min, int max) {
    return rand() % (max - min + 1) + min;
}

#define MEM_SIZE (1 << 16)

static int child_proc(int input_fd, size_t num_bytes, const char *mem_name, const char *dummy_name, long page_size) {
    // wait until the heap has been created
    {
        char buf[PIPE_BUF];
        int res = read(input_fd, buf, PIPE_BUF);
        assert(res == 1);
    }
    
    // allocate some shared mem just to block out the heap space
    int fd2 = shm_open(dummy_name, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    ftruncate(fd2, MEM_SIZE);
    void *mem2 = mmap(NULL, MEM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd2, 0);
    close(fd2);
    
    // connect to heap
    shmheap_memory_handle mem = shmheap_connect(mem_name);
    
    // retrieve the handle
    char *ptr;
    {
        char buf[PIPE_BUF];
        assert(read(input_fd, buf, PIPE_BUF) == sizeof(shmheap_object_handle));
        const shmheap_object_handle obj = *((const shmheap_object_handle *)buf);
        ptr = shmheap_handle_to_ptr(mem, obj);
    }
    
    // deallocate the shared mem
    munmap(mem2, MEM_SIZE);
    
    // check that the data is correct
    for (size_t i=0; i!=num_bytes; ++i){
        if (ptr[i] != (char)(rand() & 255)) {
            shmheap_disconnect(mem);
            return 1;
        }
    }
    
    shmheap_disconnect(mem);
    
    return 0;
}

int main (int argc, char *argv[]) {
    if (argc < 2) {
        printf("usage: %s num_receiver_processes [seed]\n", argv[0]);
        return 1; // run failed
    }
    
    // silence sigpipe (might happen if the child died)
    struct sigaction tmp_sa = {SIG_IGN};
    sigaction(SIGPIPE, &tmp_sa, NULL);

    const int num_proc = atoi(argv[1]);

    if (argc > 2) {
        const int seed = atoi(argv[2]);
        srand(seed ? seed : time(NULL));
    }
    
    const long page_size = sysconf(_SC_PAGESIZE);
    
    assert(num_proc > 0);
    
    int i = 0;
    
    // find a name for our shm heap
    const char *const mem_name = find_good_shm_name(&i);
    
    // find a name for our dummy shm heap
    const char *const dummy_name = find_good_shm_name(&i);
    
    // create pipes
    pipe_pair *const pp = malloc(sizeof(pipe_pair) * num_proc);
    
    // amount of bytes to allocate (done before forking so the PRNG will be in sync)
    const size_t num_bytes = randint(1, MEM_SIZE - 80);
    
    // spawn children
    for (int i=0; i!=num_proc; ++i) {
        pipe2(pp[i].fd, O_DIRECT);
        int res = fork();
        assert(res != -1);
        if (res == 0) {
            for (int j=0; j!=i; ++j) {
                close(pp[j].fd[1]);
            }
            close(pp[i].fd[1]);
            int input_fd = pp[i].fd[0];
            free(pp);
            return child_proc(input_fd, num_bytes, mem_name, dummy_name, page_size);
        }
        close(pp[i].fd[0]);
    }
    
    // init the shm heap
    shmheap_memory_handle mem = shmheap_create(mem_name, MEM_SIZE);
    
    int errcode = 0;

    // tell children that we have created the heap
    for (int i=0; i!=num_proc; ++i) {
        int dummy = 0;
        if (write(pp[i].fd[1], &dummy, 1) == -1 && errno != EPIPE) {
            printf("Write failed\n");
            if (errcode == 0) errcode = 98;
        }
    }
    
    // alloc some memory
    char *const ptr = shmheap_alloc(mem, num_bytes);
    
    // write random stuff to the memory
    for (size_t i=0; i!=num_bytes; ++i){
        ptr[i] = (char)(rand() & 255);
    }
    
    // convert the pointer to handle
    const shmheap_object_handle obj = shmheap_ptr_to_handle(mem, ptr);
    
    // send the object handle to children
    for (int i=0; i!=num_proc; ++i) {
        if (write(pp[i].fd[1], (const void *)(&obj), sizeof(shmheap_object_handle)) == -1 && errno != EPIPE) {
            printf("Write failed\n");
            if (errcode == 0) errcode = 98;
        }
    }

    // free pipes
    free(pp);

    // wait for children
    for (int i=0; i!=num_proc; ++i) {
        int status;
        int pid;
        if ((pid = wait(&status)) == -1) {
            printf("Child mysteriously disappeared\n");
            if (errcode == 0) errcode = 4;
        }
        else if (WIFSIGNALED(status)) {
            printf("Child [pid = %d] terminated abruptly!\n", pid);
            if (errcode == 0) errcode = 128 + WTERMSIG(status);
        }
        else if (!WIFEXITED(status)) {
            printf("Child [pid = %d] terminated abruptly!\n", pid);
            if (errcode == 0) errcode = 8;
        }
        else if(WEXITSTATUS(status) == 3) {
            printf("Shared memory was not unmapped by shmheap_disconnect()\n");
            if (errcode == 0) errcode = 3;
        }
        else if(WEXITSTATUS(status) == 1) {
            printf("Child [pid = %d] read incorrect data!\n", pid);
            if (errcode == 0) errcode = 1;
        }
        else if(WEXITSTATUS(status) != 0) {
            printf("Child [pid = %d] returned weird error code!\n", pid);
            if (errcode == 0) errcode = 97;
        }
        else {
            printf("Child [pid = %d] received data successfully\n", pid);
        }
    }
    
    // destroy shm
    shmheap_destroy(mem_name, mem);
    
    shm_unlink(dummy_name);

    return errcode;
}
