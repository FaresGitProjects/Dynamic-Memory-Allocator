#include <string.h>
#include <stdio.h>
#include <unistd.h>

/* When requesting memory from the OS using sbrk(), request it in
 * increments of CHUNK_SIZE. */
#define CHUNK_SIZE (1<<12)
#define POINTER_SIZE 8



typedef struct ListNode {
    size_t size;
    struct ListNode *next;
}ListNode;


static ListNode **free_list = NULL;


void free(void *ptr);

static void *prime_for_allocation(void *block_ptr) {
    *(size_t *)block_ptr = *((size_t *)block_ptr)^(1<<0);
    block_ptr += sizeof(size_t);
    return block_ptr;
}

/*static void force_crash() {
    int *null = NULL;
    if(*null == 5) {return;}
    }*/
static int validate(char *state) {
    //fprintf(stderr,"Running in %s\n",state);
    if(free_list == NULL) {
        //  fprintf(stderr,"valid: List is null\n");
        return 1;
    }
    else {
        for(int i = 0; i < 5; i++) {
            if(free_list[i] != NULL) {
                //     fprintf(stderr,"%d invalid: 0-4 pointer not null\n",i);
                return 0;
            }
        }
        for(int i = 5; i <= 12; i++) {
            if(free_list[i] == NULL) {
                //  fprintf(stderr,"%d warning: 5-12 pointer null\n",i);
            }
            else {
                ListNode *ptr = free_list[i];
                size_t expected_size = (1<<i);
                while(ptr->next != NULL) {
                    if((ptr->size ^ expected_size)) {
                        //       fprintf(stderr,"invalid: List block marked as allocated\n   expected: %lu\n   received: %lu\n",expected_size,ptr->size);
                        return 0;
                    }
                    if(!((ptr->size | expected_size) == expected_size)) {
                        //         fprintf(stderr,"invalid: Size not as expected\n");
                        return 0;
                    }
                    ptr = ptr->next;
                }
            }
        }
    }
    return 1;
}


/*
 * This function, defined in bulk.c, allocates a contiguous memory
 * region of at least size bytes.  It MAY NOT BE USED as the allocator
 * for pool-allocated regions.  Memory allocated using bulk_alloc()
 * must be freed by bulk_free().
 *
 * This function will return NULL on failure.
 */
extern void *bulk_alloc(size_t size);

/*
 * This function is also defined in bulk.c, and it frees an allocation
 * created with bulk_alloc().  Note that the pointer passed to this
 * function MUST have been returned by bulk_alloc(), and the size MUST
 * be the same as the size passed to bulk_alloc() when that memory was
 * allocated.  Any other usage is likely to fail, and may crash your
 * program.
 */
extern void bulk_free(void *ptr, size_t size);

/*
 * This function computes the log base 2 of the allocation block size
 * for a given allocation.  To find the allocation block size from the
 * result of this function, use 1 << block_size(x).
 *
 * Note that its results are NOT meaningful for any
 * size > 4088!
 *
 * You do NOT need to understand how this function works.  If you are
 * curious, see the gcc info page and search for __builtin_clz; it
 * basically counts the number of leading binary zeroes in the value
 * passed as its argument.
 */

static inline __attribute__((unused)) int block_index(size_t x) {
    if (x <= 8) {
        return 5;
    } else {
        return 32 - __builtin_clz((unsigned int)x + 7);
    }
 }
 
/*
 * You must implement malloc().  Your implementation of malloc() must be
 * the multi-pool allocator described in the project handout.
 */
