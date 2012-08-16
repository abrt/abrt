#include <libabrt.h>
#include <stdio.h>

int main(int argc, char **argv)
{
	problem_data_t *pd = problem_data_new();
	/* required fields */
	problem_data_add_text_editable(pd, "type", "java");
	problem_data_add_text_editable(pd, "analyzer", "java");
	problem_data_add_text_editable(pd, "uid", "1000");
	problem_data_add_text_editable(pd, "pid", "1000");
	/* executable must belong to some package otherwise ABRT refuse it */
	problem_data_add_text_editable(pd, "EXECUTABLE", "/usr/bin/sleep");
	problem_data_add_text_editable(pd, "backtrace", "the whole exception");
	/* type and analyzer are the same for abrt, we keep both just for sake of comaptibility */
	problem_data_add_text_editable(pd, "reason", "some shortsummary (usually the first line of the exception");
	/* end of required fields */
	problem_data_add_text_editable(pd, "optional_1", "something optional");
	problem_data_add_text_editable(pd, "foo", "bar");

	/* sends problem data to abrtd over the socket */
	int res = problem_data_send_to_abrt(pd);
	    printf("problem data created: '%s'\n", res ? "failure" : "success");
	problem_data_free(pd);

	return res;
}
