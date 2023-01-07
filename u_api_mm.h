#ifndef __UAPI_MM__
#define __UAPI_MM__

#include <stdint.h>


void * xmalloc(char *struct_name, int units);

void xfree(void *app_ptr);

void mm_instantiate_new_page_family( char *struct_name, uint32_t struct_size);

void mm_print_memory_usage();

void mm_print_registered_page_families();

void init_mm();


#define MM_REG_STRUCT(struct_name)  \
	(mm_instantiate_new_page_family(#struct_name, sizeof(struct_name)))


#define XCALLOC(units, struct_name) \
	(xmalloc(#struct_name, units))

#define XFREE(ptr) \
	(xfree(ptr))

#endif 
