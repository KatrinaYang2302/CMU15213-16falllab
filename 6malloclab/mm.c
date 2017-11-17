//mm.c
//by yufany (Katrina Yang)
//use an explicit segregated free list with 8 buckets, with LIFO policy and
//furthermore consideration on blocks sized 8 or less, using spare 16-sized blocks
//use a find-fit policy in between firstfit and bestfit:
//for small blocks(alias: xk, i.e. blocksize 16), allocate the head;
//for large blocks(alias: dk, i.e. blocksize>=32), find in its corresponding bucket:
//	if found, iterate at most [findThres] times to obtain a local minimum
//	else find the next nonempty bucket
//	and iterate at most [findThres] times there to obtain the local minimum
//In our code we choose bucket number 8, with smallest bucket sized 32, and growing up
//by doubling, i.e. 32~64, 64~128, ..., 2048~4096, 4096~infty
//and we also set findThres = 8 which will get the best performance.
//also, there is NO footter in allocated blocks; instead we use a bit to indicate it when
//coalescing.
//for the functions suitable for xk's ONLY, the function name is preceded by a 'x'.


//some terms:
//dk: block with size larger than 16
//xk: block with size equal to 16
//FL: free lists
//DFL: dk's free list array
//n-th DFL: the n-th bucket in the DFL. n can range from 0 to 7
//XFL: xk's free list


//signal bits in headers:
//the lowest bit is the allocated bit, i.e. 1 iff allocated.
//the second lowest bit is the prev-allocated bit, i.e. 1 iff the previous block is allocated
//the third lowest bit is xk bit, i.e. 1 iff the block is a xk
//note that all addresses are at least 8-aligned, so these 3 bits are independent of
//the "pure" addresses.

//using a total of 96 bytes of storage outside the heap.
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>

#include "mm.h"
#include "memlib.h"

/*
 * If you want debugging output, uncomment the following.  Be sure not
 * to have debugging enabled in your final submission
 */
// #define DEBUG

#ifdef DEBUG
/* When debugging is enabled, the underlying functions get called */
#define dbg_printf(...) printf(__VA_ARGS__)
#define dbg_assert(...) assert(__VA_ARGS__)
#define dbg_requires(...) assert(__VA_ARGS__)
#define dbg_ensures(...) assert(__VA_ARGS__) 
#define dbg_checkheap(...) mm_checkheap(__VA_ARGS__)
#define dbg_printheap(...) print_heap(__VA_ARGS__)
#else
/* When debugging is disabled, no code gets generated */
#define dbg_printf(...)
#define dbg_assert(...)
#define dbg_requires(...)
#define dbg_ensures(...)
#define dbg_checkheap(...)
#define dbg_printheap(...)
#endif

/* do not change the following! */
#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#define memset mem_memset
#define memcpy mem_memcpy
#endif /* def DRIVER */

/* What is the correct alignment? */
#define ALIGNMENT 16
typedef uint64_t kuai;//one block of data, i.e. 8 bytes

struct yikuai{
	//a dk
	kuai header;
	struct yikuai *prev;
	struct yikuai *next;
}; 

struct yixiaokuai{
	//a xk
	struct yixiaokuai *prev;
	struct yixiaokuai *next;
};

struct yisettings{
	//a struct to manipulate small block pointer arithmetics.
	//can only be pointed to the start address of a [xiaokuai].
	kuai prev;
	kuai next;
};

typedef struct yikuai dakuai;
typedef struct yixiaokuai xiaokuai;
typedef struct yisettings stgs;
//static const bool bestFit = true;//this value is set if a best-fit policy is used
static const int findThres = 9;//finding threshold. If in a best-fit search, we find for more than
//findThres, then return the currently best one
static const bool coalescePrint = 0;//whether to print in coalesce
static const bool mallocPrint = 0;
static const int segNum = 8;
static kuai* heapBeg = NULL;//Begin of heap (included)
static kuai* heapEnd = NULL;//End of heap (excluded); all values must be with these two
static dakuai* freeListHead[segNum];
static xiaokuai* xFreeListHead = NULL;
static stgs* Stgs;
//the head of the free list. NULL if there is no free blocks.
static const size_t xinKuaiSize = (1 << 12);
//the minimum expanding value when we run out of blocks

