#ifndef _UTIL_H_
#define _UTIL_H_

#include <exec/types.h>

extern unsigned long get_msec(void);

extern void *load_image(const char *fname, unsigned long *xsz, unsigned long *ysz);

extern unsigned int setup_shader(const char *fname);
extern void set_uniform1f(unsigned int prog, const char *name, float val);
extern void set_uniform2f(unsigned int prog, const char *name, float v1, float v2);
extern void set_uniform1i(unsigned int prog, const char *name, int val);

extern BOOL GetProcAddresses(void);

#endif	/* _UTIL_H_ */
