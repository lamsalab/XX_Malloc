#define _GNU_SOURCE

#include <assert.h>
#include <malloc.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <math.h>
#include <stdbool.h>
// The minimum size returned by malloc
#define MIN_MALLOC_SIZE 16

// Round a value x up to the next multiple of y
#define ROUND_UP(x,y) ((x) % (y) == 0 ? (x) : (x) + ((y) - (x) % (y)))

// The size of a single page of memory, in bytes
#define PAGE_SIZE 0x1000

#define MAGIC_NUMBER 0xD00FCA75

typedef struct node_t {
  struct node_t * next_node;
} node_t;

typedef struct freelist_header {
  size_t size;
  size_t magic_number;
  struct node_t * free_node;
  struct freelist_header * next_header;
} freelist_header;

// USE ONLY IN CASE OF EMERGENCY
bool in_malloc = false;           // Set whenever we are inside malloc.
bool use_emergency_block = false; // If set, use the emergency space for allocations
char emergency_block[1024];       // Emergency space for allocating to print errors
//Pointer array that stores the pointers to the memory chunk
void * pointers[8];


//https://stackoverflow.com/questions/466204/rounding-up-to-next-power-of-2

size_t roundUp(long long v) {
  int leading = __builtin_clzll(v);
  int trailing = __builtin_ctzll(v);

  if (v < 16) {
    return 16;
  }
  if (abs(leading - trailing) == 1) {
    return v;
  }
  else {
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
  }
}

/**
 * Helper function that does the partitioning of memory based on size.
 * \param size  The minimium number of bytes that must be allocated
 * \param index  Index of the pointer array pointing to the memory chunk
 * \returns     A pointer to the beginning of whole chunk
 *              This function may return NULL when an error occurs.
 */
struct freelist_header* xxmallocHelper(size_t size_small, int index) {
  void * mem_chunk = mmap (NULL, PAGE_SIZE, PROT_READ
                           | PROT_WRITE, MAP_ANONYMOUS
                           | MAP_PRIVATE, -1, 0);
  
  if(mem_chunk == MAP_FAILED) {
    use_emergency_block = true;
    perror("mmap");
    exit(2);
  }
  
  int starting_index;
  int no_of_chunks = PAGE_SIZE/size_small;
  intptr_t temp = (intptr_t)mem_chunk;
  freelist_header * header = mem_chunk;
  
  //Creating space for header for 16 bytes
  if (index == 0) {
    starting_index = 2;
    temp = temp + (size_small * 2);
  }  
  //for the rest
  else {
    starting_index = 1;
    temp = temp + size_small;
  }
  
  //Set up the header.
  header->free_node = (node_t *)temp;
  header->size = size_small;
  header->next_header = NULL;
  header->magic_number = MAGIC_NUMBER;
  
  //Partitions the memory chunk in a linked list structure 
  for (int i = starting_index; i < no_of_chunks; i++) {
    node_t * new_node = (node_t *)temp;
    new_node->next_node = (node_t *)(temp + size_small);
    temp = temp + size_small;
    if (i == no_of_chunks - 1) {
      new_node->next_node = NULL;
    }
  }
  return header;
}
  
/**
 * Allocate space on the heap.
 * \param size  The minimium number of bytes that must be allocated
 * \returns     A pointer to the beginning of the allocated space.
 *              This function may return NULL when an error occurs.
 */
void* xxmalloc(size_t size) {
  // Before we try to allocate anything, check if we are trying to print an error or if
  // malloc called itself. This doesn't always work, but sometimes it helps.
  if(use_emergency_block) {
    return emergency_block;
  } else if(in_malloc) {
    use_emergency_block = true;
    puts("ERROR! Nested call to malloc. Aborting.\n");
    abort();
  }
  
  // If we call malloc again while this is true, bad things will happen.
  in_malloc = true;
  
  // Round the size up to the next multiple of the page size
  size_t size_small = roundUp(size);
  
  //Pointer to the memory that will be returned
  void * ret;  
  if (size > 2048) {
    size_t object_size = ROUND_UP(size, PAGE_SIZE);
    void * mem_size = mmap (NULL, object_size, PROT_READ
                            | PROT_WRITE, MAP_ANONYMOUS
                            | MAP_PRIVATE, -1, 0);
    
    if(mem_size == MAP_FAILED) {
      use_emergency_block = true;
      perror("mmap");
      exit(2);
    }
    in_malloc = false;
    return mem_size;
  }
  else {
    //Index of the array, which has a pointer to the memory (if allocated)
    int index = (log10 (size_small)/log10(2)) - (log10(16)/ log10(2));
    //if memory has already been allocated before   
    if (pointers[index] != NULL) {
      freelist_header * header = pointers[index];
      //Traverse till you find a free memory or break if you reach the last chunk
      while (header->free_node == NULL) {
        if (header->next_header != NULL) {
          header = header->next_header;
        } else {
          break;
        }
      }
      
      //if all spots have been allocated, allocate a new chunk
      if (header->free_node == NULL) {
        freelist_header * chunk = xxmallocHelper(size_small, index);
        void* ret;
        header->next_header = chunk;
        header = chunk;
        ret = chunk->free_node;
        header->free_node = header->free_node->next_node;
        in_malloc = false;
        return ret;
      }
      //if there is space in any of the chunks of the specified size
      else {
        void * ret;
        ret = header->free_node;
        header->free_node = header->free_node->next_node;
        in_malloc = false;
        return ret;
      }
    }
    
    //if memory hasnt already been allocated before, since pointer[index] is null
    else {      
      freelist_header * chunk = xxmallocHelper(size_small, index);
      pointers[index] = chunk;
      void * ret;
      ret = chunk->free_node;
      chunk->free_node = chunk->free_node->next_node;
      in_malloc = false;
      return ret;      
    }
  }
}

/**
 * Get the available size of an allocated object
 * \param ptr   A pointer somewhere inside the allocated object
*  \param size   size of the allocated object (or PAGE_SIZE)
 * \returns     Pointer to the rounded down multiple of size parameter
 */
freelist_header* xxmalloc_round_down (void * ptr, size_t size) {
    //Rounds down to the header of the page
  intptr_t temp = (intptr_t)ptr % size;  
  freelist_header * header = (freelist_header*)(ptr - temp);
  return header;
}

/**
 * Get the available size of an allocated object
 * \param ptr   A pointer somewhere inside the allocated object
 * \returns     The number of bytes available for use in this object
 */
size_t xxmalloc_usable_size(void* ptr) {
  return xxmalloc_round_down(ptr, PAGE_SIZE)->size;
}

/**
 * Check if the magic number exists
 * \param ptr   A pointer somewhere inside the allocated object
 * \returns     Magic number
 */
size_t xxmalloc_check_magic_number(void* ptr) {
  return xxmalloc_round_down(ptr, PAGE_SIZE)->magic_number;
}

/**
 * Free space occupied by a heap object.
 * \param ptr   A pointer somewhere inside the object that is being freed
 */
void xxfree(void* ptr) {
  if (ptr == NULL) {
    return;
  }
  size_t header_check = xxmalloc_check_magic_number(ptr);
  if (header_check =! MAGIC_NUMBER) {
    return;
  }
  size_t size = xxmalloc_usable_size(ptr);
  freelist_header* temp =  xxmalloc_round_down(ptr, size);
  freelist_header* header = xxmalloc_round_down(ptr, PAGE_SIZE);;
  node_t * new;
  new = (node_t*)temp;
  new->next_node = header->free_node;
  header->free_node = new;    
}