static int getFreeListIndex(size_t sz){
	//given the size of a block, determine its corresponding bucket.
	if(sz < 64) return 0;
	if(sz < 128) return 1;
	if(sz < 256) return 2;
	if(sz < 512) return 3;
	if(sz < 1024) return 4;
	if(sz < 2048) return 5;
	if(sz < 4096) return 6;
	return 7;
}

static bool isSmallBlock(dakuai* dk){
	//return TRUE if the block dk points to is a xk.
	return ((dk->header)/4)%2==1;
}

static void setSmallBlock(dakuai* dk){
	//set a block to a xk
	Stgs = (stgs*)dk;
	if((Stgs->prev/4)%2==0) Stgs->prev+=4;
	if((Stgs->next/4)%2==0) Stgs->next+=4;
}

static void setBigBlock(dakuai* dk){
	//set a block to a dk
	if(!isSmallBlock(dk)) return;
	dk->header -= 4;
	size_t sz = (dk->header/16)*16;
	*(kuai*)(((char*)dk)+sz-8) -= 4;
}

static dakuai* getPrev(dakuai* dk){
	//get the previous block in terms of free list
	if(isSmallBlock(dk)){
		xiaokuai* xk = (xiaokuai*) dk;
		Stgs = (stgs*)xk;
		size_t offset = ((size_t)Stgs->prev)%8;
		return (dakuai*)(((char*)(xk->prev)) - offset);
	}
	return dk->prev;
}

static dakuai* getNext(dakuai* dk){
	//get the next block in the free list
	if(isSmallBlock(dk)){
		xiaokuai* xk = (xiaokuai*) dk;
		Stgs = (stgs*)xk;
		size_t offset = ((size_t)Stgs->next)%8;
		return (dakuai*)(((char*)(xk->next)) - offset);
	}
	return dk->next;
}

static bool isMalloced(dakuai* dk){
	//return TRUE iff dk is malloced
	return (dk->header)%2==1;
}
static size_t getSize(dakuai* dk){
	//return the size of a block
	if(isSmallBlock(dk)) return 16;
	return (size_t)((dk->header)/16)*16;
}
static size_t getRoundSize(size_t sz){
	//round a number to the nearest nultiple of 16
	if(sz%16 == 0) return sz;
	return 16*(sz/16+1);
}

static bool isPrevMalloced(dakuai* dk){
	//return TRUE iff the (physically) previous one is malloced
	return (dk->header/2)%2==1;
}

static bool isFree(dakuai *dk){
	//return TRUE iff dk is free AND it is not out of bound
	return getSize(dk)!=0 && !isMalloced(dk);
}

static dakuai* getHeapNext(dakuai *dk){
	//get the (physically) next dakuai in heap. 
	//it guarantees neither it is free, nor it is in bound
	//so we have to check this explicitly when calling this function
	
	return (dakuai*)(((char*)(dk))+getSize(dk));
	
}

static dakuai* getHeapPrev(dakuai *dk){
	//get the (physically) previous dakuai in heap.
	//if that one is malloced, or out of bound, return NULL
	if(isPrevMalloced(dk)) return NULL;
	kuai footer = *(((kuai*)dk)-1);
	if((footer/4)%2 == 1){
		//small block
		//printf("prev is small block\n");
		if(footer%2 == 1) return NULL;
		else return (dakuai*)(((char*)dk)-16);
	}
	if(footer/16 == 0 || footer%2 == 1) return NULL;
	size_t prevSize = (footer/16)*16;
	//printf("prevSize is: %d\n",(int)prevSize);
	return (dakuai*)(((char*)dk)-prevSize);
}

//in the following 4 functions: sz denotes the size of a block, and
//isM denotes whether we wish to set the block free or allocated
//the last 3 ones are only available for dk's and its behaviour is undetermined for xk's

static void tianHF(kuai* k, size_t sz, bool isM){
	//fill in the header/footer content into a datablock(i.e. 8 bytes)
	//it keeps the prev-malloced and is-xk field!
	kuai prv = ((*k)/2)%2;
	*k = sz - sz%16 + isM + prv*2;
}

static void tianH(dakuai* dk, size_t sz, bool isM){
	//fill in the header of a given block
	tianHF(&dk->header, sz, isM);
}
static void tianF(dakuai* dk, size_t sz, bool isM){
	//fill in the footer of a given block
	tianHF((kuai*)(((char*)dk)+sz-8), sz, isM);
}
static void tianHFR(dakuai* dk, size_t sz, bool isM){
	//fill in both the header and footer of a given block
	tianH(dk, sz, isM);
	tianF(dk, sz, isM);
}

