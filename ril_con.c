/*
 * =====================================================================================
 *
 *       Filename:  ril_con.c
 *
 *    Description:  for Meizu Inc Modem test.
 *
 *        Version:  1.0
 *        Created:  2012年11月08日 15时00分07秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  KarlZheng (zhengkarl#gmail.com)
 *        Company:  Meizu, Inc
 *
 * =====================================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include "readline/readline.h"
#include "readline/history.h"

int done = 0;

void interrupt_handler(int signum)
{
	done = 1;
	printf("\n");
}

int ril_con_exit_cb(void)
{
	write_history(".rilcon_history");
	return 0;
}

int process_command(const char* cmdline)
{
	if(strcmp(cmdline, "quit") == 0)
		done = 1;
	else if(strcmp(cmdline, "exit") == 0) {
		ril_con_exit_cb();
		exit(0);
	} else
		printf("RIL_CON get:%s\n", cmdline);

	return 0;
}

int main(int argc, const char* const argv[])
{
	signal(SIGINT,  interrupt_handler); 
	signal(SIGHUP,  interrupt_handler); 
	signal(SIGALRM, interrupt_handler); 
	signal(SIGTERM, interrupt_handler); 

	rl_set_signals();
	using_history();
	read_history(".rilcon_history");

	done = 0;
	while (!done) {
		char* line = readline("MEIZU_RILCON # ");

		if(line && *line) {
			char* expanded;
			int result = history_expand(line, &expanded);

			if(result < 0 || result == 2) {
				if(result == 2)
					printf("%s\n", expanded);
				free(line);
				free(expanded);
				continue;
			}
			add_history(expanded);
			process_command(expanded);
			free(expanded);
		}
		if(line)
			free(line);
	}
	ril_con_exit_cb();

	return 0;
}
