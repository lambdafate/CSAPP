/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "lambdafate",
    /* First member's full name */
    "Wu",
    /* First member's email address */
    "lambdafate@163.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

#define MAX(v1, v2) (((v1) > (v2)) ? (v1) : (v2))

#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1 << 12)

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8
// #define ALIGN(size)       (ALIGNMENT * (size + ALIGNMENT - 1) / ALIGNMENT)

#define PACK(size, alloc) ((size) | (alloc))

#define GET(hfp) (*(unsigned int *)(hfp))
#define PUT(hfp, val) (*(unsigned int *)(hfp) = (val))

#define FREE_PREV(bp)      ((unsigned int*)(bp))
#define FREE_NEXT(bp)      ((unsigned int*)(bp) + 1)

#define GET_ALLOC(hfp) (GET(hfp) & 0x01)
#define GET_SIZE(hfp) (GET(hfp) & ~0x07)

// get head and foot pointer by block pointer
#define HEAD(bp) ((char *)(bp)-WSIZE)
#define FOOT(bp) ((char *)(bp) + GET_SIZE(HEAD(bp)) - DSIZE)

// get prev and next block pointer by current pointer
#define BLOCK_PREV(bp) ((char *)(bp)-GET_SIZE(((char *)bp - DSIZE)))
#define BLOCK_NEXT(bp) ((char *)(bp) + GET_SIZE(HEAD(bp)))

static char *heap_listp;
static char *free_head;

static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t size);
static void place(void *bp, size_t size);
static void heap_checker();

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    heap_listp = NULL;
    free_head = NULL;

    if ((heap_listp = (char *)mem_sbrk(4 * WSIZE)) == (void *)(-1))
    {
        return -1;
    }

    PUT(heap_listp, 0);
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));
    heap_listp += DSIZE;

    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
    {
        return -1;
    }

    // free_head != NULL
    assert(free_head != NULL);

    // heap_checker();
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    if (size <= 0){
        return NULL;
    }
    // printf("size: %d  ", size);
    // fflush(stdout);

    size = (size + DSIZE + ALIGNMENT - 1) / ALIGNMENT;
    size = ALIGNMENT * size;

    // printf("malloc size: %d\t", size);
    // fflush(stdout);

    char *bp;
    if ((bp = find_fit(size)) != NULL)
    {
        // printf("malloc ptr: %p\n", bp);
        place(bp, size);
        // heap_checker();
        return bp;
    }
    if ((bp = extend_heap(MAX(size, CHUNKSIZE) / WSIZE)) == NULL)
    {
        // printf("malloc failed: NULL\n");
        return NULL;
    }
    // printf("\nextend heap over!\n");
    place(bp, size);
    // printf("malloc ptr: %p\n", bp);
    // heap_checker();
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    // heap_checker();
    if (ptr == NULL)
    {
        return;
    }
    size_t size = GET_SIZE(HEAD(ptr));
    PUT(HEAD(ptr), PACK(size, 0));
    PUT(HEAD(ptr), PACK(size, 0));

    coalesce(ptr);  
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size){
    if (size <= 0){
        mm_free(ptr);
        return NULL;
    }
    if (ptr == NULL)
    {
        return mm_malloc(size);
    }

    char *bp = (char *)mm_malloc(size);
    if (bp == NULL)
    {
        mm_free(ptr);
        return NULL;
    }
    size_t copy_size = GET_SIZE(HEAD(ptr)) - DSIZE;
    if (GET_SIZE(HEAD(ptr)) > GET_SIZE(HEAD(bp)))
    {
        copy_size = GET_SIZE(HEAD(bp)) - DSIZE;
    }
    memmove(bp, ptr, copy_size);
    mm_free(ptr);
    return bp;
}

static void *extend_heap(size_t words)
{
    char *brk;
    size_t size = (words % 2) ? ((words + 1) * WSIZE) : (words * WSIZE);

    if ((brk = (char *)(mem_sbrk(size))) == (void *)-1){
        return NULL;
    }
    // printf("extend-heap: extend-size = %d   old brk = %p    new brk = %p\n", size, brk, mem_heap_hi()+1);
    
    PUT(HEAD(brk), PACK(size, 0));
    PUT(FOOT(brk), PACK(size, 0));
    PUT(HEAD(BLOCK_NEXT(brk)), PACK(0, 1));
    
    // coalesce
    return coalesce(brk);
}

