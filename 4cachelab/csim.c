#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include "cachelab.h"

//extern void printSummary(long hits, long misses, long evictions);

long P2[64];//powers of 2

int s, b;
long S, E, B;
long hit, mis, evi, counter;

struct cacheLine{
	//one cache line
	int validBit;
	unsigned long timeUsed;
	unsigned long tagAddr;
};

struct cacheLine *cacheLines;

void initP2(){
	P2[0] = 1;
	for(int i = 1; i < 64; i++){
		P2[i] = P2[i-1] << 1;
	}
}

int init(){
	hit = 0, mis = 0, evi = 0, counter = 0;
	cacheLines = (struct cacheLine*) malloc(S*E*sizeof(struct cacheLine));
	if(cacheLines == NULL){
		return 0;
	}
	for(int i = 0; i < S*E; i++){
		cacheLines[i].validBit = 0;
	}
	return 1;
}

int toDec(char c){
	if(c >= '0' && c <= '9') return (int)(c-'0');
	if(c >= 'A' && c <= 'F') return (int)(c-'A'+10);
	if(c >= 'a' && c <= 'f') return (int)(c-'a'+10);
	return -1;
}

unsigned long getAddr(char *addr){
	//get the address
	int len = strlen(addr);
	for(int i = 0; i < len; i++){
		if(addr[i] == ','){
			addr[i] = '\0';
			break;
		}
	}
	len = strlen(addr);
	unsigned long res = 0;
	for(int i = 0; i < len; i++){
		res = (res << 4);
		res += toDec(addr[i]);
	}
	return res;
}

unsigned long getTag(unsigned long address){
	//get the tag of an address
	return address >> (s+b);
}

unsigned long getSetIdx(unsigned long address){
	//get the set index
	unsigned long mask = (1 << (s+b)) - 1;
	return (address & mask) >> b;
}

void update(unsigned long tag, unsigned long setIdx){
	counter++;
	int isHit = 0;
	int emptyPos = -1;
	unsigned long offset = setIdx*E;
	unsigned long leastRecent = counter+1;//record the least recent time value
	int argLeastRecent = -1;//the index of line of least recent used block
	for(int j = 0; j < E; j++){
		if(cacheLines[offset+j].validBit == 0){
			emptyPos = j;
		}
		//record an empty position, -1 if non exist
		else{
			if(cacheLines[offset+j].tagAddr == tag){
				//there is a hit
				isHit = 1;
				hit++;
				cacheLines[offset+j].timeUsed = counter;
				break;
			}
			if(cacheLines[offset+j].timeUsed < leastRecent){
				leastRecent = cacheLines[offset+j].timeUsed;
				argLeastRecent = j; 
			}
		}
	}
	if(isHit){
		return;//there is a hit
	}
	mis++;//there is a miss
	if(emptyPos != -1){
		//there is an empty position to put, no eviction
		cacheLines[offset+emptyPos].tagAddr = tag;
		cacheLines[offset+emptyPos].timeUsed = counter;
		cacheLines[offset+emptyPos].validBit = 1;
	}
	else{
		//no empty position, must have eviction
		evi++;
		if(argLeastRecent == -1){
			printf("Unknown bug.\n");//this cannot happen!
		}
		else{
			cacheLines[offset+argLeastRecent].tagAddr = tag;
			cacheLines[offset+argLeastRecent].timeUsed = counter;
		}
	}
}

int main(int argc, char **argv){
	if(argc < 9){
		printf("argument error\n");
		return 0;
	}
	initP2();
	s = atoi(argv[2]);
	E = atol(argv[4]);
	b = atoi(argv[6]);
	S = P2[s], B = P2[b];
	if(!init()){
		printf("memory allocation error\n");
		return 0;
	}
	FILE *trc;
	trc = fopen(argv[8], "rw");
	if(trc == NULL){
		printf("file opening error\n");
		return 0;
	}
	char op[4], addr[32];
	while(fscanf(trc, "%s%s", op, addr) == 2){
		unsigned long address = getAddr(addr);
		unsigned long tag = getTag(address), setIdx = getSetIdx(address);
		update(tag, setIdx);
	}
	printSummary(hit, mis, evi);
	fclose(trc);
	free(cacheLines);
	return 0;
}
