#include <stdio.h>
#include <memory.h>
#include <unistd.h>
#include <sys/mman.h>
#include <assert.h>
#include "mm.h"
static size_t PAGE_SIZE;

static vm_page_for_families_t*  first_vm_page_for_families = NULL;

void init_mm(){
	PAGE_SIZE = getpagesize();
}

static void *get_vm_page_from_kernel(int units){

	char *vm_page = mmap(
			0,
			units * PAGE_SIZE,
			PROT_READ|PROT_WRITE|PROT_EXEC,
			MAP_ANON|MAP_PRIVATE,
			0,0);
	if(vm_page == MAP_FAILED){
		printf("ERROR: VM allocation failed.\n");
		return NULL;
	}
	memset(vm_page, 0, units * PAGE_SIZE);
	return (void*)vm_page;
}	

static void return_vm_page_to_kernel(void *vm_page, int units){
	if(munmap(vm_page, units * PAGE_SIZE)){
		printf("ERROR: VM return failed.\n");
	}
}

void mm_instantiate_new_page_family(char *struct_name, uint32_t struct_size){
	vm_page_family_t *vm_page_family_curr = NULL;
	vm_page_for_families_t  *new_vm_page_for_families = NULL;

	if(struct_size > PAGE_SIZE){
		printf("ERROR: %s size exceed page size.\n", struct_name);
		return;
	} 

	if(!first_vm_page_for_families){
		first_vm_page_for_families = (vm_page_for_families_t*)get_vm_page_from_kernel(1);
		first_vm_page_for_families->next = NULL;
		strncpy(first_vm_page_for_families->family[0].struct_name, struct_name, MAX_STRUCT_NAME);
		first_vm_page_for_families->family[0].struct_size = struct_size;
		first_vm_page_for_families->family[0].first_page = NULL;
		init_glthread(&first_vm_page_for_families->family[0].free_block_priority_list_head);
		return;
	}

	uint32_t count = 0;
	ITERATE_PAGE_FAMILY_BEGIN(first_vm_page_for_families, vm_page_family_curr){
		if(strncmp(vm_page_family_curr->struct_name, struct_name, MAX_STRUCT_NAME) != 0) {count++; continue;}
		assert(0);
	} ITERATE_PAGE_FAMILY_END(first_vm_page_for_families, vm_page_family_curr);

	if(count == MAX_FAMILIES_PER_VM_PAGE){
		new_vm_page_for_families = (vm_page_for_families_t*)get_vm_page_from_kernel(1);
		new_vm_page_for_families->next = first_vm_page_for_families;
		first_vm_page_for_families = new_vm_page_for_families;
		vm_page_family_curr = &first_vm_page_for_families->family[0];
	}
	strncpy(vm_page_family_curr->struct_name, struct_name, MAX_STRUCT_NAME);
	vm_page_family_curr->struct_size = struct_size;
	vm_page_family_curr->first_page = NULL;
	init_glthread(&vm_page_family_curr->free_block_priority_list_head);
}

void mm_print_registered_page_families(){
	vm_page_family_t *vm_page_family_curr = NULL;
	vm_page_for_families_t *vm_page_for_families_curr = NULL;
	for(vm_page_for_families_curr = first_vm_page_for_families; vm_page_for_families_curr ; vm_page_for_families_curr  = vm_page_for_families_curr->next){
		ITERATE_PAGE_FAMILY_BEGIN(vm_page_for_families_curr, vm_page_family_curr){
			printf("Page Family : %s, Size = %u\n",vm_page_family_curr->struct_name,vm_page_family_curr->struct_size);
		} ITERATE_PAGE_FAMILY_END(vm_page_for_families_curr, vm_page_family_curr);
	}
}

vm_page_family_t *lookup_page_family_by_name (char *struct_name){
	vm_page_family_t *vm_page_family_curr = NULL;
	vm_page_for_families_t *vm_page_for_families_curr = NULL;
	for(vm_page_for_families_curr = first_vm_page_for_families; vm_page_for_families_curr ; vm_page_for_families_curr  = vm_page_for_families_curr->next){
		ITERATE_PAGE_FAMILY_BEGIN(vm_page_for_families_curr, vm_page_family_curr){
			if(strncmp(vm_page_family_curr->struct_name, struct_name, MAX_STRUCT_NAME) == 0) return vm_page_family_curr;			       } ITERATE_PAGE_FAMILY_END(vm_page_for_families_curr, vm_page_family_curr);
	}
	return NULL;
}

