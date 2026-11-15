#ifndef NV2A_VK_UI_SURVEY_H
#define NV2A_VK_UI_SURVEY_H
#include <stdint.h>
/* Ordinal matches the existing color draw-capture selector at the same hook. */
static int nvk_ui_survey_next(uint64_t frame, uint64_t first, uint64_t second,
                              int depth_only, uint64_t *last, uint32_t *ordinal) {
    if ((frame!=first && frame!=second) || depth_only) return 0;
    if(*last!=frame){*last=frame;*ordinal=0;}
    if(*ordinal==UINT32_MAX)return 0;
    ++*ordinal; return *ordinal<=4096u;
}
#endif
