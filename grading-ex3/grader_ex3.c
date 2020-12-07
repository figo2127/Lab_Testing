/**
 * This runner tests ex3 by creating a shared heap,
 * allocating N equal-sized objects in it in parallel,
 * freeing N/2 + allocating N/2 equal-sized objects in parallel (x3),
 * then freeing N objects in parallel.
 * It does not use dummy memory to prevent equal addresses,
 * or check if the memory is unmapped,
 * so as not to double-penalise students
 * (who will already be penalised in ex2).
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

#define SHMHEAP_CONNECT 0
#define SHMHEAP_DISCONNECT 1
#define SHMHEAP_READ 2
#define SHMHEAP_ALLOC 3
#define SHMHEAP_FREE 4

#define OBJECT_SIZE 32

typedef struct {
    int fd[2];
} pipe_pair;

typedef struct {
    pipe_pair in, out;
} bidir_pipe;

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

static int child_proc(const bidir_pipe *bp, const char *mem_name) {
    shmheap_memory_handle mem;
    void *base;
    int input_fd = bp->in.fd[0];
    int output_fd = bp->out.fd[1];
    int res;
    int type;
    while (read(input_fd, &type, sizeof(type)) == sizeof(type)) {
        switch (type) {
            case SHMHEAP_CONNECT: {
                mem = shmheap_connect(mem_name);
                base = shmheap_underlying(mem);
                char dummy = 0;
                write(output_fd, &dummy, sizeof(dummy));
                break;
            }
            case SHMHEAP_DISCONNECT: {
                shmheap_disconnect(mem);
                char dummy = 0;
                write(output_fd, &dummy, sizeof(dummy));
                break;
            }
            case SHMHEAP_ALLOC: {
                void *data = shmheap_alloc(mem, OBJECT_SIZE);
                shmheap_object_handle hdl = shmheap_ptr_to_handle(mem, data);
                write(output_fd, &hdl, sizeof(hdl));
                break;
            }
            case SHMHEAP_FREE: {
                shmheap_object_handle hdl;
                res = read(input_fd, &hdl, sizeof(hdl));
                assert(res == sizeof(hdl));
                void *data = shmheap_handle_to_ptr(mem, hdl);
                shmheap_free(mem, data);
                char dummy = 0;
                write(output_fd, &dummy, sizeof(dummy));
                break;
            }
            default: {
                return 100; // grader error
            }
        }
    }

    return 0;
}

static ssize_t checked_write(int fd, const void *buf, size_t count, int *errcode) {
    if (write(fd, buf, count) == -1 && errno != EPIPE) {
        printf("Write failed\n");
        if (*errcode == 0) *errcode = 98;
    }
}

static int voidptr_cmp(const void *a, const void *b) {
    return *(void **)a - *(void **)b;
}

// Requires arr has size n, and r <= n
// After returning, the first r elements of arr will be a permutation of {0,...,n-1}
// The remaining n-r elements will be arbitrary
static void generate_permutation(int *arr, int r, int n) {
    for (int i=0; i!=n; ++i) {
        arr[i] = i;
    }
    for (int i=0; i!=r; ++i) {
        int idx = rand() % (n-i);
        int tmp = arr[i + idx];
        arr[i + idx] = arr[i];
        arr[i] = tmp;
    }
}

int main (int argc, char *argv[]) {
    if (argc < 4) {
        printf("usage: %s first_space subsequent_space N [seed]\n", argv[0]);
        return 1; // run failed
    }

    // silence sigpipe (might happen if the child died)
    struct sigaction tmp_sa = {SIG_IGN};
    sigaction(SIGPIPE, &tmp_sa, NULL);

    size_t first_space, mid_space;
    sscanf(argv[1], "%zu", &first_space);
    sscanf(argv[2], "%zu", &mid_space);
    const int num_proc = atoi(argv[3]);

    if (argc > 2) {
        const int seed = atoi(argv[4]);
        srand(seed ? seed : time(NULL));
    }

    assert(num_proc > 0 && num_proc % 2 == 0);

    int i = 0;

    // find a name for our shm heap
    const char *const mem_name = find_good_shm_name(&i);

    // create pipes
    bidir_pipe *const pp = malloc(sizeof(bidir_pipe) * num_proc);

    const size_t mem_size = (OBJECT_SIZE + 16) * num_proc * 2 /* ordering possibility */ + 80 + 1024 /* spare space */;

    // spawn children
    for (int i=0; i!=num_proc; ++i) {
        pipe2(pp[i].in.fd, O_DIRECT);
        pipe2(pp[i].out.fd, O_DIRECT);
        int res = fork();
        assert(res != -1);
        if (res == 0) {
            for (int j=0; j!=i; ++j) {
                close(pp[j].in.fd[1]);
                close(pp[j].out.fd[0]);
            }
            close(pp[i].in.fd[1]);
            close(pp[i].out.fd[0]);
            const bidir_pipe curr_pp = pp[i];
            free(pp);
            return child_proc(&curr_pp, mem_name);
        }
        close(pp[i].in.fd[0]);
        close(pp[i].out.fd[1]);
    }

    // init the shm heap
    shmheap_memory_handle mem = shmheap_create(mem_name, mem_size);

    void *const base = shmheap_underlying(mem);

    int errcode = 0, readerr = 0;

    void **objects = malloc(sizeof(void*) * num_proc);

    // send SHMHEAP_CONNECT command
    for (int i=0; i!=num_proc; ++i) {
        int type = SHMHEAP_CONNECT;
        checked_write(pp[i].in.fd[1], &type, sizeof(type), &errcode);
    }
    for (int i=0; i!=num_proc; ++i) {
        char dummy;
        read(pp[i].out.fd[0], &dummy, sizeof(dummy));
    }

    // send SHMHEAP_ALLOC command
    for (int i=0; i!=num_proc; ++i) {
        int type = SHMHEAP_ALLOC;
        checked_write(pp[i].in.fd[1], &type, sizeof(type), &errcode);
    }
    for (int i=0; i!=num_proc; ++i) {
        shmheap_object_handle hdl;
        read(pp[i].out.fd[0], &hdl, sizeof(hdl));
        objects[i] = shmheap_handle_to_ptr(mem, hdl);
    }

    // check that the set of objects returned is as expected
    qsort(objects, num_proc, sizeof(void*), voidptr_cmp);
    size_t current_offset = first_space - mid_space;
    for (int i=0; i!=num_proc; ++i){
        if (objects[i] != base + (current_offset + mid_space)) {
            printf("First alloc was at unexpected location (child %d): got %p, expected %p\n", i, objects[i], base + (current_offset + mid_space));
            readerr = 1;
        }
        current_offset += OBJECT_SIZE + mid_space;
    }

    const int num_swap = num_proc / 2;

    for (int j=0; j!=3; ++j) {
        // pick out half the indices
        int *selected_swap_indices = malloc(sizeof(int) * num_proc);
        generate_permutation(selected_swap_indices, num_swap, num_proc);

        int *selected_processes = malloc(sizeof(int) * num_proc);
        generate_permutation(selected_processes, num_proc, num_proc);

        for (int i=0; i!=num_proc; ++i) {
            if (selected_processes[i] < num_swap) {
                int swapidx = selected_processes[i];
                int swapval = selected_swap_indices[swapidx];
                int type = SHMHEAP_FREE;
                checked_write(pp[i].in.fd[1], &type, sizeof(type), &errcode);
                shmheap_object_handle hdl = shmheap_ptr_to_handle(mem, objects[swapval]);
                checked_write(pp[i].in.fd[1], &hdl, sizeof(hdl), &errcode);
            }
            else {
                int swapidx = selected_processes[i] - num_swap;
                int swapval = selected_swap_indices[swapidx];
                int type = SHMHEAP_ALLOC;
                checked_write(pp[i].in.fd[1], &type, sizeof(type), &errcode);
            }
        }

        for (int i=0; i!=num_proc; ++i) {
            if (selected_processes[i] < num_swap) {
                int swapidx = selected_processes[i];
                int swapval = selected_swap_indices[swapidx];
                char dummy;
                read(pp[i].out.fd[0], &dummy, sizeof(dummy));
            }
            else {
                int swapidx = selected_processes[i] - num_swap;
                int swapval = selected_swap_indices[swapidx];
                shmheap_object_handle hdl;
                read(pp[i].out.fd[0], &hdl, sizeof(hdl));
                objects[swapval] = shmheap_handle_to_ptr(mem, hdl);
            }
        }

        free(selected_processes);
        free(selected_swap_indices);

        // check that the set of objects returned is as expected
        qsort(objects, num_proc, sizeof(void*), voidptr_cmp);
        for (int i=0; i!=num_proc; ++i){
            if (objects[i] < base + first_space || objects[i] >= base + first_space + 2 * num_proc * (OBJECT_SIZE + mid_space))  {
                printf("Mid (%d) alloc was at unexpected location (child %d): got %p, expected in range [%p, %p)\n", j, i, objects[i], base + first_space, base + first_space + 2 * num_proc * (OBJECT_SIZE + mid_space));
                readerr = 1;
            }
        }
    }

    // send SHMHEAP_FREE command
    for (int i=0; i!=num_proc; ++i) {
        int type = SHMHEAP_FREE;
        checked_write(pp[i].in.fd[1], &type, sizeof(type), &errcode);
        shmheap_object_handle hdl = shmheap_ptr_to_handle(mem, objects[i]);
        checked_write(pp[i].in.fd[1], &hdl, sizeof(hdl), &errcode);
    }
    for (int i=0; i!=num_proc; ++i) {
        char dummy;
        read(pp[i].out.fd[0], &dummy, sizeof(dummy));
    }

    void *obj = shmheap_alloc(mem, (OBJECT_SIZE + 16) * num_proc);
    if (base + first_space != obj)  {
        printf("Final alloc was at unexpected location: got %p, expected %p\n", obj, base + first_space);
        readerr = 1;
    }

    shmheap_free(mem, obj);

    // send SHMHEAP_DISCONNECT command
    for (int i=0; i!=num_proc; ++i) {
        int type = SHMHEAP_DISCONNECT;
        checked_write(pp[i].in.fd[1], &type, sizeof(type), &errcode);
    }
    for (int i=0; i!=num_proc; ++i) {
        char dummy;
        read(pp[i].out.fd[0], &dummy, sizeof(dummy));
    }

    free(objects);
    
    for (int i=0; i!=num_proc; ++i) {
        close(pp[i].in.fd[1]);
        close(pp[i].out.fd[0]);
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

    if (errcode == 0 && readerr != 0) return readerr;

    return errcode;
}
