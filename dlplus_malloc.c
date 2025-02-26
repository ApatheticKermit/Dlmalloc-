#include <unistd.h> 
#include <stdint.h>
#include <stdlib.h>





#include <stdio.h>
#include <string.h>

#include "dlplus_malloc.h"

#define BINS 20
#define POINTER_SIZE (sizeof(chunkpointer))
#define CONVERT_TO_POINTER(chunk) ((void*)((char*)(chunk) + sizeof(chunk->header)))
#define CONVERT_TO_CHUNK(pointer) ((chunkpointer)((char*)(pointer) - sizeof(((chunkpointer)pointer)->header)))

#define ALIGN(size) (((size) + (7)) & ~0x7) // aligns mem to 8 bytes used in malloc mainly
#define MIN_CHUNK_SIZE (sizeof(size_t) + 2 * POINTER_SIZE) //
#define FIND_BIN(ind, size) { size_t value = 64; for (ind = 0 ; ind < BINS; ++ind) { if (value > size) break; else value *= 2;}}

#define EXTRACT_SIZE(size) (size & ~7)// func to excract inuse bit
#define GET_IN_USE_BIT(ptr) ((ptr)->header & 1)//func to check inuse
#define SET_INUSE_BIT(size, bit) (size + bit)//func to insert inuse bit
#define SET_HEADER_INFO(ptr, size, bit) ((ptr)->header = SET_INUSE_BIT(size,bit))


#define TRAVERSE(ptr) ((chunkpointer)((char*)ptr + EXTRACT_SIZE(ptr->header)))


// global variables

chunkpointer freelist[BINS];
chunkpointer end_heap;
chunkpointer start_heap;

// Functions

int dlplus_init(void){

    for (int i = 0; i < BINS; ++i){
        freelist[i] = NULL; // check that the pointer is pointing to null, rather than being NULL?
    }
    // //call malloc with min size, but make it special and set startheap to that ptr
    if((start_heap = sbrk(MIN_CHUNK_SIZE)) == NULL){ // alloc new mem with sbrk if no suitable free chunk is found
        return 1;
    }
    
    SET_HEADER_INFO(start_heap,MIN_CHUNK_SIZE,1);
    end_heap = start_heap;
    

    return 0; // return 0 if success
}

void* first_fit_search_free_list(int bin, size_t size){ // do a first fit search on the bin by traversing using the next pointer
    chunkpointer roving_ptr = freelist[bin]; // make a roving pointer that starts at the start of the bin
    
    if (roving_ptr == NULL){
        return NULL;
    }

    while (1){
        if(roving_ptr->header >= size){ // if roving pointer size is big enough
            return roving_ptr;
        }
        if (roving_ptr->next_ptr == NULL){
            
            break;
        }
        else{
            roving_ptr = roving_ptr->next_ptr;
        }
    }    
    return NULL;
}

void del_from_free_list(int bin, chunkpointer ptr){
    if (bin < 0 || bin >= BINS){
        return;
    }

    chunkpointer* bin_pointer = &freelist[bin];

    if(ptr->previous_ptr == NULL){ // if there is no prev,
        if (ptr->next_ptr == NULL){ // if no next
            *bin_pointer = NULL;// set start of freelist to null
            return;
        } else{
            *bin_pointer = ptr->next_ptr;
            ptr->next_ptr->previous_ptr = ptr->previous_ptr;
            return;
        }
    }
    else{ // if there is a prev pointer
        ptr->previous_ptr->next_ptr = ptr->next_ptr;
    } 
    if (ptr->next_ptr != NULL){ // if next is null
        ptr->next_ptr->previous_ptr = ptr->previous_ptr;
    }
}

void realloc_clear_free(chunkpointer ptr, chunkpointer stop, size_t remainder){
    int bin;
    chunkpointer roving_ptr = ptr;
    while (roving_ptr != stop){
        roving_ptr = TRAVERSE(roving_ptr);
        FIND_BIN(bin, EXTRACT_SIZE(roving_ptr->header));
        del_from_free_list(bin,roving_ptr);
    }
}

void* try_inplace_realloc(chunkpointer ptr, size_t size, size_t remainder){
    size_t total_size = 0;
    chunkpointer roving_ptr = ptr;
    while (roving_ptr != end_heap && !GET_IN_USE_BIT(TRAVERSE(roving_ptr))){
        total_size += EXTRACT_SIZE(TRAVERSE(roving_ptr)->header);
        roving_ptr = TRAVERSE(roving_ptr);
        if(total_size >= remainder){// check that total is bigger or equal to remainder
            realloc_clear_free(ptr,roving_ptr,remainder);
            SET_HEADER_INFO(ptr,EXTRACT_SIZE(ptr->header)+total_size,1);
            return CONVERT_TO_POINTER(ptr);
        }
    }
    return NULL; 
}

