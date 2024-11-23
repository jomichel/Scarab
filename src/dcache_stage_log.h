 
#pragma once
#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>



typedef enum  {
    NO_EVICTION,
    COMPULSORY,
    CAPACITY,
    CONFLICT
}EvictType;


typedef struct CCacheStateOBJ *CCacheState;
void dcacheLogInit(CCacheState state);
EvictType dcacheLogInsert(CCacheState state, uint64_t address, uint64_t cacheLine, uint64_t cacheLine2,uint64_t cacheSize);
void dcacheLogEvictCacheLine(CCacheState  state, uint64_t cacheLine, uint64_t cacheSize);
uint64_t * dcacheLogGetEvictions(CCacheState state); 
 char * dcacheLogGetEvictionsString(CCacheState state);
 

#ifdef __cplusplus
}
#endif