static void setPrevMalloced(dakuai* dk, bool isMalloced){
	//set the prev-malloced bit of dk to be isMalloced
	//leave other fields unchanged
	if(isPrevMalloced(dk)) dk->header -= 2;
	if(isMalloced) dk->header += 2;
}

static bool onlyOneFree(int idx){
	//check whether idx-th bucket has only one block
	//used when we are sure that the bucket is not empty
	return freeListHead[idx]->prev == freeListHead[idx];
}

static bool xOnlyOneFree(){
	//check whether the xk free-list has only one block
	//ised when we are sure that the xfree-list is not empty
	return (xiaokuai*)getNext((dakuai*)xFreeListHead) == xFreeListHead;
}

void setNext(xiaokuai* a, xiaokuai* b){
	//set the next block in the xfree-list of a to be b
	Stgs = (stgs*)a;
	size_t offset = (size_t)(Stgs->next%8);
	a->next = (xiaokuai*)(((char*)b)+offset);
}

void setPrev(xiaokuai* a, xiaokuai* b){
	//set the prev block in the xfree-list of a to be b
	Stgs = (stgs*)a;
	size_t offset = (size_t)(Stgs->prev%8);
	a->prev = (xiaokuai*)(((char*)b)+offset);
}

void setMalloced(xiaokuai* xk){
	//set a xk to be malloced
	if(isMalloced((dakuai*)xk)) return;
	Stgs = (stgs*)xk;
	Stgs->prev += 1;
	Stgs->next += 1;
}

void setFree(xiaokuai* xk){
	//set a xk to be free
	if(!isMalloced((dakuai*)xk)) return;
	Stgs = (stgs*)xk;
	Stgs->prev -= 1;
	Stgs->next -= 1;
}

void xDeleteFromFreeList(xiaokuai* xk){
	//delete xk from the xfree-list
	//printf("a small deletion\n");
	if(xOnlyOneFree()) xFreeListHead = NULL;
	else{
		//printf("multiple\n");
		if(xFreeListHead == xk) xFreeListHead = (xiaokuai*)getNext((dakuai*)xk);
		xiaokuai* pv = (xiaokuai*)getPrev((dakuai*)xk), *nx = (xiaokuai*)getNext((dakuai*)xk);
		setNext(pv, nx);
		setPrev(nx, pv);
	}
}

void xAddToFreeList(xiaokuai* xk){
	//add xk to the xfree-list
	if(xFreeListHead == NULL){
		setNext(xk, xk);
		setPrev(xk, xk);
		xFreeListHead = xk;
	}
	else{
		xiaokuai* pv = (xiaokuai*)getPrev((dakuai*)xFreeListHead), *nx = xFreeListHead;
		setNext(xk, nx); setNext(pv, xk);
		setPrev(nx, xk); setPrev(xk, pv);
		xFreeListHead = xk;
	}
}

/* rounds up to the nearest multiple of ALIGNMENT */
static size_t align(size_t x) {
    return ALIGNMENT * ((x+ALIGNMENT-1)/ALIGNMENT);
}

//initialize the heap.
//return NULL if sbrk fails.
//if success, heapBeg and heapEnd are set, which are the lower and upper bounds
//of the heap.
//the heap is initialized to a single free dk with size [xinKuaiSize].
bool mm_init(void) {
	//printf("Begin initing...\n\n");
	kuai* beg;
	if((beg = (kuai*)(mem_sbrk(16))) == (void *)-1){
		//printf("1******\n");
		return false;
	}
	heapBeg = beg + 1;
	tianHF(beg, 0, true);
	kuai* ext;
	if((ext = (kuai*)(mem_sbrk(xinKuaiSize))) == (void *)-1){
		//printf("2******\n");
		return false;
	}
	freeListHead[segNum-1] = (dakuai*)heapBeg;
	heapEnd = (kuai*)(((char*)heapBeg)+xinKuaiSize);
	tianHFR(freeListHead[segNum-1], xinKuaiSize, false);
	setPrevMalloced(freeListHead[segNum-1], true);
	setPrevMalloced(getHeapNext(freeListHead[segNum-1]), false);
	tianHF(heapEnd, 0, true);
	freeListHead[segNum-1]->prev = freeListHead[segNum-1];
	freeListHead[segNum-1]->next = freeListHead[segNum-1];
	xFreeListHead = NULL;
	for(int i = 0; i < segNum-1; i++) freeListHead[i] = 0;
	return true;
}


