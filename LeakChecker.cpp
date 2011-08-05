#include "LeakChecker.h"
#include "stdio.h"
#include "stdlib.h"
#include <pthread.h>
#include <assert.h>
#include <vector>
#include <set>
#include <list>
#include <algorithm>
#include <execinfo.h>
#include <cxxabi.h>

//#include <QtCore/qDebug>

//#include "//qDebugTrace.h"

#define MAX_STACKSIZE 20
#define MAX__SYMBOL_SIZE 200
//#define LEAK_REPORT_FILE_NAME "/var/opt/bosch/dynamic/media/LOG/leak.log"
#define LEAK_REPORT_FILE_NAME "/home/sunny/CodeTest/log/leak.log"
//IMPLEMENT_//qDebug_OBJECT(LeakChecker,"LeakChecker.txt");

using namespace std;

struct MemInfo{
        int iSize;
        void* pAddress;
        void* stackTrace[MAX_STACKSIZE];
        int iStackSize;
        bool bIsArray;
        MemInfo* pNext;
        bool bProcessed;
        int iTotalFound;


};

void InitMemInfo(MemInfo *pMemInfo) {
    pMemInfo->pNext = 0;
    pMemInfo->bProcessed = false;
    pMemInfo->iTotalFound = 1;
}

bool operator < (const MemInfo& first, const MemInfo& other) {
    return (first.iTotalFound * first.iSize) > (other.iTotalFound * other.iSize);
}

pthread_mutex_t mutexNew;
MemInfo *pNewListStart=NULL;
unsigned int iTotalNewDone = 0;
unsigned int iTotalFreeDone = 0;
unsigned int iTotalNewDeleteMismatch = 0;
unsigned int iTotalFreeMismatch = 0;
LeakChecker LeakChecker::m_globalLeakChecker;
int iTotalUniqueMemInfo = 0;
int iTotalSizeofLeaksFound = 0;

void CreateAndSortVector(MemInfo *pLocalNewListStart, vector<MemInfo> &uniqueVector) {
    pLocalNewListStart = pLocalNewListStart->pNext;

    uniqueVector.resize(iTotalUniqueMemInfo);

    for(int i=0;pLocalNewListStart; pLocalNewListStart = pLocalNewListStart->pNext) {
        if(pLocalNewListStart->bProcessed)
            continue;
        uniqueVector[i++] = *pLocalNewListStart;

        iTotalSizeofLeaksFound += pLocalNewListStart->iTotalFound * pLocalNewListStart->iSize;
    }

    sort<vector<MemInfo>::iterator> ( uniqueVector.begin(), uniqueVector.end() );
}

void Aggregate(MemInfo *pLocalNewListStart) {
    pLocalNewListStart = pLocalNewListStart->pNext;
    for(;pLocalNewListStart; pLocalNewListStart = pLocalNewListStart->pNext) {
        if(pLocalNewListStart->bProcessed)
            continue;
        iTotalUniqueMemInfo++;
        for(MemInfo *pCurrent = pLocalNewListStart->pNext; pCurrent; pCurrent = pCurrent->pNext) {
            if(pCurrent->bProcessed)
                continue;

            bool bEqual = true;
            if(pLocalNewListStart->iStackSize != pCurrent->iStackSize) {
                bEqual = false;
            } else {
                for(int i=0; i<pLocalNewListStart->iStackSize; i++) {
                    if(pCurrent->stackTrace[i] != pLocalNewListStart->stackTrace[i]) {
                        bEqual = false;
                        break;
                    }
                }
            }
            if(bEqual) {
                pLocalNewListStart->iTotalFound++;
                pCurrent->bProcessed = true;
            }
        }
    }
}



void* Allocate(int size, bool bIsArray) {

    void *p=malloc(size);

    pthread_mutex_lock (&mutexNew);

    if(pNewListStart) {
        MemInfo *pMemInfoNode = (MemInfo*) malloc(sizeof(MemInfo));        
        InitMemInfo(pMemInfoNode);
        pMemInfoNode->iSize = size;
        pMemInfoNode->pAddress = p;
        pMemInfoNode->iStackSize = backtrace (pMemInfoNode->stackTrace, MAX_STACKSIZE);
        pMemInfoNode->bIsArray = bIsArray;

        pMemInfoNode->pNext = pNewListStart->pNext ;
        pNewListStart->pNext = pMemInfoNode;
        iTotalNewDone++;
    }

    pthread_mutex_unlock(&mutexNew);

    return p;
}

