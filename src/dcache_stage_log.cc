#include "dcache_stage_log.h"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <unordered_map>

enum class EvictionType { NOT_EVICTED, CAPACITY, CONFLICT };
class DcacheLog {
 private:
  std::unordered_map<uint64_t, EvictionType> gcacheMetadata;
  std::unordered_map<uint64_t, EvictionType> cacheMetadata;
  uint64_t                                   cacheSize;
  uint64_t                                   ourCacheSize        = 0;
  uint64_t                                   totalEvictions      = 0;
  uint64_t                                   compulsoryEvictions = 0;
  uint64_t                                   capacityEvictions   = 0;
  uint64_t                                   conflictEvictions   = 0;

 public:
  /**
   * @param address The new address we are trying to put into the hashmap
   * @param cacheLine The cache location we are trying to put it in
   * @param cacheSize max size of cache, used to determine if capacity miss
   * This function should only be called when a miss occurs
   */
  DcacheLog() {}
  DcacheLog(uint64_t CacheSize) : cacheSize(CacheSize) {}
  uint64_t getCapacityEvictions() { return capacityEvictions; }
  uint64_t getConflictEvictions() { return conflictEvictions; }
  uint64_t getCompulsoryEvictions() { return compulsoryEvictions; }

  /**
   * @param address The new address we are trying to put into the hashmap
   * @param cacheLine The cache location we are trying to put it in
   * This function should only be called when a miss occurs
   */
  EvictType cacheInsert(uint64_t address, uint64_t cacheLine,
                        uint64_t cacheLine2, uint64_t cacheSize);
  EvictType evictCacheLine(uint64_t cacheLine, uint64_t cacheSize);
};

typedef struct CCacheStateOBJ {
  DcacheLog* dcacheLog;
} CCacheStateOBJ;

EvictType DcacheLog::evictCacheLine(uint64_t cacheLine, uint64_t cacheSize) {
  auto cacheIt = cacheMetadata.find(cacheLine);
  if(cacheIt == cacheMetadata.end()) {
    std::cout << "cache line " << cacheLine << " is not occupied" << std::endl;
    return NO_EVICTION;
  }

  EvictionType evictionType;
  if(cacheMetadata.size() >= cacheSize) {
    evictionType = EvictionType::CAPACITY;
  } else {
    evictionType = EvictionType::CONFLICT;
  }

  gcacheMetadata[cacheLine] = evictionType;
  totalEvictions++;
  cacheMetadata.erase(cacheIt);

  return NO_EVICTION;
}

EvictType DcacheLog::cacheInsert(uint64_t address, uint64_t cacheLine,
                                 uint64_t cacheLine2, uint64_t cacheSize) {

  EvictType result = NO_EVICTION;
  
  auto      it     = gcacheMetadata.find(cacheLine);

  std::cout << "cache line: " << cacheLine << std::endl;
  std::cout << "cache line2: " << cacheLine2 << std::endl;

  // Check existing cache line status
  if(it != gcacheMetadata.end()) {
    switch(it->second) {
      case EvictionType::NOT_EVICTED:
        std::cout << "not evicted dupe" << std::endl;
        return NO_EVICTION;
      case EvictionType::CAPACITY:
        ++capacityEvictions;
        result = CAPACITY;
        break;
      case EvictionType::CONFLICT:
        ++conflictEvictions;
        result = CONFLICT;
        break;
    }
    ++totalEvictions;
  } else {
    ++compulsoryEvictions;
    result = COMPULSORY;
  }

  std::cout << "size of cacheMetadata: " << cacheMetadata.size() << std::endl;

  // Handle eviction if needed
 
  // Insert new cache line if not 
  gcacheMetadata[cacheLine] = EvictionType::NOT_EVICTED;
  cacheMetadata[cacheLine]  = EvictionType::NOT_EVICTED;

  std::cout << "size of cacheMetadata2: " << cacheMetadata.size() << std::endl;
  return result;
}

void dcacheLogInit(CCacheState state) {
  std::cout << "Dcache log" << std::endl;

  state->dcacheLog = new DcacheLog();
  std::cout << "Dcache log initialized" << std::endl;
}
EvictType dcacheLogInsert(CCacheState state, uint64_t address,
                          uint64_t cacheLine, uint64_t cacheSize);
char*     dcacheLogGetEvictionsString(CCacheState state);

EvictType dcacheLogInsert(CCacheState state, uint64_t address,
                          uint64_t cacheLine, uint64_t cacheLine2,
                          uint64_t cacheSize) {
  return state->dcacheLog->cacheInsert(address, cacheLine, cacheLine2,
                                       cacheSize);
}

void dcacheLogEvictCacheLine(CCacheState state, uint64_t cacheLine,
                             uint64_t cacheSize) {
  state->dcacheLog->evictCacheLine(cacheLine, cacheSize);
}
char* dcacheLogGetEvictionsString(CCacheState state) {
  char* evictions = (char*)malloc(100);
  sprintf(evictions,
          "Capacity Evictions: %ld\nConflict Evictions: %ld\nCompulsory "
          "Evictions: %ld\n",
          state->dcacheLog->getCapacityEvictions(),
          state->dcacheLog->getConflictEvictions(),
          state->dcacheLog->getCompulsoryEvictions());
  return evictions;
}

uint64_t* dcacheLogGetEvictions(CCacheState state) {
  uint64_t* evictions = (uint64_t*)malloc(3 * sizeof(uint64_t));
  evictions[0]        = state->dcacheLog->getCapacityEvictions();
  evictions[1]        = state->dcacheLog->getConflictEvictions();
  evictions[2]        = state->dcacheLog->getCompulsoryEvictions();
  return evictions;
}