static void mm_union_free_blocks(block_meta_data_t *first, block_meta_data_t *second){
	assert(first->is_free == MM_TRUE && second->is_free == MM_TRUE);
	first->block_size += (sizeof(block_meta_data_t) + second->block_size);
	first->next_block = second->next_block;
	if(second->next_block) second->next_block->prev_block = first;
}

vm_bool_t mm_is_vm_page_empty(vm_page_t *vm_page){
	if(vm_page->block_meta_data.next_block == NULL &&
			vm_page->block_meta_data.prev_block == NULL &&
			vm_page->block_meta_data.is_free == MM_TRUE) return MM_TRUE;
	else return MM_FALSE;
}

static inline uint32_t mm_max_page_allocatable_memory (int units){
	return (uint32_t) ((PAGE_SIZE * units) - offset_of(vm_page_t, page_memory));
}

vm_page_t *allocate_vm_page(vm_page_family_t *vm_page_family){
	vm_page_t *vm_page = get_vm_page_from_kernel(1);
	MARK_VM_PAGE_EMPTY(vm_page);

	vm_page->block_meta_data.block_size = mm_max_page_allocatable_memory(1);
	vm_page->block_meta_data.offset = offset_of(vm_page_t, block_meta_data);
	init_glthread(&vm_page->block_meta_data.priority_thread_glue);

	vm_page->next = NULL;
	vm_page->prev = NULL;
	vm_page->pg_family = vm_page_family;

	if(!vm_page_family->first_page){
		vm_page_family->first_page = vm_page;
		return vm_page;
	}
	vm_page->next = vm_page_family->first_page;
	vm_page_family->first_page->prev = vm_page;
	vm_page_family->first_page = vm_page;
	return vm_page;
}

void mm_vm_page_delete_and_free(vm_page_t *vm_page){
	vm_page_family_t *vm_pg_family = vm_page->pg_family;
	if(vm_pg_family->first_page == vm_page){
		vm_pg_family->first_page = vm_page->next;
		if(vm_page->next) vm_page->next->prev = NULL;
		vm_page->next = NULL;
		vm_page->prev = NULL;
		vm_page->pg_family = NULL;
		return_vm_page_to_kernel((void *)vm_page, 1);
		return;
	}
	if(vm_page->next) vm_page->next->prev = vm_page->prev;
	vm_page->prev->next = vm_page->next;
	vm_page->next = NULL;
	vm_page->prev = NULL;
	vm_page->pg_family = NULL;
	return_vm_page_to_kernel((void *)vm_page, 1);
}

static int free_blocks_comparison_function( void *_block_meta_data1, void *_block_meta_data2){

	block_meta_data_t *block_meta_data1 = (block_meta_data_t *)_block_meta_data1;
	block_meta_data_t *block_meta_data2 = (block_meta_data_t *)_block_meta_data2;
	if(block_meta_data1->block_size > block_meta_data2->block_size)
		return -1;
	else if(block_meta_data1->block_size < block_meta_data2->block_size)
		return 1;
	return 0;
}

static void mm_add_free_block_meta_data_to_free_block_list( vm_page_family_t *vm_page_family, block_meta_data_t *free_block){

	assert(free_block->is_free == MM_TRUE);
	glthread_priority_insert(&vm_page_family->free_block_priority_list_head,
			&free_block->priority_thread_glue,
			free_blocks_comparison_function,
			offset_of(block_meta_data_t, priority_thread_glue));
}

static vm_page_t *mm_family_new_page_add(vm_page_family_t *vm_page_family){
	vm_page_t *vm_page = allocate_vm_page(vm_page_family);
	if(!vm_page) return NULL;
	mm_add_free_block_meta_data_to_free_block_list(vm_page_family, &vm_page->block_meta_data);
	return vm_page;
}