/*
    coalesce prev and next free blocks
    case 1: prev alloced, next alloced
    case 2: prev alloced, next free
    case 3: prev free   , next alloced
    case 4: prev free   , next free
*/
static void *coalesce(void *bp){
    int prev_alloc = GET_ALLOC(HEAD(BLOCK_PREV(bp)));
    int next_alloc = GET_ALLOC(HEAD(BLOCK_NEXT(bp)));
    size_t size = GET_SIZE(HEAD(bp));

    if (prev_alloc && next_alloc){
        if(free_head == NULL || bp < free_head){
            PUT(FREE_PREV(bp), NULL);
            PUT(FREE_NEXT(bp), free_head);
            if (free_head != NULL)
            {
                PUT(FREE_PREV(free_head), bp);
            }
            free_head = bp;
            return bp;
        }
        char *work = free_head;
        while(GET(FREE_NEXT(work)) != NULL && work < bp){
            work = GET(FREE_NEXT(work));
        }
        if(work > bp){
            PUT(FREE_PREV(bp), GET(FREE_PREV(work)));
            PUT(FREE_NEXT(bp), work);

            PUT(FREE_NEXT(GET(FREE_PREV(bp))), bp);
            PUT(FREE_PREV(work), bp);
        }else{
            PUT(FREE_NEXT(work), bp);
            
            PUT(FREE_PREV(bp), work);
            PUT(FREE_NEXT(bp), NULL);
        }
        return bp;
    }
    else if (prev_alloc && !next_alloc){

        PUT(FREE_PREV(bp), GET(FREE_PREV(BLOCK_NEXT(bp))));
        PUT(FREE_NEXT(bp), GET(FREE_NEXT(BLOCK_NEXT(bp))));

        size += GET_SIZE(HEAD(BLOCK_NEXT(bp)));
        
        PUT(HEAD(bp), PACK(size, 0));
        PUT(FOOT(bp), PACK(size, 0));

        if(GET(FREE_NEXT(bp)) != NULL){
            PUT(FREE_PREV(GET(FREE_NEXT(bp))), bp);
        }
        if(GET(FREE_PREV(bp)) != NULL){
            PUT(FREE_NEXT(GET(FREE_PREV(bp))), bp);
        }else{
            free_head = bp;
        }
        return bp;
    }
    else if (!prev_alloc && next_alloc){
        char *prev_bp = BLOCK_PREV(bp);
        size += GET_SIZE(HEAD(prev_bp));
    
        PUT(HEAD(prev_bp), PACK(size, 0));
        PUT(FOOT(prev_bp), PACK(size, 0));
        return prev_bp;
    }
    else{
        PUT(FREE_NEXT(BLOCK_PREV(bp)), GET(FREE_NEXT(BLOCK_NEXT(bp))));
        if(GET(FREE_NEXT(BLOCK_NEXT(bp))) != NULL){
            PUT(FREE_PREV(GET(FREE_NEXT(BLOCK_NEXT(bp)))), BLOCK_PREV(bp));
        }

        // PUT(FREE_PREV(bp), NULL);
        // PUT(FREE_NEXT(bp), free_head);
        // PUT(FREE_PREV(free_head), bp);
        // free_head = bp;
        // return bp;

        
        size += GET_SIZE(HEAD(BLOCK_PREV(bp))) + GET_SIZE(HEAD(BLOCK_NEXT(bp)));
        bp = BLOCK_PREV(bp);
        PUT(HEAD(bp), PACK(size, 0));
        PUT(FOOT(bp), PACK(size, 0));
        return bp;
        
    }
}

static void *find_fit(size_t size)
{
    // first fit
    char *bp = free_head;
    while(bp != NULL){
        // printf("find-fit: bp = %p  block-size = %d\n", bp, GET_SIZE(HEAD(bp)));
        if(GET_SIZE(HEAD(bp)) >= size){
            return bp;
        }
        bp = GET(FREE_NEXT(bp));
    }
    return NULL;
}

static void place(void *bp, size_t size)
{
    int left_size = GET_SIZE(HEAD(bp)) - size;
    if (left_size < (2 * DSIZE))
    {
        if(GET(FREE_PREV(bp)) == NULL){
            free_head = GET(FREE_NEXT(bp));
            if(free_head != NULL){
                PUT(FREE_PREV(free_head), NULL);
            }
        }else{
            PUT(FREE_NEXT(GET(FREE_PREV(bp))), GET(FREE_NEXT(bp)));
            if(GET(FREE_NEXT(bp)) != NULL){
                PUT(FREE_PREV(GET(FREE_NEXT(bp))), GET(FREE_PREV(bp)));
            }
        }

        PUT(HEAD(bp), PACK(left_size + size, 1));
        PUT(FOOT(bp), PACK(left_size + size, 1));
    }
    else{
        PUT(HEAD(bp), PACK(size, 1));
        PUT(FOOT(bp), PACK(size, 1));

        char *next_bp = BLOCK_NEXT(bp);
        PUT(HEAD(next_bp), PACK(left_size, 0));
        PUT(FOOT(next_bp), PACK(left_size, 0));

        PUT(FREE_PREV(next_bp), GET(FREE_PREV(bp)));
        PUT(FREE_NEXT(next_bp), GET(FREE_NEXT(bp)));

        if(GET(FREE_PREV(bp)) != NULL){
            PUT(FREE_NEXT(GET(FREE_PREV(bp))), next_bp);
        }
        if(GET(FREE_NEXT(bp)) != NULL){
            PUT(FREE_PREV(GET(FREE_NEXT(bp))), next_bp);
        }

        if(GET(FREE_PREV(bp)) == NULL){
            free_head = next_bp;
        }
    }
}

static void heap_checker(){
    return;
    char *bp = BLOCK_NEXT(heap_listp);
    char *brk = mem_heap_hi() + 1;
    printf("\n*******************heap_checker: free_head = %p brk = %p\n", free_head, brk);
    while (bp != NULL && bp < brk)
    {
        printf("%d-%d   bp = %p\n", GET_SIZE(HEAD(bp)), GET_ALLOC(HEAD(bp)), bp);
        bp = BLOCK_NEXT(bp);
    }
    // printf("\n******************\n");
    // bp = free_head;
    // while(bp != NULL && bp < brk){
    //     printf("%d-%d   bp = %p\n", GET_SIZE(HEAD(bp)), GET_ALLOC(HEAD(bp)), bp);
    //     bp = GET(FREE_NEXT(bp));    
    // }
    printf("end heap checker!\n");
}