static void printFreeList(){
	//print the first 32 free block address, to check if dead loop
	printf("Begin printing free list:\n");
	int thres = 1024;
	dakuai* dk = freeListHead[segNum-1];
	if(dk == NULL) {
		printf("full\n");
		return;
	}
	while(thres){
		thres--;
		printf("pos: %p size: %d", dk, (int)getSize(dk));
		dk = dk->next;
		if(dk == freeListHead[segNum-1]) break;
	}
	printf("\n");
}

static void xPrintFreeList(){
	printf("Begin printing x free list:\n");
	int thres = 1024;
	dakuai* dk = (dakuai*)xFreeListHead;
	if(dk == NULL) {
		printf("xfull\n");
		return;
	}
	while(thres){
		thres--;
		printf("pos: %p size: %d", dk, (int)getSize(dk));
		dk = getNext(dk);
		if(dk == (dakuai*)xFreeListHead) break;
	}
	printf("\n");
}

static void printReverseFreeList(){
	//print the reverse free list
	printf("Begin printing reverse free list:\n");
	int thres = 1024;
	dakuai* dk = freeListHead[segNum-1];
	if(dk == NULL) {
		printf("full\n");
		return;
	}
	while(thres){
		thres--;
		printf("pos: %p size: %d", dk, (int)getSize(dk));
		dk = dk->prev;
		if(dk == freeListHead[segNum-1]) break;
	}
	printf("\n");
}
static void xPrintReverseFreeList(){
	//print the x reverse free list
	printf("Begin printing x reverse free list:\n");
	int thres = 1024;
	dakuai* dk = (dakuai*)xFreeListHead;
	if(dk == NULL) {
		printf("xfull\n");
		return;
	}
	while(thres){
		thres--;
		printf("pos: %p size: %d", dk, (int)getSize(dk));
		dk = getPrev(dk);
		if(dk == (dakuai*)xFreeListHead) break;
	}
	printf("\n");
}
//finding a fit to size sz using first-k-fit, where k = findThres (defaultly set to 8)
//if not found, return NULL; otherwise return the start address of the block
//when it returns, [status] will contain an indicator
//it is 1 if and only if it returns a xk; otherwise it is ZERO

dakuai* findFit(size_t sz, int *status){
	if(mallocPrint) printf("finding block with at least size %d...\n", (int)sz);
	//if sz is a small block, find it
	if(sz == 16){
		//find a small block
		if(xFreeListHead != NULL){
			//if there is small block, set status to 1 as a flag
			*status = 1;
			if(mallocPrint) printf("find a small block.\n");
			return (dakuai*)xFreeListHead;
		}
	}
	*status = 0;
	int idx = getFreeListIndex(sz);
	dakuai* result = NULL;
	int find = 0;
	dakuai *start;
	if(freeListHead[idx] != NULL){
		start = freeListHead[idx];
		size_t curSize;
		while(1){
			size_t cSize = getSize(start);
			if(find > findThres) break;
			if(cSize >= sz){
				if(!find){
					curSize = cSize;
					result = start;
				}
				else{
					if(cSize < curSize){
						curSize = cSize;
						result = start;
					}
				}
				find++;
			}
			start = start->next;
			if(start == freeListHead[idx]) break;
		}
	}
	if(find) return result;
	int largeIdx = idx+1;
	if(largeIdx >= segNum) return NULL;
	for(; largeIdx < segNum; largeIdx++){
		if(freeListHead[largeIdx] != NULL) break;
	}
	if(largeIdx == segNum) return NULL;
	start = freeListHead[largeIdx];
	size_t curSize;
	while(1){
		size_t cSize = getSize(start);
		if(find > findThres) break;
		if(!find){
			curSize = cSize;
			result = start;
		}
		else if(cSize < curSize){
			curSize = cSize;
			result = start;
		}
		find++;
		start = start->next;
		if(start == freeListHead[largeIdx]) break;
	}
	return result;
	
}

void deleteFromFreeList(dakuai* dk){
	//delete a dk from the freelist.
	//we can determine the index of the FL from dk's header.
	if(isSmallBlock(dk)){
		xDeleteFromFreeList((xiaokuai*)dk);
		return;
	}
	int idx = getFreeListIndex(getSize(dk));
	if(onlyOneFree(idx)){
		freeListHead[idx] = NULL;
	}
	else{
		if(dk == freeListHead[idx]) freeListHead[idx] = dk->next;
		dk->next->prev = dk->prev;
		dk->prev->next = dk->next;
	}
}


