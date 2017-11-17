#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

struct cc{
	//a struct for cache.
	//it records the source of the cache: host, path and port number
	//and also its size, timestamp
	//content field is its real body.
	//prev and next pointer fields are used in maintaining the list.

	char host[MAXBUF];
	char path[MAXBUF];
	int port;
	int size;
	unsigned long timeStamp;
	char content[MAX_OBJECT_SIZE];
	struct cc *prev, *next;

};

typedef struct cc cache;

//the head of cache list
cache *cacheHead = NULL;

//usage of cache
int cacheSize = 0;

//the recorded timestamp.
//note that this mechanism requires no more than 2^64 read and write operations in a run
//which is quite realistic!
unsigned long tStmp = 0;

//mutex
static sem_t mtx;

//cache read and write lock
pthread_rwlock_t cacheRWLock;

cache* readCache(char *host, char *path, int port, int connFd){
	//read the cache
	//if there is a hit, write it to connFd
	//if hit: return the pointer to the cache object
	//otherwise, return NULL

	//first use a read lock
	pthread_rwlock_rdlock(&cacheRWLock);

	cache *thisCache = cacheHead;
	while(thisCache != NULL){
		if(strcmp(thisCache->host, host)==0 && strcmp(thisCache->path, path)==0 && thisCache->port == port){
			//if we have an identical record
			break;
		}
		thisCache = thisCache->next;
	}

	if(thisCache != NULL){
		//if we find the object cached
		//write it to connFd
		Rio_writen(connFd, thisCache->content, thisCache->size);
	}

	//unlock
	pthread_rwlock_unlock(&cacheRWLock);

	return thisCache;
}


void updateCache(cache *thisCache){
	//when a cache is read, update the timestamp for it
	//must use a write lock

	//ill-formed call
	if(thisCache == NULL){
		return;
	}

	//write lock
	pthread_rwlock_wrlock(&cacheRWLock);

	//update its timestamp
	thisCache->timeStamp = tStmp;
	tStmp++;

	//unlock
	pthread_rwlock_unlock(&cacheRWLock);
}

void deleteLRU(){
	//use LRU policy to delete a cache object from cache list, according
	//to their timestamps
	//and update the total cache size
	//if no cache in list, do nothing
	//we assume that when entering this function, there is already a write lock
	//it should only be used in addToCache()

	//if the list is empty, just ignore
	if(cacheHead == NULL){
		return;
	}

	//find the victim, i.e. the object with the earliest timestamp
	cache *toDel = cacheHead;
	cache *iter = cacheHead;
	long tstmp = cacheHead->timeStamp;
	while(iter != NULL){
		if(iter->timeStamp < tstmp){
			tstmp = iter->timeStamp;
			toDel = iter;
		}
		iter = iter->next;
	}

	//the victim is *toDel at this moment
	//first update cache size
	cacheSize -= toDel->size;

	//delete toDel and free it
	if(toDel == cacheHead){
		cacheHead = toDel->next;
	}
	if(toDel->next != NULL){
		toDel->next->prev = toDel->prev;
	}
	if(toDel->prev != NULL){
		toDel->prev->next = toDel->next;
	}
	free(toDel);
}

void addToCache(char *host, char *path, int port, char *response, int readLen){
	//add a new object to cache

	//if bad form or too long, just ignore
	//in fact this cannot happen, we explicitly check it before calling this func
	if(response == NULL || readLen > MAX_OBJECT_SIZE){
		return;
	}

	//prepare the cache object
	//except the timestamp field, which requires a lock
	cache *newCache = (cache*)malloc(sizeof(cache));
	strcpy(newCache->host, host);
	strcpy(newCache->path, path);
	newCache->port = port;
	newCache->size = readLen;
	int i;
	for(i = 0; i < readLen; i++){
		newCache->content[i] = response[i];
	}

	//begin our transaction, first write lock
	pthread_rwlock_wrlock(&cacheRWLock);

	//evict older blocks to fit the size
	while(cacheSize > MAX_CACHE_SIZE - readLen){
		deleteLRU(); 
	}

	//set timestamp
	newCache->timeStamp = tStmp;
	tStmp++;
	
	//add to the head of cache list
	newCache->next = cacheHead;
	newCache->prev = NULL;
	if(cacheHead != NULL){
		cacheHead->prev = newCache;
	}
	cacheHead = newCache;
	cacheSize += readLen;

	//unlock
	pthread_rwlock_unlock(&cacheRWLock);
}

int parseURL(char *URL, char *hostName, char *path){
    //parse the URL: if it is illegal, return -1
    //otherwise return the port number; and parsed host name and path are
    //stored in hostName and path.
    //must ensure that hostName and path are initialized with '\0''s

    int port = 80;//the default port

    if(strlen(URL) <= 7){
	//ill-formed URL
	return -1;
    }

    //beginning of real URL
    char *realHost = URL + 7;

    //temporarily set the 7th position to '\0' to call strcmp
    char pos7 = *realHost;
    *realHost = '\0';

    //check length > 7 and first 7 characters
    if(strcmp(URL, "http://") != 0){
	return -1;
    }

    //realHost now points to the start of hostname
    *realHost = pos7;
    int offset = 0;
    int realLen = strlen(realHost);
    while(offset < realLen){
        if(realHost[offset] == ':' || realHost[offset] == '/'){
		//offset is the point at which hostname ends
		break;
	}
        offset ++;
    }

    if(offset == 0){
	return -1;//no host name, illegal
    }
    int i;
    if(offset == realLen){
        //no path, and no port
        for(i = 0; i < realLen; i++){
            hostName[i] = realHost[i];
        }
    }
    else if(realHost[offset] == '/'){
        //has path but no specified port
        for(i = 0; i < offset; i++){
	    hostName[i] = realHost[i];
	}
        for(i = 0; i < realLen - offset; i++){
	    path[i] =  realHost[offset + i];
	}
    }
    else{
        //has a specified port number
	int numberEnd = offset + 1;
        char portChar[MAXBUF] = {'\0'};
	while(numberEnd < realLen && realHost[numberEnd] >= '0' && realHost[numberEnd] <= '9'){
            portChar[numberEnd - offset - 1] = realHost[numberEnd];
	    numberEnd ++;
	}
        if(numberEnd != realLen && realHost[numberEnd] != '/'){
	    //ill-formed after port number
	    return -1;
	}
        port = atoi(portChar);
	for(i = 0; i < offset; i++){
	    hostName[i] = realHost[i];
	}
	for(i = 0; i < realLen - numberEnd; i++){
	    path[i] = realHost[numberEnd + i];
	}
    }
    return port;
}

