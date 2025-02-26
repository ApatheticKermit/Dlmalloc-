#include <unistd.h> 
#include <stdint.h>
#include <stdlib.h>

#include <stdio.h>
#include <time.h>
#include <signal.h>

struct chunk { // 
    size_t header; // put size and inuse bit
    struct chunk* next_ptr; // ptr to next chunk
    struct chunk* previous_ptr; // pointer to prev chunk
};

typedef struct chunk* chunkpointer;

struct fragdata{
    size_t num_free_blocks;
    size_t total_free;
    size_t total_alloc;
};


extern int dlplus_init (void);
extern void *dlplus_malloc (size_t size);
extern void dlplus_free (void *ptr);
extern void *dlplus_realloc(void *ptr, size_t size);
extern void split(chunkpointer ptr, size_t size, size_t remainder_size);
extern void coalesce();
extern void *search_free_list(int bin, size_t size);
extern void remove_from_free_list(int bin, void *ptr);
extern void LIFO_bin_insert(int bin, chunkpointer ptr);
extern int simulate_right_coalesce(void *ptr, size_t diff);

extern void check_faulty_coalesce();
extern void check_traverse();
extern void check_freelist_info();
extern chunkpointer return_endheap();
extern struct fragdata fragmentation_data();