void addToFreeList(dakuai* dk){
	//add a block to FL.
	//determine the FL to add by reading its header
	if(isSmallBlock(dk)){
		xAddToFreeList((xiaokuai*)dk);
		return;
	}
	int idx = getFreeListIndex(getSize(dk));
	if(freeListHead[idx] == NULL){
		dk->prev = dk;
		dk->next = dk;
		freeListHead[idx] = dk;
	}
	else{
		dakuai *pv = freeListHead[idx]->prev, *nx = freeListHead[idx];
		dk->prev = pv, dk->next = nx;
		pv->next = dk, nx->prev = dk;
		freeListHead[idx] = dk;
	}
}

//coalesce:
//must ensure that freeListHead != NULL, i.e. there are free space before freeing
//the current dk
//coalesce with the heap-prev and heap-next dakuai, and return the new dk
//note that after coalesceing, the block would always become a dk
dakuai* coalesce(dakuai* dk){
	dakuai* retVal = NULL;//the new dakuai to be returned
	dakuai* hPrev = getHeapPrev(dk);
	dakuai* hNext = getHeapNext(dk);
	if(coalescePrint) {
		printf("hPrev: %p, hNext: %p\n", hPrev, hNext);
		printf("heap begin: %p, end: %p\n", heapBeg, heapEnd);
		printf("previous one is malloced? %d\n", isPrevMalloced(dk));
	}
	retVal = (hPrev == NULL)? dk: hPrev;
	//if the previous one is malloced or out of bound, the coalesced block would be dk;
	//else it is the start address of the previous block
	if(hPrev==NULL && !isFree(hNext)){
		if(coalescePrint){
		printf("coalescing %p: case 0\n", dk);
		printf("free list before coalescing:\n");
		printFreeList();
		printReverseFreeList();}
		//case 0: an isolated free block
		if(isSmallBlock(dk)) setSmallBlock(dk);
		addToFreeList(dk);
		
		if(coalescePrint){
		printf("free list after coalescing:\n");
		printFreeList();
		printReverseFreeList();}
	}
	else if(hPrev==NULL && isFree(hNext)){
		if(coalescePrint){
		printf("coalescing %p: case 1\n", dk);
		printf("free list before coalescing:\n");
		printFreeList();
		printReverseFreeList();}
		//case 1: only coalesce with the next one in heap
		//set header and footer
		size_t newSz = getSize(dk) + getSize(hNext);
		
		deleteFromFreeList(hNext);
		tianHFR(dk, newSz, false);
		
		setBigBlock(dk);
		
		addToFreeList(dk);
		
		
		if(coalescePrint){
		printf("free list after coalescing:\n");
		printFreeList();
		printReverseFreeList();}
	}
	else if(hPrev!=NULL && !isFree(hNext)){
		if(coalescePrint){
		printf("coalescing %p: case 2\n", dk);
		printf("free list before coalescing:\n");
		printFreeList();
		printReverseFreeList();}
		//case 2: only coalesce with the previous one in heap
		//set heaeder and footer
		size_t newSz = getSize(dk) + getSize(hPrev);
		deleteFromFreeList(hPrev);
		tianHFR(hPrev, newSz, false);
		setBigBlock(hPrev);
		addToFreeList(hPrev);
		
		if(coalescePrint){
		printf("free list after coalescing:\n");
		printFreeList();
		printReverseFreeList();}
	}
	else if(hPrev != NULL && isFree(hNext)){
		if(coalescePrint){
		printf("coalescing %p: case 3\n", dk);
		printf("free list before coalescing:\n");
		printFreeList();
		printReverseFreeList();}
		//case 3: must coalesce with both prev and next
		//set header and footer
		size_t newSz = getSize(dk) + getSize(hPrev) + getSize(hNext);
		
		deleteFromFreeList(hPrev);
		deleteFromFreeList(hNext);
		
		tianHFR(hPrev, newSz, false);
		
		setBigBlock(hPrev);
		
		
		
		addToFreeList(hPrev);
		
		
		if(coalescePrint){
		printf("free list after coalescing:\n");
		printFreeList();
		printReverseFreeList();}
	}
	return retVal;
}



