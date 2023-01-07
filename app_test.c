#include <stdio.h>
#include "u_api_mm.h"
#include <assert.h>

typedef struct emp_ {

	char name[32];
	uint32_t emp_id;
} emp_t;

typedef struct student_ {

	char name[32];
	uint32_t rollno;
	uint32_t marks_phys;
	uint32_t marks_chem;
	uint32_t marks_maths;
	struct student_ *next;
} student_t;

int main(int argc, char **argv){
	int wait;
	init_mm();
	MM_REG_STRUCT(emp_t);
	MM_REG_STRUCT(student_t);
	mm_print_registered_page_families();
	void *emp1 = XCALLOC(1, emp_t);
	void *emp2 = XCALLOC(1, emp_t);
	void *emp3 = XCALLOC(1, emp_t);

	void *stu1 = XCALLOC(1, student_t);
	void *stu2 = XCALLOC(1, student_t);

	mm_print_memory_usage(0);
	
	scanf("%d", &wait);
	XFREE(emp1);
	XFREE(emp3);
	XFREE(stu2);

	mm_print_memory_usage(0);
}
	
