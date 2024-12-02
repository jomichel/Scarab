#include <algorithm>
#include <iostream>
#include <unordered_map>
#include <vector>

#include "debug/debug_macros.h"
#include "debug/debug_print.h"
#include "globals/global_defs.h"
#include "globals/global_types.h"
#include "globals/global_vars.h"
#include "prefetcher/pref_bof.param.h"

#include "globals/assert.h"
#include "globals/utils.h"
#include "op.h"

#include "core.param.h"
#include "dcache_stage.h"
#include "debug/debug.param.h"
#include "general.param.h"
#include "libs/cache_lib.h"
#include "libs/hash_lib.h"
#include "libs/list_lib.h"
#include "memory/memory.h"
#include "memory/memory.param.h"
#include "prefetcher/l2l1pref.h"
#include "prefetcher/pref.param.h"
#include "prefetcher/pref_bof.h"
#include "prefetcher/pref_bof.param.h"
#include "statistics.h"
#include "globals/global_defs.h"
#include "globals/global_types.h"
#include "globals/global_vars.h"

struct RRentry {
  uint64_t base;
  uint64_t offset;
  uint64_t score;
};

#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>

class BOFprefetcher {
 private:
  struct RRentry {
    uint64_t base;
    uint64_t offset;
    uint64_t score;
    uint64_t last_access_time;
    uint64_t tag;
  };
  uint64_t                               active_offset = 0;
  uint64_t                               MXOFFSET = VA_PAGE_SIZE_BYTES / DCACHE_LINE_SIZE ;
  std::unordered_map<uint64_t, RRentry> RRtable;

  std::vector<uint64_t>                  offset_list;
  std::unordered_map<uint64_t, uint64_t> score_table;
  uint64_t                               best_offset;
  bool                                   have_learned = false;
  uint64_t                               last_miss_addr;
  uint64_t                               new_entry_base;
  uint64_t                               current_offset_index;
  uint64_t                               round_count;
  uint64_t       cycle_count;  
  const uint64_t BAD_SCORE = PREF_BOF_BADSCORE;
  const uint64_t SCOREMAX  = 32;
  const uint64_t ROUNDMAX  = 128;
  const uint64_t RRMAX    = 256;

 public:
  uint64_t  best_score;
  HWP_Info* hwp_info;
  uint64_t  type;

  BOFprefetcher() :
      best_offset(0) , last_miss_addr(0), new_entry_base(0),
      current_offset_index(0), round_count(0), cycle_count(0), best_score(0),
      hwp_info(nullptr) {
    initialize_offset_list();
  }

  BOFprefetcher(HWP* hwp) :
      best_offset(0)   , last_miss_addr(0), new_entry_base(0),
      current_offset_index(0), round_count(0), cycle_count(0), best_score(0),
      hwp_info(hwp->hwp_info) {
    initialize_offset_list();
  }

  BOFprefetcher& operator=(const BOFprefetcher& other) {
    if(this != &other) {
      this->RRtable              = other.RRtable;
      this->best_offset          = other.best_offset;
      this->best_score           = other.best_score;
       this->last_miss_addr       = other.last_miss_addr;
      this->new_entry_base       = other.new_entry_base;
      this->hwp_info             = other.hwp_info;
      this->current_offset_index = other.current_offset_index;
      this->round_count          = other.round_count;
      this->offset_list          = other.offset_list;
      this->score_table          = other.score_table;
      this->cycle_count          = other.cycle_count;
    }
    return *this;
  }

  void initialize_offset_list() {
    offset_list = {1,   2,   3,   4,   5,   6,   8,   9,   10,  12,  15,
                   16,  18,  20,  24,  25,  27,  30,  32,  36,  40,  45,
                   48,  50,  54,  60,  65,  72,  75,  80,  81,  90,  96,
                   100, 108, 120, 125, 128, 135, 144, 150, 160, 162, 180,
                   192, 200, 216, 225, 240, 243, 250, 256};
    // // remove offsets greater than MXOFFSET
    // offset_list.erase(std::remove_if(offset_list.begin(), offset_list.end(),
    //                                  [this](uint64_t offset) {
    //                                    return offset > MXOFFSET;
    //                                  }),
    //                   offset_list.end());
    // Initialize the score table
    for(auto offset : offset_list) {
      score_table[offset] = 0;
    }
  }

void train_miss(uint64_t miss_addr, uint64_t cycle_count) {

     
    
    if (current_offset_index >= offset_list.size()) {
      //  std::cerr << "Invalid offset index: " << current_offset_index << std::endl;
        return;
    }
    uint64_t current_offset = offset_list[current_offset_index];
    uint64_t test_addr = miss_addr - current_offset;
 
     if (RRtable.find(test_addr) != RRtable.end()    ) {
        uint64_t access_time = cycle_count - RRtable[test_addr].last_access_time;
       std::cout << "Hit at offset " << current_offset << " with access time " << access_time << std::endl;

        if (access_time >= 100) {
            score_table[current_offset]++;
            if (score_table[current_offset] > best_score) {
                best_score = score_table[current_offset];
                best_offset = current_offset;
            }
        }
    }

    current_offset_index = (current_offset_index + 1) % offset_list.size();
    if (current_offset_index == 0) {
        round_count++;
    }

    if (best_score >= SCOREMAX || round_count >= ROUNDMAX) {
        have_learned = true;
        if (best_score < BAD_SCORE) {
            best_offset = 0;
        }
        // Debugging outputs
        for (auto& entry : score_table) {
            std::cout << "Offset: " << entry.first << " Score: " << entry.second << std::endl;
        }
      
        active_offset = best_offset;
        best_offset = 0;
       std::cout << "Learning phase completed with best offset: " << active_offset << std::endl;
        reset();
    }

    RRentry new_entry;
    new_entry.base = miss_addr - current_offset;
    new_entry.offset = current_offset;
    new_entry.score = score_table[current_offset];
    new_entry.last_access_time = cycle_count;
 
    RRtable[miss_addr] = new_entry;

    last_miss_addr = miss_addr;
}
 bool is_same_page(uint64_t addr1, uint64_t addr2) {
    return (addr1 / VA_PAGE_SIZE_BYTES) == (addr2 / VA_PAGE_SIZE_BYTES);
  }
  void train_hit(uint64_t hit_addr) {}