bool DeleteFromNewList(void *p, bool bIsArray) {
    if(p == NULL) {
        return true;
    }

    MemInfo *pPrev = pNewListStart;
    MemInfo *pLocalNewListStart = pNewListStart->pNext;
    while(pLocalNewListStart) {
        if(pLocalNewListStart->pAddress == p) {
            if(pLocalNewListStart->bIsArray != bIsArray) {
                iTotalNewDeleteMismatch++;
                //report New/Delete mismatch
            }
            pPrev->pNext = pLocalNewListStart->pNext;
            free(pLocalNewListStart);
          return true;
        }
        pPrev = pLocalNewListStart;
        pLocalNewListStart = pLocalNewListStart->pNext;
    }
    return false;
}

void Deallocate(void *p, bool bIsArray) {
    pthread_mutex_lock (&mutexNew);

    if(pNewListStart) {
        bool bSuccess = DeleteFromNewList(p, bIsArray);
        if(!bSuccess) {
            iTotalFreeMismatch++;
            // Report Deallocate Error
            // Return from here, as It will give crash to the user.
        } else {
            iTotalFreeDone++;
        }
    }

    pthread_mutex_unlock(&mutexNew);
    free(p);
}

void* operator new (size_t size) {
    return Allocate(size, false);
}

void* operator new[] (size_t size) {
    return Allocate(size, true);
}

void operator delete[](void *p) {
    Deallocate(p, true);
}


void operator delete (void *p) {
    Deallocate(p, false);
}


void freeAllTheMemory(MemInfo *pLocalNewListStart) {
    MemInfo* pNode = pLocalNewListStart->pNext;

    while(pNode) {
        MemInfo *pCurrent = pNode;
        pNode = pNode->pNext;
        free(pCurrent);
    }    
}

void getMangledSymbolName(char* strSymbolName, int *iSymbolStart , int *iSymbolLength,
                          int *iAddressStart, int *iAddressLength) {
    *iSymbolStart = *iSymbolLength =  *iAddressStart = *iAddressLength = 0;

    for(int i=0; strSymbolName[i] != '\0'; i++) {
        if(strSymbolName[i] == '(') {
            strSymbolName[i] = 0;
            *iSymbolStart = i+1;
        } else if( strSymbolName[i] == '+' ) {
            if(*iSymbolStart != 0) {
                *iSymbolLength = i - *iSymbolStart;
                strSymbolName[i] = 0;
                continue;
            }
        } else if ( strSymbolName[i] == '[') {
            *iAddressStart = i;
        } else if(strSymbolName[i] == ']') {
            if(*iAddressStart != 0) {
                *iAddressLength = i - *iAddressStart;
            }
        }

    }
}



