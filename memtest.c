#include <unistd.h> 
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <time.h>
#include <signal.h>
#include <stdbool.h>
#include <locale.h> //DEBUG for comma


#include "dlplus_malloc.h"
#include "dlmalloc.h"

#define ITERATIONS 10000

#define RAND_RANGE(a, b) (rand() % (b + 1 - a) + a)
#define RANDOM_SIZE() ((rand() & 1) ? RAND_RANGE(0, 511) : RAND_RANGE(512, 1000000))

#define USE_DLPLUS_MALLOC 0 // 0 to test standard functions 1 to test custom

#if USE_DLPLUS_MALLOC // Unimplemented functions commented out
    size_t max_heap_size = 0;
    #define malloc dlplus_malloc
    #define free dlplus_free
    #define realloc dlplus_realloc
#else
    #define malloc dlmalloc
    #define free dlfree
    #define realloc dlrealloc
#endif

// GLOBALS

long long int total_alloc;
long long int total_free;
long long int max_alloc;

char *pointers[ITERATIONS]; // array of pointers to mem blocks
int block_sizes[ITERATIONS]; // array of alloc block_sizes
char chars[ITERATIONS]; // array of chars

void populate_array (int index, size_t size){
    block_sizes[index] = size;
    chars[index] = (char)RAND_RANGE(0, 256);
    for (size_t j = 0; j < size; ++j) {
        pointers[index][j] = chars[index];
    }
}

void clear_array (int index){
    pointers[index] = NULL;
    block_sizes[index] = 0;
    chars[index] = '\0';
}


// DATA COLLECTION FUNCS
void count_allocs(long long int size){
    total_alloc += size; // increase total allocation by size
    if ((total_free - size) >= 0){ // if there is enough free space on the heap
        total_free -= size; // take heap space from free space
    } else { // if not taking from free must be taking from system
        max_alloc += size; // therefore increase peak heap usage
    }
}

void count_frees(int index, long long int remainder){
    if(remainder != 0){
        total_free+=remainder;
        total_alloc-=remainder;
        
    } else {
        int size = block_sizes[index];
        total_free+=size;

        total_alloc -= size;

    }
}


void count_reallocs(int index, long long int size){
    long long int remainder = 0;
    if( size < block_sizes[index]){
        remainder = block_sizes[index] - size;
        count_frees(index,remainder);
    } else if (size > block_sizes[index]){
        remainder = size - block_sizes[index];
        count_allocs(remainder);
    }
}

void display_requestinfo(int action_performed, int index, long long int size, FILE* fpt){

    switch (action_performed){
        case 1: // malloc
            count_allocs(size);
            break;
        case 2: // free
            count_frees(index, size);
            break;
        case 3: // realloc
            count_reallocs(index, size);
            break;
    }

    fprintf(fpt,"%lld,", total_alloc); // total allocated
    fprintf(fpt,"%lld,", total_free); // total free
    fprintf(fpt,"%lld,", max_alloc); // peak_heap_size

}

void display_mallinfo(FILE* fpt){
    #if USE_DLPLUS_MALLOC
        struct fragdata data = fragmentation_data();

        if(max_heap_size < (data.total_alloc + data.total_free)){// does test on if maxheap if smaller if so set to total alloc
            max_heap_size = data.total_alloc + data.total_free;
        }

        fprintf(fpt,"%zu,",data.total_alloc); // total alloc
        fprintf(fpt,"%zu,", data.total_free); // total free
        fprintf(fpt,"%zu,",max_heap_size); // peak heap size

        fprintf(fpt,"%zu\n", data.num_free_blocks); // num of free chunks


    #else
        struct mallinfo info;

        info = dlmallinfo();

        fprintf(fpt,"%zu,", info.uordblks); // Total allocated space (uordblks):
        fprintf(fpt,"%zu,", info.fordblks); // Total free space (fordblks):
        fprintf(fpt,"%zu,", info.usmblks); // Max. TOTAL HEAP (usmblks):

        fprintf(fpt,"%zu\n", info.ordblks);// # of free chunks (ordblks):
        
    #endif
}

