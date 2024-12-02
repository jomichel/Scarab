#pragma once
#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>
#include "pref_common.h"

void pref_bof_init(HWP* hwp);

void pref_bof_ul1_miss(uns8 proc_id, Addr lineAddr, Addr loadPC,
                       uns32 global_hist);
void pref_bof_ul1_prefhit(uns8 proc_id, Addr lineAddr, Addr loadPC,
                          uns32 global_hist);
void pref_bof_umlc_miss(uns8 proc_id, Addr lineAddr, Addr loadPC,
                        uns32 global_hist);
void pref_bof_umlc_prefhit(uns8 proc_id, Addr lineAddr, Addr loadPC,
                           uns32 global_hist);

#ifdef __cplusplus
}
#endif