static vm_bool_t mm_split_free_data_block_for_allocation(vm_page_family_t *vm_page_family, block_meta_data_t *block_meta_data, uint32_t size){
	block_meta_data_t *next_block_meta;
	assert(block_meta_data->is_free == MM_TRUE);
	if(block_meta_data->block_size < size) return MM_FALSE;
	uint32_t remain_size = block_meta_data->block_size - size;

	block_meta_data->is_free = MM_FALSE;
	block_meta_data->block_size = size;
	remove_glthread(&block_meta_data->priority_thread_glue);
	if(remain_size == 0) return MM_TRUE;
	else if(sizeof(block_meta_data_t) < remain_size && sizeof(block_meta_data_t) + vm_page_family->struct_size > remain_size){
		printf("1 remain_size = %u \n", remain_size);
		next_block_meta = NEXT_META_BLOCK_BY_SIZE(block_meta_data);
		next_block_meta->is_free = MM_TRUE;
		next_block_meta->block_size = remain_size - sizeof(block_meta_data_t) ;
		next_block_meta->offset = block_meta_data->offset + sizeof(block_meta_data_t) + next_block_meta->block_size;
		init_glthread(&next_block_meta->priority_thread_glue);
		mm_add_free_block_meta_data_to_free_block_list(vm_page_family, next_block_meta);
		mm_bind_blocks_for_allocation(block_meta_data, next_block_meta);	
	}
	else if(sizeof(block_meta_data_t) > remain_size){
		printf("2 remain_size = %u \n", remain_size);
	}
	else{
		next_block_meta = NEXT_META_BLOCK_BY_SIZE(block_meta_data);
		next_block_meta->is_free = MM_TRUE;
		next_block_meta->block_size = remain_size - sizeof(block_meta_data_t) ;
		next_block_meta->offset = block_meta_data->offset + sizeof(block_meta_data_t) + block_meta_data->block_size;
		init_glthread(&next_block_meta->priority_thread_glue);
		
		mm_add_free_block_meta_data_to_free_block_list(vm_page_family, next_block_meta);
		
		mm_bind_blocks_for_allocation(block_meta_data, next_block_meta);
	}
	return MM_TRUE;
}

static block_meta_data_t *mm_allocate_free_data_block(vm_page_family_t *vm_page_family, uint32_t req_size){
	vm_page_t *vm_page = NULL;
	vm_bool_t status = MM_FALSE;
	block_meta_data_t *biggest_block_meta_data = mm_get_biggest_free_block_page_family(vm_page_family);
	if(!biggest_block_meta_data || biggest_block_meta_data->block_size < req_size){
		vm_page = mm_family_new_page_add(vm_page_family);
		status = mm_split_free_data_block_for_allocation(vm_page_family, &vm_page->block_meta_data, req_size);
		if(status) return &vm_page->block_meta_data;
		return NULL;
	}

	if(biggest_block_meta_data){
		status = mm_split_free_data_block_for_allocation(vm_page_family, biggest_block_meta_data, req_size);
	}
	if(status) return biggest_block_meta_data;
	return NULL;

}

void *xmalloc(char *struct_name, int units){
	vm_page_family_t *pg_family = lookup_page_family_by_name (struct_name);
	if(!pg_family){
		printf("Error: Structure %s is not registered\n", struct_name);
		return NULL;
	}
	if(units * pg_family->struct_size >  mm_max_page_allocatable_memory (1)){
		printf("Error: Structure %s is too large\n", struct_name);
		return NULL;
	}
	block_meta_data_t *free_block_meta_data = NULL;
	free_block_meta_data = mm_allocate_free_data_block(pg_family, units * pg_family->struct_size);
	if(free_block_meta_data){
		memset((char*)(free_block_meta_data + 1), 0, free_block_meta_data->block_size);
		return (void*)(free_block_meta_data + 1);
	}
	return NULL;


}

static uint32_t mm_get_hard_internal_memory_frag_size(block_meta_data_t *first, block_meta_data_t* second){
	uint32_t first_end = first->offset + sizeof(block_meta_data_t) + first->block_size;
	return second->offset - first_end;
}