void printOutput(vector<MemInfo> &uniqueVector) {

    //qDebug("Start void printOutput MemInfo *pLocalNewStart %d", iTotalLeaksFound);
    FILE *fp = fopen(LEAK_REPORT_FILE_NAME, "w");
    if(!fp) {
        //qDebug("unable to open file");
        return;
    } else {
        //qDebug("File Open Done");
    }

    int iTotalLeaksFound = iTotalNewDone - iTotalFreeDone;
    fprintf(fp,"iTotalSizeofLeaksFound=%0.2f KBytes TotalLeaksFound =%d  iTotalNewDone = %d iTotalFreeDone=%d iTotalNewDeleteMismatch=%d iTotalFreeMismatch=%d\n\n",
            iTotalSizeofLeaksFound/1000.0, iTotalLeaksFound, iTotalNewDone, iTotalFreeDone, iTotalNewDeleteMismatch, iTotalFreeMismatch);
    fflush(fp);

    int iCurrentCount = 0;
    for(unsigned int iVectorCount = 0; iVectorCount<uniqueVector.size(); iVectorCount++) {
        fprintf(fp,"---------------------------------------------------------------------- \n");
            MemInfo* pLocalNewStart = &uniqueVector[iVectorCount];
            char** strings = backtrace_symbols(pLocalNewStart->stackTrace, pLocalNewStart->iStackSize);
            fprintf(fp,"Occurance=%d TotalLeakSize=%d Bytes LeakSize = %d Bytes Pointer = %p %d outof %d \n",
                    pLocalNewStart->iTotalFound, pLocalNewStart->iSize * pLocalNewStart->iTotalFound,
                    pLocalNewStart->iSize, pLocalNewStart->pAddress, iVectorCount+1, uniqueVector.size());
            iCurrentCount++;
            //qDebug("Size = %d Pointer = %p %d outof %d \n", pLocalNewStart->iSize, pLocalNewStart->pAddress, iCurrentCount, iTotalLeaksFound);
            for(int i=2; i<pLocalNewStart->iStackSize; i++) {
                //printf("%s\n", strings[i]);
                //fflush(stdout);
                char * strName = strings[i];
                //fprintf(fp, "%s\n", strName);
                int iSymbolStart, iSymbolLength, iAddressStart, iAddressLength;
                getMangledSymbolName(strName, &iSymbolStart, &iSymbolLength,
                                     &iAddressStart, &iAddressLength);
                if(iSymbolStart != 0) {
                    fprintf(fp,"%s", strName);
                }
                if(iSymbolLength == 0) {
                    fprintf(fp," [No Symbol Availale]");
                } else {
                    char *strDemangledSymbol = (char*) malloc(MAX__SYMBOL_SIZE);
                    size_t iDemangledSymbolSize = MAX__SYMBOL_SIZE;
                    int iStatus = 0;
                    char* ret = abi::__cxa_demangle(&strName[iSymbolStart],
                                     strDemangledSymbol, &iDemangledSymbolSize, &iStatus);
                    strDemangledSymbol = ret;
                    if(iStatus == 0) {
                        fprintf(fp," %s", strDemangledSymbol);
                    } else {
                        fprintf(fp," %s", "[Symbol Error]");
                    }
                    free(strDemangledSymbol);
                }
                if(iAddressStart != 0) {
                    fprintf(fp," %s\n", &strName[iAddressStart]);
                } else {
                    fprintf(fp,"[No Address Available]\n");
                }
            }
            free(strings);
            fprintf(fp,"\n");        
            fflush(fp);
            //pLocalNewStart = pLocalNewStart->pNext;
    }
    fprintf(fp,"---------------------------------------------------------------------- \n");
    fclose(fp);
    //qDebug("End of printOutput(MemInfo *pLocalNewStart)");
}

bool findLeaks() {
    MemInfo *pLocalNewStart = NULL;

    pthread_mutex_lock (&mutexNew);

    if(!pNewListStart) {        
        pthread_mutex_unlock(&mutexNew);
        //qDebug("Returning from findLeaks()");
        return false;
    }

    pLocalNewStart = pNewListStart;
    pNewListStart = NULL;

    pthread_mutex_unlock(&mutexNew);

    Aggregate(pLocalNewStart);
    vector<MemInfo> uniqueVector;
    CreateAndSortVector(pLocalNewStart,uniqueVector);

    printOutput(uniqueVector);

    //qDebug("Before Deleting all the memory");

    freeAllTheMemory(pLocalNewStart);
    free(pLocalNewStart);

    return true;
}

void initLeakChecker() {
    pthread_mutex_init(&mutexNew, NULL);
    pNewListStart = (MemInfo*)malloc(sizeof(MemInfo));
    InitMemInfo(pNewListStart);
}


LeakChecker::LeakChecker() {
        //INIT_//qDebug_FILE;
    initLeakChecker();
}

void LeakChecker::generateLeakReport() {
        //qDebug("Start generateLeakReport()");
    findLeaks();
    //qDebug("Stop generateLeakReport()");
}

LeakChecker::~LeakChecker() {
        //KILL_//qDebug_OBJECT;
    findLeaks();
}
