#ifndef __MM__
#define __MM__

#include "glthread.h"
#include <stdint.h>

#define MAX_FAMILIES_PER_VM_PAGE \
	        ((PAGE_SIZE - sizeof(vm_page_for_families_t *)) \
		          / sizeof(vm_page_family_t))

#define MAX_STRUCT_NAME 32

struct vm_page_;

typedef struct vm_page_family {
    char struct_name[MAX_STRUCT_NAME];
    uint32_t struct_size;
    struct vm_page_ *first_page;
    glthread_t free_block_priority_list_head;
} vm_page_family_t;

typedef struct vm_page_for_families {
    struct vm_page_for_families *next;
    vm_page_family_t family[0];
} vm_page_for_families_t;

#define ITERATE_PAGE_FAMILY_BEGIN(vm_page_for_families_ptr, cur)  \
{                                                                 \
    uint32_t count = 0; \
    cur = (vm_page_family_t*)&vm_page_for_families_ptr->family[0]; \
    for(; cur->struct_size > 0 && count < MAX_FAMILIES_PER_VM_PAGE ; cur++, count++){ \

#define ITERATE_PAGE_FAMILY_END(vm_page_for_families_ptr, cur) }}

vm_page_family_t * lookup_page_family_by_name (char *struct_name);

typedef enum{
    MM_FALSE,
    MM_TRUE
} vm_bool_t;

typedef struct block_meta_data_{
    vm_bool_t is_free;
    uint32_t block_size;
    uint32_t offset;
    glthread_t priority_thread_glue;
    struct block_meta_data_* prev_block;
    struct block_meta_data_* next_block; 
} block_meta_data_t;
GLTHREAD_TO_STRUCT(glthread_to_block_meta_data, block_meta_data_t, priority_thread_glue, glthread_ptr);

#define offset_of(container_structure, field_name) ((size_t) &(((container_structure *)0)->field_name))

#define MM_GET_PAGE_FROM_META_BLOCK(block_meta_data_ptr) ((void*)((char*)block_meta_data_ptr - block_meta_data_ptr->offset))
    
#define NEXT_META_BLOCK(block_meta_data_ptr) (block_meta_data_ptr->next_block)

#define NEXT_META_BLOCK_BY_SIZE(block_meta_data_ptr) \
    (block_meta_data_t*)((char*)(block_meta_data_ptr + 1) + block_meta_data_ptr->block_size)

#define PREV_META_BLOCK(block_meta_data_ptr) (block_meta_data_ptr->prev_block)

#define mm_bind_blocks_for_allocation(allocated_meta_block, free_meta_block) \
    free_meta_block->next_block = allocated_meta_block->next_block; \
    free_meta_block->prev_block = allocated_meta_block; \
    allocated_meta_block->next_block = free_meta_block; \
    if(free_meta_block->next_block) free_meta_block->next_block->prev_block = free_meta_block;

static inline block_meta_data_t *mm_get_biggest_free_block_page_family(vm_page_family_t *vm_page_family){
    glthread_t *biggest_free_block_glue = vm_page_family->free_block_priority_list_head.right;
    if(biggest_free_block_glue){
    	return glthread_to_block_meta_data(biggest_free_block_glue);
    	
    } 
    return NULL;
}
    

typedef struct vm_page_{
    struct vm_page_ *next;
    struct vm_page_ *prev;
    vm_page_family_t *pg_family;
    block_meta_data_t block_meta_data;
    char page_memory[0];
} vm_page_t;

vm_bool_t mm_is_vm_page_empty(vm_page_t *vm_page); 

#define MARK_VM_PAGE_EMPTY(vm_page_t_ptr) \
    vm_page_t_ptr->block_meta_data.next_block = NULL; \
    vm_page_t_ptr->block_meta_data.prev_block = NULL; \
    vm_page_t_ptr->block_meta_data.is_free = MM_TRUE; \

#define ITERATE_VM_PAGE_BEGIN(vm_page_family_ptr, curr) \
{ \
    curr = vm_page_family_ptr->first_page; \
    for(; curr ; curr = curr->next){ 

#define ITERATE_VM_PAGE_END(vm_page_family_ptr, curr) \
    }}

#define ITERATE_VM_PAGE_PER_FAMILY_BEGIN(vm_page_ptr, curr) \
{ \
    curr = vm_page_ptr->block_meta_data; \
    for(; curr ; curr = NEXT_META_BLOCK(curr)){

#define ITERATE_VM_PAGE_PER_FAMILY_END(vm_page_ptr, curr) \
    }} 


#endif

#define ITERATE_VM_PAGE_ALL_BLOCKS_BEGIN(vm_page_ptr, curr)    \
{\
	curr = &vm_page_ptr->block_meta_data;\
	block_meta_data_t *next = NULL;\
	for( ; curr; curr = next){\
		next = NEXT_META_BLOCK(curr);

#define ITERATE_VM_PAGE_ALL_BLOCKS_END(vm_page_ptr, curr)   \
	}}

