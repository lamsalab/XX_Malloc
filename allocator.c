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

typedef struct node {
  struct node * next;
} node;

typedef struct first {
  int size;
  struct node * head;
  struct first * next;
} first;

// USE ONLY IN CASE OF EMERGENCY
bool in_malloc = false;           // Set whenever we are inside malloc.
bool use_emergency_block = false; // If set, use the emergency space for allocations
char emergency_block[1024];       // Emergency space for allocating to print errors
void * pointers[8];
bool arr[8];

//https://stackoverflow.com/questions/466204/rounding-up-to-next-power-of-2

size_t roundUp(long long v) {
  int leading = __builtin_clzll(v);
  int trailing = __builtin_ctzll(v);

  if (v < 16) {
    return (long long) 16;
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

struct first* xxmallocHelper(size_t size_small, int index) {
  void * p = mmap (NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  if(p == MAP_FAILED) {
    use_emergency_block = true;
    perror("mmap");
    exit(2);
  }
  int starting_index;
  int no_of_chunks = PAGE_SIZE/size_small;
  intptr_t temp = (intptr_t)p;
  first * start = p;
  //for 16 bytes
  if (index == 0) {
    starting_index = 2;
    temp = temp + (size_small * 2);
    start->head = (node *)temp;
  }
  //for the rest
  else {
    starting_index = 1;
    temp = temp + size_small;
    start->head = (node *)temp;
  }
  //Set up the header.
  start->size = size_small;
  start->next = NULL;
  for (int i = starting_index; i < no_of_chunks; i++) {
    node * n = (node *)temp;
    n->next = (node *)(temp + size_small);
    temp = temp + size_small;
    if (i == no_of_chunks - 1) {
      n->next = NULL;
    }
  }
  return start;
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

  void * ret;
  
  if (size > 2048) {
    size_t object_size = ROUND_UP(size, PAGE_SIZE);
    void * p = mmap (NULL, object_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if(p == MAP_FAILED) {
      use_emergency_block = true;
      perror("mmap");
      exit(2);
    }
    in_malloc = false;
    return p;
  }
  else {
    int index = (log10 (size_small)/log10(2)) - (log10(16)/ log10(2));
    //if memeory has already been allocated before   
    if (pointers[index] != NULL) {
      first * start = pointers[index];
      //Traverse till you find a free memory or break if you reach the last chunk
      while (start->head == NULL) {
        if (start->next != NULL) {
          start = start->next;
          } else {
          break;
          }
      }
      //if all spots have been allocated, allocate a new chunk
      if (start->head == NULL) {
        first * chunk = xxmallocHelper(size_small, index);
        void* ret;
        start->next = chunk;
        start = chunk;
        ret = chunk->head;
        start->head = start->head->next;
        in_malloc = false;
        return ret;
      }
      //if there is space left
      else {
        void * ret;
        ret = start->head;
        start->head = start->head->next;
        in_malloc = false;
        return ret;
      }
    }
    //if memeory hasnt already been allocated before
    else {      
      first * chunk = xxmallocHelper(size_small, index);
      pointers[index] = chunk;
      void * ret;
      ret = chunk->head;
      chunk->head = chunk->head->next;
      in_malloc = false;
      return ret;      
    }
  }
}
/*    
      }
      if (arr [index] == false) {
      void * p = mmap (NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
      if(p == MAP_FAILED) {
      use_emergency_block = true;
      perror("mmap");
      exit(2);
      }
      pointers[index] = p;
      arr[index] = true;
      //for the allocating loop
      int starting_index;
      int no_of_chunks = PAGE_SIZE/size_small;
      intptr_t temp = (intptr_t)p;
      first * start = p;
      //for 16 bytes
      if (index == 0) {
      starting_index = 2;
      temp = temp + (size_small * 2);
      start->head = (void *)temp;
      }
      //for the rest
      else {
      starting_index = 1;
      temp = temp + size_small;
      start->head = (void *)temp;
      }
      //Set up the header.
      start->size = size_small;
      start->next = NULL;
      for (int i = starting_index; i < no_of_chunks; i++) {
      node * n = (void *)temp;
      n->next = (void *)(temp + size_small);
      temp = temp + size_small;
      if (i == no_of_chunks - 1) {
      n->next = NULL;
      }
      }
      //n->next = NULL;
      ret = start->head;

      start->head = start->head->next;
      // Done with malloc, so clear this flag
      in_malloc = false;
      return ret;
      }
      else {
      first * start = pointers[index];
      if (start->head != NULL) {
      node * cur = start->head;
      start->head = cur->next;
      void * ret = cur;
      in_malloc = false;
      return ret;
      }
      else {
      first * start = pointers[index];
      void * p = mmap (NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0)
      in_malloc= false;
      void * s;
      return s;
      }
      }
      }
      }*/
/**
 * Get the available size of an allocated object
 * \param ptr   A pointer somewhere inside the allocated object
 * \returns     The number of bytes available for use in this object
 */
size_t xxmalloc_usable_size(void* ptr) {
  // We aren't tracking the size of allocated objects yet, so all we know is that it's at least PAGE_SIZE bytes.

  intptr_t temp = (intptr_t)ptr % PAGE_SIZE;
  
  first * header = (first*)(ptr - temp);
  return header->size;
  /*
    intptr_t cur = (intptr_t) ptr;  
    first*temp = (first*)((cur/PAGE_SIZE)* PAGE_SIZE);
    return temp->size;
  */
  
}



/**
 * Free space occupied by a heap object.
 * \param ptr   A pointer somewhere inside the object that is being freed
 */
void xxfree(void* ptr) {
  size_t size = xxmalloc_usable_size(ptr);
  intptr_t cur = (intptr_t)ptr;
  cur = (cur/ size) * size;
  node * new;
  new = (void*)cur;
  int index = (log10 (size)/log10(2)) - (log10(16)/ log10(2));
  first* temp = pointers[index];
  new->next = temp->head;
  temp->head = new;
    
    
}