void* dlplus_malloc(size_t size) {
    int bin;
    chunkpointer pointer;
    size_t remainder_size;

    if (size == 0){ // 0 sized alloc
        return NULL;
    }
    
    size_t actualsize = ALIGN(size + sizeof(chunkpointer));// Include the size of the chunk metadata in the total requested size

    if (actualsize < MIN_CHUNK_SIZE) {// Ensure minimum chunk size is met
        actualsize = MIN_CHUNK_SIZE;
    }
    FIND_BIN(bin, actualsize);
    
    while (bin<BINS){
        if((pointer = first_fit_search_free_list(bin,actualsize)) != NULL){ // if block is found   
            remainder_size = EXTRACT_SIZE(pointer->header) - actualsize;
            if(MIN_CHUNK_SIZE < remainder_size){

                split(pointer, actualsize, remainder_size);

                SET_HEADER_INFO(pointer,actualsize,1);
                del_from_free_list(bin, pointer);
            }
            else{

                SET_HEADER_INFO(pointer,EXTRACT_SIZE(pointer->header),1);
                del_from_free_list(bin,pointer);// remove pointer from free list thi is the messe dup one
            }
                return CONVERT_TO_POINTER(pointer);// then return it
        }
        else{
            bin++;
        }

    }
    
    if((pointer = sbrk(actualsize)) == NULL){ // alloc new mem with sbrk if no suitable free chunk is found
        return NULL;
    }


    // coalesce(); uncomment this when submitting

    SET_HEADER_INFO(pointer,actualsize,1);

    end_heap = pointer;

    return CONVERT_TO_POINTER(pointer); // returns mem adress after the header
}

void LIFO_bin_insert (int bin, chunkpointer ptr){
    if (bin < 0 || bin >= BINS ){
        return;
    }
    chunkpointer bin_pointer = freelist[bin];

    if(bin_pointer == NULL){ // if bin is empty set next and prev pointers to null
        ptr->next_ptr = NULL;
        ptr->previous_ptr = NULL;
    }
    else{ 
        bin_pointer->previous_ptr = ptr;// set prev pointer of pointer to bin to this ptr 
        ptr->next_ptr = bin_pointer;// set next pointer to the previous pointer to bin
        ptr->previous_ptr = NULL;//then set prev pointer to null
    }
    freelist[bin] = ptr;
}

void dlplus_free(void* ptr){
    int bin;

    if (ptr == NULL){
        return;
    }
    // coalesce();
    chunkpointer pointer = CONVERT_TO_CHUNK(ptr);// may be to do with converting to chunk
    size_t size = EXTRACT_SIZE(pointer->header);
    SET_HEADER_INFO(pointer,size,0);
    FIND_BIN(bin, size);

    LIFO_bin_insert(bin,pointer);// insert block onto free lisT
}

void split(chunkpointer ptr, size_t size, size_t remainder_size){
    int bin;
    chunkpointer newblock = (chunkpointer)((char*)ptr + size);
    SET_HEADER_INFO(newblock,remainder_size,0);

    FIND_BIN(bin, remainder_size);
    
    LIFO_bin_insert(bin, newblock);

    if(end_heap == ptr){
    end_heap = newblock;
    }

}

void coalesce(){ 
    chunkpointer roving_ptr = start_heap;
    chunkpointer start_of_free;
    size_t total_size;
    int bin;


    
    if (start_heap == end_heap){
        return;
    } else{
        while(1){
            if (roving_ptr >= end_heap){
                break;
            } 
            if(!GET_IN_USE_BIT(roving_ptr) && !GET_IN_USE_BIT(TRAVERSE(roving_ptr)) && EXTRACT_SIZE(TRAVERSE(roving_ptr)->header) != 0){
                start_of_free = roving_ptr;
                total_size = EXTRACT_SIZE(roving_ptr->header);
                FIND_BIN(bin,total_size);
                del_from_free_list(bin,start_of_free);
                while (roving_ptr != end_heap && !GET_IN_USE_BIT(roving_ptr) && !GET_IN_USE_BIT(TRAVERSE(roving_ptr)) && EXTRACT_SIZE(TRAVERSE(roving_ptr)->header) != 0){
                    total_size += EXTRACT_SIZE(TRAVERSE(roving_ptr)->header);
                    FIND_BIN(bin,(EXTRACT_SIZE(TRAVERSE(roving_ptr)->header)));
                    del_from_free_list(bin,TRAVERSE(roving_ptr));
                    roving_ptr = TRAVERSE(roving_ptr);
                }
                SET_HEADER_INFO(start_of_free,total_size,0);
                FIND_BIN(bin, total_size);
                LIFO_bin_insert(bin,start_of_free);// insert block onto free lisT
                if(roving_ptr == end_heap){
                    end_heap = start_of_free;
                }
            }
            else{
                roving_ptr = TRAVERSE(roving_ptr);   
            }
        }
    }
    
}