void collect_data(int action_performed, int index, long long int size, FILE* fpt){ // function that calls both above data collection functions
    // check_freelist_info();
    // display_requestinfo(action_performed, index, size,fpt);
    // display_mallinfo(fpt);
}


// MAIN BELOW

int main(void) {

    setlocale(LC_NUMERIC,"");//comma debug

    #if USE_DLPLUS_MALLOC
        printf("Using functions from: custom dlplus_malloc \n");
        if (dlplus_init()){
            exit(1);//end program if init failed
        }
    #else
        printf("Using functions from: standard dlmalloc \n");
    #endif

    int seed;
    seed = 10;// change this to be a specific number of your choice
    srand(seed); // randomises the see

    char filename[50];
    #if USE_DLPLUS_MALLOC
        snprintf(filename,sizeof(filename),"custom_data_seed%d.csv",seed);
    #else
        snprintf(filename,sizeof(filename),"dlmalloc_data_seed%d.csv",seed);
    #endif
    // FILE *fpt = fopen(filename,"w+");
    
    // if(fpt == NULL){
    //     perror("Error creating file");
    //     exit(EXIT_FAILURE);  // Exit if file creation fails
    // }

    // fprintf(fpt,"INPUTtotalalloc,INPUTtotalfree,INPUTpeakheapsize,OUTPUTtotalloc,OUTPUTtotalfree,OUTPUTpeakheapsize,numberoffreeblocks\n");//write csv titles





    clock_t start = clock(); // starts test

    printf("Now doing malloc functions\n"); // does random mallocs
    for (int i = 0; i < ITERATIONS; ++i) {
        size_t size = RANDOM_SIZE();
        block_sizes[i] = 0;
        pointers[i] = malloc(size);
        if (pointers[i] != NULL) {
            populate_array(i,size);
            // count_allocs(size);
            // collect_data(1,0,size,fpt);
        }
    }


    printf("Now doing free for realloc\n");
    for (int i = 0; i < ITERATIONS; ++i) { // Frees random pointers
        // int index = i;
        int index = rand() % ITERATIONS;
        // count_frees(index,0);
        // collect_data(2,index,0,fpt);
        free(pointers[index]);
        clear_array(index);

    }
        // coalesce();

    printf("Now doing realloc functions\n");
    for (int i = 0; i < ITERATIONS / 2; ++i) { // testing for realloc
        int index = rand() % ITERATIONS;
        size_t size = RANDOM_SIZE();

                // printf("original block size: %d, requested size%lu \n",block_sizes[index],size);
        // find out original block size and remainder
        if (size != 0){
            void *x = realloc(pointers[index], size); // coalesce not working properly with realloc
            if (x != NULL) {
                pointers[index] = x;
                // count_reallocs(index,size);
                // collect_data(3,index,size,fpt);
                populate_array(index,size);
            } 
        }
    }

    printf("Verifiying that chars were stored correctly\n");
    for (int i = 0; i < ITERATIONS; ++i) { // verifies that all allocations actually stored what was requested
        for (int j = 0; j < block_sizes[i]; ++j) {
            assert(pointers[i][j] == chars[i]);
        }
    }
    
    printf("Now freeing all \n");
    for (int i = 0; i < ITERATIONS; ++i) { // frees everything else and ends test
        free(pointers[i]);
        // count_frees(i,0);
        // collect_data(2,i,0,fpt);
        pointers[i] = NULL;
        block_sizes[i] = 0;
        chars[i] = '\0';
    }



    clock_t stop = clock(); // ends test and prints time taken
    double time_taken = (double)(stop - start) / CLOCKS_PER_SEC;
    printf("Time taken: %.02fs\n", time_taken);

    // fclose(fpt);
    // printf("File '%s' created and written successfully.\n", filename);

    return 0;
}