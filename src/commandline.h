#ifndef COMMANDLINE_H
#define COMMANDLINE_H

#include "config.h"
#define LHC_CMDLINE_INPUT_BUFFER_SIZE 512

#ifdef WITH_GNU_READLINE

#include <readline/readline.h>
#include <readline/history.h>
#include <stdlib.h>
#define read_line(buf, prompt) (((buf) = readline((prompt))) != NULL)
#define save_line(buf) add_history((buf))
#define free_line(buf) free((buf))

#else /* WITH_GNU_READLINE */

#define read_line(buf, prompt) ((fputs((prompt), stdout), fflush(stdout),\
            fgets((buf), LHC_CMDLINE_INPUT_BUFFER_SIZE, stdin)) != NULL)
#define save_line(buf) (void)(buf)
#define free_line(buf) (void)(buf)

#endif /* WITH_GNU_READLINE */

#endif /* COMMANDLINE_H */
