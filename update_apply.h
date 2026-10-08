#ifndef _UPDATE_APPLY_H_
#define _UPDATE_APPLY_H_

// Applies a staged image, or resumes a started apply; returns without touching
// the running image when the staged one is unusable
void update_apply_staged(void);

#endif