void* thread(void *vargp){
	//the thread to process a request.
	//vargp contains the integer value of connFd
	//first read the client's request, and parse it
	//check if it is in cache, if it is then simply return it
	//otherwise connect to the server and wait for response
	//then send back the response to client
	
	//detach thread
	Pthread_detach(Pthread_self());

	//convert the vargp to integer form
        int connFd = (int)((long)vargp);

	//write client request to buf
        rio_t rioAsServer;
        Rio_readinitb(&rioAsServer, connFd);
        char buf[MAXBUF];
        Rio_readlineb(&rioAsServer, buf, MAXBUF);
        char method[MAXBUF] = {'\0'}, URL[MAXBUF] = {'\0'};

	//parse buf into method and URL (and everything behind them is unimportant)
        sscanf(buf, "%s%s", method, URL);

	//if not a GET or bad URL
        if(strcmp(method, "GET") != 0 || strlen(URL) <= 7){
		return NULL;
	}

	//get the required port by parsing URL
        char hostName[MAXBUF] = {'\0'}, path[MAXBUF] = {'\0'};
        int sendPort = parseURL(URL, hostName, path);

	//if return -1, meaning bad URL, do nothing
        if(sendPort == -1) return NULL;

	//if path is empty, use a replacement of '/'
	if(strlen(path) == 0){
		path[0] = '/';
	}

	//check if it is already in cache
	cache* thisCache = readCache(hostName, path, sendPort, connFd);
	if(thisCache != NULL){
		//if it is in cache, update its timestamp, and return
		updateCache(thisCache);
		Close(connFd);
		return NULL;
	}

	//if not in cache, initiate a connection to the specified URL
	char sP[MAXBUF] = {'\0'};
	sprintf(sP, "%d", sendPort);
	int clientFd;

	//must lock it
	P(&mtx);
        clientFd = Open_clientfd(hostName, sP);
	V(&mtx);

	//write headers
	char aHeader[MAXBUF] = {'\0'};
	rio_t rioAsClient;
	Rio_readinitb(&rioAsClient, clientFd);
	//GET header
	sprintf(aHeader, "GET %s HTTP/1.0\r\n", path);
	Rio_writen(clientFd, aHeader, strlen(aHeader));
	//HOST header
	sprintf(aHeader, "Host: %s\r\n", hostName);
	Rio_writen(clientFd, aHeader, strlen(aHeader));
	//user agent header
	Rio_writen(clientFd, (char*)user_agent_hdr, strlen(user_agent_hdr));
	//connection and proxy-connection header
	char CONN[256] = "Connection: close\r\n";
	char pCONN[256] = "Proxy-Connection: close\r\n";
	Rio_writen(clientFd, CONN, strlen(CONN));
	Rio_writen(clientFd, pCONN, strlen(pCONN));
	//header ender
	Rio_writen(clientFd, "\r\n", 2);

	//read the response, and determine whether it can be cached
	//we read MAX_OBJECT_SIZE+1 bytes at once
	//so it can be cached if and only if the first read is not full
	char response[MAX_OBJECT_SIZE+1];
	int readLen;
	int canCache = 1;
	while(1){
	    readLen = Rio_readnb(&rioAsClient, response, MAX_OBJECT_SIZE+1);
	    if(readLen > MAX_OBJECT_SIZE){
		//if a read exceeds MAX_OBJECT_SIZE, then it cannot be chched!
		canCache = 0;
	    }
	    if(readLen <= 0){
		//reaches the EOF
		break;
	    }
	    Rio_writen(connFd, response, readLen);
            if(canCache){
		//if can cache, add to cache and return
		addToCache(hostName, path, sendPort, response, readLen);
		break;
	    }
	}
	Close(connFd);
	Close(clientFd);
	return NULL;
}

int main(int argc, char **argv)
{
    //if no port is specified, return
    if(argc < 2){
	printf("Must specify a port number!\n");
	return 0;
    }

    //first initialize locks
    pthread_t tid;
    Sem_init(&mtx, 0, 1);
    pthread_rwlock_init(&cacheRWLock, NULL);
    Signal(SIGPIPE, SIG_IGN);

    //the listening port
    int listenFd = Open_listenfd(argv[1]), connFd;
    
    struct sockaddr_in clientAddr;
    socklen_t clientLen = sizeof(clientAddr);
    
    while(1){

        clientLen = sizeof(clientAddr);
        connFd = Accept(listenFd, (SA*)(&clientAddr), &clientLen);

        //create the actual processing thread
	Pthread_create(&tid, NULL, thread, (void*)((long)connFd));

    }
    return 0;
}