void *malloc(size_t size) {
    validate("malloc--1");
    if(size <= 0) {return NULL;}
    if(free_list == NULL) {
        void *sbrkptr = sbrk(CHUNK_SIZE);
        free_list = sbrkptr;
        for(int i = 0; i <= 12; i++ ) {
            free_list[i] = NULL;
            sbrkptr += POINTER_SIZE;
        }
        sbrkptr += 24;
        free_list[7] = sbrkptr;
        for(int i = 0; i < 31; i++) {
            ((ListNode *)sbrkptr)->size = 128;
            ((ListNode *)sbrkptr)->next = (sbrkptr+128);
            sbrkptr += 128;
        }
        sbrkptr-=128;
        ((ListNode *)sbrkptr)->next = NULL;
        //sbrkptr is beyond the prgrm brk - Abandon or back up
    }
    int index = 0;
    size_t block_size = 0;
    // Can I take from free list or need I bulk?
    for(int i = 32; i <= 4096; i=(i<<1)) {
        if(size <= (size_t)(i-8)) {
            block_size = i;
            index = block_index(i-8);
            break;
        }
    }
    //if index is zero, size is too large, bulk
    if(index) {
        //entry is null, sbrk and divide
        if(free_list[index] == NULL) {
            void *sbrkptr = sbrk(CHUNK_SIZE);
            free_list[index] = sbrkptr;;
            for(size_t i = 0; i < (CHUNK_SIZE/block_size); i++) {
                ((ListNode *)sbrkptr)->size = block_size;
                ((ListNode *)sbrkptr)->next = (sbrkptr+block_size);
                sbrkptr+=block_size;
            }
            //sbrkptr is beyond program break. Abandon or back up
            sbrkptr-=block_size;
            ((ListNode *)sbrkptr)->next = NULL;
        }
        void *allocation = free_list[index];
       if(free_list[index]->next == NULL) {
           free_list[index] = NULL;
           validate("malloc--2");
           return prime_for_allocation(allocation);
        }
        else {
            free_list[index] = free_list[index]->next;
            validate("malloc--3");
            return prime_for_allocation(allocation);
        }
    }
    else {
        void *bulk_ptr = bulk_alloc(size+8);
        *(size_t *)bulk_ptr = size+8;
        bulk_ptr = prime_for_allocation(bulk_ptr);
        validate("malloc--4");
        return bulk_ptr;
        //bulk
    }

    // Check free list index for any free blocks
    // If free block found, allocate
    // If not, call sbrk, chop it up and assign it to free list
    fprintf(stderr, "This shouldn't run");
    return bulk_alloc(size);
}

/*
 * You must also implement calloc().  It should create allocations
 * compatible with those created by malloc().  In particular, any
 * allocations of a total size <= 4088 bytes must be pool allocated,
 * while larger allocations must use the bulk allocator.
 *
 * calloc() (see man 3 calloc) returns a cleared allocation large enough
 * to hold nmemb elements of size size.  It is cleared by setting every
 * byte of the allocation to 0.  You should use the function memset()
 * for this (see man 3 memset).
 */
void *calloc(size_t nmemb, size_t size) {
    validate("calloc--1");
    if(nmemb == 0 || size == 0) {
        validate("calloc--2");
        return NULL;
    }
    void *return_ptr = malloc(size*nmemb);
    memset(return_ptr, 0, nmemb*size);
    validate("calloc--3");
    return return_ptr;
}

/*
 * You must also implement realloc().  It should create allocations
 * compatible with those created by malloc(), honoring the pool
 * alocation and bulk allocation rules.  It must move data from the
 * previously-allocated block to the newly-allocated block if it cannot
 * resize the given block directly.  See man 3 realloc for more
 * information on what this means.
 *
 * It is not possible to implement realloc() using bulk_alloc() without
 * additional metadata, so the given code is NOT a working
 * implementation!
 */
void *realloc(void *ptr, size_t size) {
    validate("realloc--1");
    //case 1: ptr null, size 0
    if(ptr == NULL && size == 0) {
        validate("realloc--2");
        return NULL; //equivalent to malloc(size)
    }
    //case 2: ptr init, size 0
    if(ptr != NULL && size == 0) {
        free(ptr);
        validate("realloc--3");
        return ptr;
    }
    //case 3: ptr null, size any
    if(ptr == NULL) {
        void *n = malloc(size);
        validate("realloc--4");
        return n;
    }
    //case 4: ptr init, size > 0
    if(ptr != NULL && size != 0) {
        size_t cur_size = (*((size_t *)(ptr-8)) & ~(1<<0))-8;
        if(cur_size > size) {
            validate("realloc--5");
            return ptr;
            //cur_size should be x;
        }
        else if(cur_size < size){
            void *new_block = malloc(size);
            memcpy(new_block,ptr,cur_size);
            validate("realloc--6");
            return new_block;
        }
        else {
            validate("realloc--7");
            return ptr;
        }
    }
    fprintf(stderr,"Realloc failed!");
    return NULL;
}

/*
 * You should implement a free() that can successfully free a region of
 * memory allocated by any of the above allocation routines, whether it
 * is a pool- or bulk-allocated region.
 *
 * The given implementation does nothing.
 */
void free(void *ptr) {
    validate("free--1");
    if(ptr == NULL) {
        validate("free--2");
        return;
    }
    ptr -= sizeof(size_t);
    *(size_t *)ptr = (*(size_t *)ptr) & ~(1<<0);
    size_t cur_size = (*(size_t *)ptr);
    //If this is a bulk allocation, bulk free
    if((cur_size-8) > 4088) {
        bulk_free(ptr, cur_size);
        validate("free--3");
        return;
    }
    else {
        int index = block_index(cur_size-8);
        if(free_list[index] == NULL) {
            free_list[index] = ptr;
            free_list[index]->next = NULL;
            validate("free--4");
            return;
        }
        else {
            ((ListNode *)ptr)->next = free_list[index];
            free_list[index] = ptr;
            validate("free--5");
            return;
        }
    }
    validate("free--6");
    return;
}