//mallocing:
//if size<=8, we first check if there is xk, and allocate to it if there is
//otherwise we find a dk to fit the size
//if no fit, sbrk it
void *malloc (size_t size) {
	if(mallocPrint){
	printf("\n\nBegin Mallocing %u:\n\n", (unsigned int)size);
	printf("Free Lists:\n");
	printFreeList();
	printReverseFreeList();
	xPrintFreeList();
	xPrintReverseFreeList();}
	//printf("1\n");
	if(size == 0) return NULL;
	if(heapBeg == NULL) {
		if(!mm_init()) return NULL;
	}
	
	size_t sz = getRoundSize(size+8);
	//if(sz < 32) sz = 32;
	//printf("size=%u\n", (unsigned int)sz);
	int status;
	dakuai* fit = findFit(sz, &status);
	//printf("status: %d", status);
	//printf("find fit value: %p\n", fit);
	void* retVal = NULL;
	if(status == 1){
		//printf("never reached here");
		//if we find a small block
		//now sz must be 16
		if(mallocPrint) printf("Small Fit!\n");
		
		xiaokuai* xFit = (xiaokuai*) fit;
		xDeleteFromFreeList(xFit);
		//printf("Small Fit!\n");
		//xPrintFreeList();
		retVal = (void*)(((char*)fit)+8);
		setMalloced(xFit);
		setPrevMalloced(getHeapNext(fit), true);
		return retVal;
	}
	if(fit == NULL){
		size_t brkSize = (xinKuaiSize > sz)? xinKuaiSize: sz;
		void* ext = mem_sbrk(brkSize);
		if(ext == (void*)-1) return NULL;
		//printf("sbrk success, with size: %u\n", (unsigned int)brkSize);
		heapEnd = (kuai*)(((char*)heapEnd)+brkSize);
		dakuai* newDK = (dakuai*)(((char*)ext)-8);
		tianHFR(newDK, brkSize, false);
		tianHF(heapEnd, 0, true);
		setPrevMalloced(getHeapNext(newDK), false);
		//printFreeList();
		//printReverseFreeList();	
		if(freeListHead[7] == NULL){
			//printf("Empty\n");
			freeListHead[7] = newDK;
			newDK->prev = newDK;
			newDK->next = newDK;
		}
		else{
			//printf("Need coalesce\n");
			newDK = coalesce(newDK);
		}
		//printf("Can it reach here?\n");
		//return malloc(sz);
		fit = newDK;
	}
	//now fit is pointing to the block we have to allocate!
	retVal = (void*)(((char*)fit)+8);
	size_t fitSize = getSize(fit);
	size_t leftSize = fitSize - sz;
	//if(leftSize < 32) {
	//	sz = fitSize;
	//	leftSize = 0;
	//}//this code block must be deleted after smallblock mode enabled.
	if(sz >= 32){
		//allocate a dk
		deleteFromFreeList(fit);
		tianHFR(fit, sz, true);
		dakuai* newDK = getHeapNext(fit);
		setPrevMalloced(newDK, true);
		if(leftSize >= 32){
			tianHFR(newDK, fitSize-sz, false);
			addToFreeList(newDK);
		}
		else if(leftSize == 16){
			setSmallBlock(newDK);
			xiaokuai* newXK = (xiaokuai*)newDK;
			setFree(newXK);
			xAddToFreeList(newXK);
			//xPrintFreeList();
		}
	}
	else{
		//allocate a xk
		
		deleteFromFreeList(fit);
		
		setSmallBlock(fit);
		
		xiaokuai* mallocedXK = (xiaokuai*)fit;
		setMalloced(mallocedXK);
		
		dakuai* newDK = getHeapNext(fit);
		setPrevMalloced(newDK, true);
		
		if(leftSize == 16){
			setSmallBlock(newDK);
			xiaokuai* newXK = (xiaokuai*)newDK;
			setFree(newXK);
			xAddToFreeList(newXK);
			
		}
		else{
			tianHFR(newDK, leftSize, false);
			addToFreeList(newDK);
		}
		
	}
	return retVal;
}