  uint64_t getPrefAddr(uint64_t miss_addr) {
    if(!have_learned)
      return 0;
    if (active_offset == 0)
      return 0;
    // if (!is_same_page(miss_addr, last_miss_addr))
    //   return 0;
     return miss_addr + (active_offset);
  }

  void reset() {
    RRtable.clear();
    best_score = 0;
    round_count = 0;
    current_offset_index = 0;
 
    for (auto& entry : score_table) {
        entry.second = 0;
    }
}
  
};

typedef struct {
  BOFprefetcher* bof_hwp_core_ul1;
  BOFprefetcher* bof_hwp_core_umlc;
} bof_prefetchers;

bof_prefetchers bof_prefetchers_array;

void init_bof(HWP* hwp, BOFprefetcher* bof_hwp_core) {
  for(uns i = 0; i < NUM_CORES; i++) {
    bof_hwp_core[i]                   = BOFprefetcher(hwp);
    bof_hwp_core[i].hwp_info          = hwp->hwp_info;
    bof_hwp_core[i].hwp_info->enabled = TRUE;
  }
}

void pref_bof_init(HWP* hwp) {
  printf("BAD_SCORE: %d\n", PREF_BOF_BADSCORE);
  printf("PREF_BOF_ON: %d\n", PREF_BOF_ON);
  if(!PREF_BOF_ON)
    return;
  if(PREF_UMLC_ON) {
    bof_prefetchers_array.bof_hwp_core_umlc = new BOFprefetcher[NUM_CORES];
    init_bof(hwp, bof_prefetchers_array.bof_hwp_core_umlc);
  }

  if(PREF_UL1_ON) {
    bof_prefetchers_array.bof_hwp_core_ul1 = new BOFprefetcher[NUM_CORES];
    init_bof(hwp, bof_prefetchers_array.bof_hwp_core_ul1);
  }
}

void pref_bof_ul1_miss(uns8 proc_id, Addr lineAddr, Addr loadPC,
                       uns32 global_hist) {
 
  if(!bof_prefetchers_array.bof_hwp_core_ul1)
    return;
  bof_prefetchers_array.bof_hwp_core_ul1[proc_id].train_miss(lineAddr, cycle_count);
  Addr pref_addr = bof_prefetchers_array.bof_hwp_core_ul1[proc_id].getPrefAddr(
    lineAddr);
  if(pref_addr != 0) {
    // Issue prefetch
    // if(!pref_addto_ul1req_queue(
    //      proc_id, pref_addr,
    //      bof_prefetchers_array.bof_hwp_core_ul1[proc_id].hwp_info->id)) {
    //   std::cout << "Prefetch queue is full\n";
    // }
  }
}

void pref_bof_ul1_prefhit(uns8 proc_id, Addr lineAddr, Addr loadPC,
                          uns32 global_hist) {
  if(!bof_prefetchers_array.bof_hwp_core_ul1)
    return;
  bof_prefetchers_array.bof_hwp_core_ul1[proc_id].train_miss(lineAddr, cycle_count);
}

void pref_bof_umlc_miss(uns8 proc_id, Addr lineAddr, Addr loadPC,
                        uns32 global_hist) {
  if(!bof_prefetchers_array.bof_hwp_core_umlc)
    return;
  bof_prefetchers_array.bof_hwp_core_umlc[proc_id].train_miss(lineAddr, cycle_count);
  Addr pref_addr = bof_prefetchers_array.bof_hwp_core_umlc[proc_id].getPrefAddr(
    lineAddr);
  if(pref_addr != 0) {
    // // Issue prefetch
    // if(!pref_addto_umlc_req_queue(
    //      proc_id, pref_addr,
    //      bof_prefetchers_array.bof_hwp_core_umlc[proc_id].hwp_info->id)) {
    //   std::cout << "Prefetch queue is full\n";
    // }
  }
}

void pref_bof_umlc_prefhit(uns8 proc_id, Addr lineAddr, Addr loadPC,
                           uns32 global_hist) {
  if(!bof_prefetchers_array.bof_hwp_core_umlc)
    return;
  bof_prefetchers_array.bof_hwp_core_umlc[proc_id].train_miss(lineAddr, cycle_count);
}