static block_meta_data_t *mm_free_blocks(block_meta_data_t *to_be_free_block){
	block_meta_data_t *return_block = NULL;
	assert(to_be_free_block->is_free == MM_FALSE);
	vm_page_t *vm_page =  MM_GET_PAGE_FROM_META_BLOCK(to_be_free_block);
	vm_page_family_t *vm_page_family = vm_page->pg_family;
	
	return_block = to_be_free_block;
	to_be_free_block->is_free = MM_TRUE;
	block_meta_data_t *next_block = NEXT_META_BLOCK(to_be_free_block);
	if(next_block){
		to_be_free_block->block_size += mm_get_hard_internal_memory_frag_size(to_be_free_block, next_block);
	}
	else{
		char *end_address_of_vm_page = (char*)((char*) vm_page + PAGE_SIZE);
		char *end_address_of_free_data_block = (char *)(to_be_free_block + 1) + to_be_free_block->block_size;
		uint32_t internal_mem_fragmentation = (uint32_t)((unsigned long) end_address_of_vm_page 
			- (unsigned long) end_address_of_free_data_block);
		to_be_free_block->block_size += internal_mem_fragmentation;
	}
	if(next_block && next_block->is_free == MM_TRUE){
		mm_union_free_blocks(to_be_free_block, next_block);
		return_block = to_be_free_block;
	}

	block_meta_data_t *prev_block = PREV_META_BLOCK(to_be_free_block);
	if(prev_block && prev_block->is_free){
		mm_union_free_blocks(prev_block, to_be_free_block);
		return_block = prev_block;	
	}
	if(mm_is_vm_page_empty(vm_page)){
		 mm_vm_page_delete_and_free(vm_page);
		 return NULL;
	}
	 mm_add_free_block_meta_data_to_free_block_list(vm_page_family, return_block);
	return return_block;
}


void xfree(void *app_data){
	block_meta_data_t *block_meta_data = (block_meta_data_t*) ((char*) (app_data) -  sizeof(block_meta_data_t));
	assert(block_meta_data->is_free == MM_FALSE);
	mm_free_blocks(block_meta_data);
}

void
mm_print_vm_page_details(vm_page_t *vm_page){

	printf("\t\t next = %p, prev = %p\n", vm_page->next, vm_page->prev);
	printf("\t\t page family = %s\n", vm_page->pg_family->struct_name);

	uint32_t j = 0;
	block_meta_data_t *curr;
	ITERATE_VM_PAGE_ALL_BLOCKS_BEGIN(vm_page, curr){

		printf("\t\t\t%-14p Block %-3u %s  block_size = %-6u  "
				"offset = %-6u  prev = %-14p  next = %p\n",
				curr,
				j++, curr->is_free ? "F R E E D" : "ALLOCATED",
				curr->block_size, curr->offset, 
				curr->prev_block,
				curr->next_block);
	} ITERATE_VM_PAGE_ALL_BLOCKS_END(vm_page, curr);
}

void mm_print_memory_usage(){
	vm_page_family_t *vm_page_family_curr = NULL;
	vm_page_for_families_t *vm_page_for_families_curr = NULL;
	vm_page_t *vm_page_curr = NULL;
	block_meta_data_t *block_meta_data_curr = NULL;
	for(vm_page_for_families_curr = first_vm_page_for_families; vm_page_for_families_curr ; vm_page_for_families_curr  = vm_page_for_families_curr->next){
		ITERATE_PAGE_FAMILY_BEGIN(vm_page_for_families_curr, vm_page_family_curr){
			printf("Page Family : %s, Size = %u\n",vm_page_family_curr->struct_name,vm_page_family_curr->struct_size);
			ITERATE_VM_PAGE_BEGIN(vm_page_family_curr, vm_page_curr){
				mm_print_vm_page_details(vm_page_curr);			
			} ITERATE_VM_PAGE_END(vm_page_family_curr, vm_page_curr);
		} ITERATE_PAGE_FAMILY_END(vm_page_for_families_curr, vm_page_family_curr);
	}				        

}





















