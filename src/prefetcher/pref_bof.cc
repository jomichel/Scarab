
#include <unordered_map>


#include "debug/debug_macros.h"
#include "debug/debug_print.h"
#include "globals/global_defs.h"
#include "globals/global_types.h"
#include "globals/global_vars.h"

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
#include "statistics.h"
#include <iostream>
#include "prefetcher/pref_bof.h"
struct RRentry {
  uint64_t base;
  uint64_t offset;
  uint64_t score;
};


class BOFprefetcher {
 private:
  std::unordered_map<uint64_t, RRentry> RRtable;
  uint64_t best_offset;
  uint64_t best_score;
  bool learning;
  uint64_t last_miss_addr;
  uint64_t new_entry_base;
  HWP_Info* hwp_info;  // This needs to be public or accessed via a getter

  const uint64_t BAD_SCORE = 8;

 public:
  BOFprefetcher(HWP* hwp) :
    best_offset(0),
    best_score(0), 
    learning(true),
    last_miss_addr(0),
    new_entry_base(0),
    hwp_info(hwp->hwp_info) {}

  // Add getter for hwp_info
  HWP_Info* get_hwp_info() const { return hwp_info; }

  void train_miss(uint64_t miss_addr) {
    if (learning) {
      if (new_entry_base == 0) {
        new_entry_base = miss_addr;
      } else {
        uint64_t offset = miss_addr - new_entry_base;
        if (RRtable.find(offset) != RRtable.end()) {
          RRtable[offset].score++;
          if (RRtable[offset].score > best_score) {
            best_score = RRtable[offset].score;
            best_offset = offset;
          }
        } else {
          RRtable[offset] = {new_entry_base, offset, 1};
        }
        new_entry_base = miss_addr;
      }
    } else {
      if (last_miss_addr != 0) {
        uint64_t offset = miss_addr - last_miss_addr;
        if (RRtable.find(offset) != RRtable.end()) {
          RRtable[offset].score++;
          if (RRtable[offset].score > best_score) {
            best_score = RRtable[offset].score;
            best_offset = offset;
          }
        }
      }
      last_miss_addr = miss_addr;
    }

    if (best_score > BAD_SCORE) {
      learning = false;
    }
  }

  void train_hit(uint64_t hit_addr) {
    if (last_miss_addr != 0) {
      uint64_t offset = hit_addr - last_miss_addr;
      if (RRtable.find(offset) != RRtable.end()) {
        RRtable[offset].score++;
      }
    }
  }

  uint64_t getPrefAddr(uint64_t miss_addr) {
    if (learning) {
      return 0;
    } else {
      return miss_addr + best_offset;
    }
  }

  void printRRtable() {
    std::cout << "RR Table:\n";
    for (const auto& entry : RRtable) {
      std::cout << "Offset: " << entry.second.offset
                << ", Base: " << entry.second.base
                << ", Score: " << entry.second.score << "\n";
    }
  }
};
static BOFprefetcher* g_bof = nullptr;

void pref_bof_init(HWP* hwp) {
  // Initialize global instance
 
  if(g_bof) delete g_bof;
  g_bof = new BOFprefetcher(hwp);
  hwp->hwp_info->enabled = true;
 
}

void pref_bof_ul1_miss(uns8 proc_id, Addr lineAddr, Addr loadPC, uns32 global_hist) {
  if(!g_bof) return;
  printf("BOF: UL1 miss\n");
  ASSERT(1, g_bof);

  g_bof->train_miss(lineAddr);
  auto pref_addr = g_bof->getPrefAddr(lineAddr);
  if (pref_addr != 0) {
    if (!pref_dl0req_queue_filter(pref_addr)) {
      pref_addto_dl0req_queue(proc_id, pref_addr, g_bof->get_hwp_info()->id);
    }
  }
}

void pref_bof_ul1_prefhit(uns8 proc_id, Addr lineAddr, Addr loadPC, uns32 global_hist) {
  if(!g_bof) return;
  printf("BOF: UL1 prefhit\n");
  ASSERT(1, g_bof);
  
  g_bof->train_hit(lineAddr);
}

void pref_bof_umlc_miss(uns8 proc_id, Addr lineAddr, Addr loadPC, uns32 global_hist) {
  printf("BOF: UMLC miss\n");
  if(!g_bof) return;
  g_bof->train_miss(lineAddr);
  auto pref_addr = g_bof->getPrefAddr(lineAddr);
  if (pref_addr != 0) {
    if (!pref_dl0req_queue_filter(pref_addr)) {
      pref_addto_dl0req_queue(proc_id, pref_addr, g_bof->get_hwp_info()->id);
    }
  }
}

void pref_bof_umlc_prefhit(uns8 proc_id, Addr lineAddr, Addr loadPC, uns32 global_hist) {
  printf("BOF: UMLC prefhit\n");
  if(!g_bof) return;
  g_bof->train_hit(lineAddr);
}

// Add cleanup function
void pref_bof_cleanup() {
  delete g_bof;
  g_bof = nullptr;
}