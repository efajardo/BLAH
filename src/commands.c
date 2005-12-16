/*
#  File:     commands.c
#
#  Author:   David Rebatto
#  e-mail:   David.Rebatto@mi.infn.it
#
#
#  Revision history:
#   23 Apr 2004 - Origina release
#
#  Description:
#   Parse client commands
#
#
#  Copyright (c) 2004 Istituto Nazionale di Fisica Nucleare (INFN).
#  All rights reserved.
#  See http://grid.infn.it/grid/license.html for license details.
#
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "commands.h"
#include "blahpd.h"

/* Initialise commands array (strict alphabetical order)
 * handler functions prototypes are in commands.h
 * */    
#define COMMANDS_NUM 15
command_t commands_array[COMMANDS_NUM] = {
	/* cmd string, # of pars, threaded, handler */
	{ "ASYNC_MODE_OFF",               0, 0, cmd_async_off },
	{ "ASYNC_MODE_ON",                0, 0, cmd_async_on },
	{ "BLAH_GET_HOSTPORT",            1, 1, cmd_get_hostport },
	{ "BLAH_JOB_CANCEL",              2, 1, cmd_cancel_job },
	{ "BLAH_JOB_HOLD",                2, 1, cmd_hold_job },
	{ "BLAH_JOB_REFRESH_PROXY",       3, 1, cmd_renew_proxy },
	{ "BLAH_JOB_RESUME",              2, 1, cmd_resume_job },
	{ "BLAH_JOB_STATUS",              2, 1, cmd_status_job },
	{ "BLAH_JOB_SUBMIT",              2, 1, cmd_submit_job },
	{ "BLAH_SET_GLEXEC_DN",           2, 0, cmd_set_glexec_dn },
        { "BLAH_SET_GLEXEC_OFF",          0, 0, cmd_unset_glexec_dn },	
	{ "COMMANDS",                     0, 0, cmd_commands },
	{ "QUIT",                         0, 0, cmd_quit },
	{ "RESULTS",                      0, 0, cmd_results },
	{ "VERSION",                      0, 0, cmd_version }
};
/* Key comparison function 
 * */
int cmd_search_array(const void *key, const void *cmd_struct)
{
	return(strcasecmp((char *)key, ((command_t *)cmd_struct)->cmd_name));
}

/* Return a pointer to the command_t structure
 * whose cmd_name is cmd
 */
command_t *
find_command(const char *cmd)
{
	return(bsearch(cmd, commands_array, COMMANDS_NUM, sizeof(command_t), cmd_search_array));
}

/* Return a list of known commands
 * */
char *
known_commands(void)
{
	char *result;
	char *reallocated;
	int i;

	if (result = strdup(""))
	{
		for (i=0; i<COMMANDS_NUM; i++)
		{
			reallocated = (char *) realloc (result, strlen(result) + strlen(commands_array[i].cmd_name) + 2);
			if (reallocated == NULL)
			{
				free (result);
				result = NULL;
				break;
			}
			result = reallocated;
			if (i) strcat(result, " ");
			strcat(result, commands_array[i].cmd_name);
		}
	}
	return (result);
}

/* Split a command string into tokens
 * */
int
parse_command(const char *cmd, int *argc, char ***argv)
{
	char *pointer;
	char *parsed;
	char *reallocated;
	char *next;
	char **retval;
	char **curarg;
	int my_argc, join_arg;

	if (strlen(cmd) == 0)
	{
		*argv = NULL;
		*argc = 0;
		return(1);
	}
	
	if((retval = (char **)malloc(sizeof(char *))) == NULL)
	{
		fprintf(stderr, "Out of memory.\n");
		exit(MALLOC_ERROR);
	}

	if((parsed = strdup(cmd)) == NULL)
	{
		fprintf(stderr, "Out of memory.\n");
		exit(MALLOC_ERROR);
	}

	my_argc = 0;
	
	next = strtok_r(parsed, " ", &pointer);
	if (next != NULL) retval[my_argc] = strdup(next);
	else              retval[my_argc] = NULL;

	while (next != NULL)
	{
		join_arg = (retval[my_argc][strlen(retval[my_argc]) - 1] != '\\') ? 0 : 1;
				
		next = strtok_r(NULL, " ", &pointer);
		if (next != NULL)
		{
			if (join_arg)
			{
				retval[my_argc][strlen(retval[my_argc]) - 1] = ' ';
				retval[my_argc] = (char *) realloc (retval[my_argc], strlen(retval[my_argc]) + strlen(next) + 1);
				strcat(retval[my_argc], next);
			}
			else
			{
				my_argc++;
				if ((retval = (char **) realloc (retval, (my_argc+1) * sizeof(char *))) == NULL)
				{
					fprintf(stderr, "Out of memory.\n");
					exit(MALLOC_ERROR);
				}
				retval[my_argc] = strdup(next);
			}
		}
	}

	my_argc++;
	if ((retval = (char **) realloc (retval, (my_argc+1) * sizeof(char *))) == NULL)
	{
		fprintf(stderr, "Out of memory.\n");
		exit(2);
	}
	retval[my_argc] = NULL;

	*argv = retval;
	*argc = my_argc;

	free(parsed);

	return(0);
}