void* dlplus_realloc(void* ptr,size_t size){
    void* new_ptr;
    if(size == 0){
        
        dlplus_free(ptr);
        return ptr;
    } 
    else if (ptr == NULL){

        return dlplus_malloc(size);
    }
    else if (ptr != NULL){

        size_t old_size = EXTRACT_SIZE(CONVERT_TO_CHUNK(ptr)->header);
        size = ALIGN(size + sizeof(chunkpointer));

        if(size < MIN_CHUNK_SIZE){
            size = MIN_CHUNK_SIZE;
        }
        if (old_size == size){
            return ptr;
        } else if(size > old_size){

            size_t remainder = size - old_size;
            if((new_ptr = try_inplace_realloc(CONVERT_TO_CHUNK(ptr),size,remainder)) != NULL){ 

                return new_ptr;
            } 
            else{
                if((new_ptr = dlplus_malloc(size)) == NULL){
                    return NULL;
                }
                memcpy(new_ptr,ptr,old_size);
                dlplus_free(ptr);
                return new_ptr;
            }

        } else { // smaller size
            size_t remainder = old_size - size;
            if(remainder > MIN_CHUNK_SIZE){
                split(CONVERT_TO_CHUNK(ptr),size,remainder);
                SET_HEADER_INFO(CONVERT_TO_CHUNK(ptr),size,1);
                return ptr;
            } else{
                return ptr;
            }
        }
    }
    return NULL; // return null if negative alloc
}


// DEBUG FUNCTIONS

void check_traverse() {
    int debug = 0;
    chunkpointer roving_ptr = start_heap;
    while(roving_ptr != end_heap && debug <50){
        debug++;
        roving_ptr = TRAVERSE(roving_ptr);
        // printf("%d,ptr: %lu, size:%ld, bit(%ld)\n",debug,(unsigned long)(roving_ptr),EXTRACT_SIZE(roving_ptr->header),GET_IN_USE_BIT(roving_ptr));
        // printf("roving:%p\n",CONVERT_TO_POINTER(roving_ptr));
        // printf("size:%ld\n",EXTRACT_SIZE(roving_ptr->header));
    }
}

void check_freelist_info() {// check that all in free list are 0
    size_t size;
    check_traverse();

    for (int i = 0; i < BINS; i++) {

        chunkpointer ptr = freelist[i];
        while (ptr != NULL) {
            size = EXTRACT_SIZE(ptr->header); // size test
            if(ptr->previous_ptr != NULL){
                if (ptr->previous_ptr->next_ptr != ptr){
                    printf("prev pointer error !!\n");
                    // exit(1);
                }
            }
            if(ptr->next_ptr != NULL){
                if(ptr->next_ptr->previous_ptr != ptr){
                    // printf("bin:%d ptr: %lu, size:%ld, bit(%ld)\n",i,(unsigned long)CONVERT_TO_POINTER(ptr),EXTRACT_SIZE(ptr->header),GET_IN_USE_BIT(ptr));
                    printf("next pointer error !!\n");
                    // exit(1);
                }
            }
            if (GET_IN_USE_BIT(ptr) != 0 ){
                printf("block marked as inuse, but on freelist ERROR\n");
                // exit(1);
            }
            // printf("Status(%ld) Size:%ld %p -> ",GET_IN_USE_BIT(ptr),EXTRACT_SIZE(ptr->header), ptr);
            
            ptr = ptr->next_ptr; // traverse

        }
    }
}

// TESTING FUNCTION

struct fragdata fragmentation_data(){
    struct fragdata data;
    data.num_free_blocks = 0; //TOTAL NUMBER OF FREE CHUNKS
    data.total_free = 0; //TOTAL AMOUNT OF FREE SPACE
    data.total_alloc = 0; //TOTAL AMOUNT OF ALLOCATED SPACE
     
    chunkpointer roving_ptr = start_heap;
    while(1){
        if(GET_IN_USE_BIT(roving_ptr) == 1){ // if allocated
            data.total_alloc += EXTRACT_SIZE(roving_ptr->header);
        } else { //else must be free
            data.total_free += EXTRACT_SIZE(roving_ptr->header);
            data.num_free_blocks++;
        }
        if(roving_ptr == end_heap){
            break;// end loop when end of heap is reached
        }
        roving_ptr = TRAVERSE(roving_ptr);
    }
    return data;
}