//free:
//free the specified block. fill in the header/footer and add it to FL's.
void free (void *ptr) {
	if(ptr == NULL) return;
	dakuai* realFree = (dakuai*) (((char*)ptr)-8);
	if(mallocPrint) printf("\n\nBegin Freeing: %p\n\n", realFree);
	size_t sz = getSize(realFree);
	//printf("the size to be freed: %u\n",(unsigned int)sz);
	if(sz >= 32){
		//free a dk
		tianHFR(realFree, sz, false);
		setPrevMalloced(getHeapNext(realFree), false);
		
		realFree = coalesce(realFree);
		if(mallocPrint) printf("after free: %p\n", realFree);
	
	}
	else{
		//free a xk
		xiaokuai* freeXK = (xiaokuai*) realFree;
		setFree(freeXK);
		setPrevMalloced(getHeapNext(realFree), false);
		realFree = coalesce(realFree);
	}
    	return;
}

/*
 * realloc
 */
void *realloc(void *oldptr, size_t size) {
	if(oldptr == NULL) return malloc(size);
	if(size == 0){
		free(oldptr);
		return NULL;
	}
	void *newPtr; 
	if((newPtr = malloc(size)) == NULL){
		return NULL;
	}
	dakuai* oldKuai = (dakuai*)(((char*)oldptr)-8);
	size_t oldSize = (size_t)(getSize(oldKuai)-8);
	if(size > oldSize) size = oldSize;
	memcpy(newPtr, oldptr, size);
	free(oldptr);
	return newPtr;
}

/*
 * calloc
 * This function is not tested by mdriver
 */
void *calloc(size_t nmemb, size_t size){
    void *bp;
    size_t asize = nmemb * size;

    if (asize/nmemb != size)
    // Multiplication overflowed
    return NULL;
    
    bp = malloc(asize);
    if (bp == NULL)
    {
        return NULL;
    }
    // Initialize all bits to 0
    memset(bp, 0, asize);

    return bp;
}


/*
 * Return whether the pointer is in the heap.
 * May be useful for debugging.
 */
static bool in_heap(const void *p) {
    return p <= mem_heap_hi() && p >= mem_heap_lo();
}

/*
 * Return whether the pointer is aligned.
 * May be useful for debugging.
 */
static bool aligned(const void *p) {
    size_t ip = (size_t) p;
    return align(ip) == ip;
}

/*
 * mm_checkheap
 */
bool mm_checkheap(int lineno) {
	xiaokuai *freeXK = xFreeListHead;
	if(freeXK == NULL) goto chaDK;
	while(1){
		if(!in_heap((void*)freeXK)){
			printf("Line %d: XK %p out of bound!\n", lineno, freeXK);
			return false;
		}
		if(isMalloced((dakuai*)freeXK)){
			printf("Line %d: XK %p is malloced, but still in FL!\n", lineno, freeXK);
			return false;
		}
		if(getSize((dakuai*)freeXK) != 16){
			printf("Line %d: XK %p 's header/footer corrupted!\n", lineno, freeXK);
			return false;
		}
		if(!isSmallBlock((dakuai*)freeXK)){
			printf("Line %d: XK %p is actually a DK!\n", lineno, freeXK);
			return false;
		}
		if(getNext(getPrev((dakuai*)freeXK)) != getPrev(getNext((dakuai*)freeXK))){
			printf("Line %d: XK %p 's prev/next pointers not consistent!\n", lineno, freeXK);
			return false;
		}
		freeXK = (xiaokuai*)getNext((dakuai*)freeXK);
	}
	chaDK:
	for(int i = 0; i < segNum; i++){
		if(freeListHead[i] == NULL) continue;
		dakuai *freeDK = freeListHead[i];
		while(1){
			if(!in_heap((void*)freeDK)){
				printf("Line %d: DK %p out of bound!\n", lineno, freeDK);
				return false;
			}
			if(isMalloced(freeDK)){
				printf("Line %d: DK %p is malloced but still in FL!\n", lineno, freeDK);
				return false;		
			}
			if(getSize(freeDK) < 32){
				printf("Line %d: DK %p 's size is: %d, which is too small!\n", lineno, freeDK, (int)getSize(freeDK));
				return false;
			}
			if(isSmallBlock(freeDK)){
				printf("Line %d: DK %p is actually a XK!\n", lineno, freeDK);
				return false;
			}
			if(getFreeListIndex(getSize(freeDK)) != i){
				printf("Line %d: DK %p 's size %d does not match the %d-th free list!\n", lineno, freeDK, (int)getSize(freeDK), i);
				return false;
			}
			if(getNext(getPrev(freeDK)) != getPrev(getNext(freeDK))){
				printf("Line %d: DK %p 's prev/next pointers not consistent!\n", lineno, freeDK);
				return false;
			}
		}
	}
    	return true;
}

