#include <stdlib.h>    // NULL, size_t, EXIT_SUCCESS, EXIT_FAILURE, ...
#include <string.h>    // strlen(), strcmp(), ...
#include <signal.h>    // sigaction(), ... 
#include <float.h>     // DBL_MAX
#include <sys/epoll.h> // epoll_wait(), ... 
#include <sys/types.h> // pid_t
#include <sys/wait.h>  // waitpid()
#include <errno.h>     // errno
#include "ini.h"       // https://github.com/benhoyt/inih
#include "succade.h"   // defines, structs, all that stuff
#include "options.c"   // Command line args/options parsing
#include "helpers.c"   // Helper functions, mostly for strings
#include "execute.c"   // Execute child processes
#include "loadini.c"   // Handles loading/processing of INI cfg file

static volatile int running;   // Used to stop main loop in case of SIGINT
static volatile int handled;   // The last signal that has been handled 
static volatile int sigchld;   // SIGCHLD has been received, please handle

/*
 * Init the given block struct to a well defined state using sensible defaults.
 */
static void init_block(block_s *block)
{
	block->block_cfg.offset = -1;
	//block->block_cfg.reload = 5.0;
}

static void init_spark(spark_s *spark)
{
}

/*
 * Frees all members of the given bar that need freeing.
 */
static void free_lemon(lemon_s *lemon)
{
	/*
	free(lemon->name);
	free(lemon->bin);
	free(lemon->fg);
	free(lemon->bg);
	free(lemon->lc);
	free(lemon->prefix);
	free(lemon->suffix);
	free(lemon->format);
	free(lemon->block_font);
	free(lemon->label_font);
	free(lemon->affix_font);
	free(lemon->block_bg);
	free(lemon->label_fg);
	free(lemon->label_bg);
	free(lemon->affix_fg);
	free(lemon->affix_bg);
	*/
}

/*
 * Frees all members of the given block that need freeing.
 */
static void free_block(block_s *block)
{
	/*
	free(block->name);
	free(block->bin);
	free(block->fg);
	free(block->bg);
	free(block->lc);
	free(block->label_fg);
	free(block->label_bg);
	free(block->affix_fg);
	free(block->affix_bg);
	free(block->label);
	free(block->trigger);
	free(block->cmd_lmb);
	free(block->cmd_mmb);
	free(block->cmd_rmb);
	free(block->cmd_sup);
	free(block->cmd_sdn);
	free(block->input);
	free(block->result);
	*/
}

/*
 * Constructs the command string for running the bar, ready to be used with 
 * popen_noshell() or similar, and places it in the provided buffer `buf`, 
 * whose length is assumed to be `buf_len`. If the buffer size is not large 
 * enough to hold the entire command string, truncation will occur. 
 * Returns the strlen() of the constructed string.
 */
int lemon_cmd(lemon_s *lemon, char *buf, size_t buf_len)
{
	lemon_cfg_s *lcfg = &lemon->lemon_cfg;
	block_cfg_s *bcfg = &lemon->block_cfg;

	// https://stackoverflow.com/questions/3919995/
	char w[8]; // TODO hardcoded value
	char h[8];

	snprintf(w, 8, "%d", lcfg->w);
	snprintf(h, 8, "%d", lcfg->h);

	char *block_font = optstr('f', lcfg->block_font, 0);
	char *label_font = optstr('f', lcfg->label_font, 0);
	char *affix_font = optstr('f', lcfg->affix_font, 0);
	char *name_str   = optstr('n', lcfg->name, 0);

	int cmd_len = snprintf(buf, buf_len,
		"%s -g %sx%s+%d+%d -F%s -B%s -U%s -u%d %s %s %s %s %s %s",
		lcfg->bin,                                   // strlen
		(lcfg->w > 0) ? w : "",                      // max 8
		(lcfg->h > 0) ? h : "",                      // max 8
		lcfg->x,                                     // max 8
		lcfg->y,                                     // max 8
		(bcfg->fg && bcfg->fg[0]) ? bcfg->fg : "-",  // strlen, max 9
		(lcfg->bg && lcfg->bg[0]) ? lcfg->bg : "-",  // strlen, max 9
		(bcfg->lc && bcfg->lc[0]) ? bcfg->lc : "-",  // strlen, max 9
		lcfg->lw,                                    // max 4
		(lcfg->bottom) ? "-b" : "",                  // max 2
		(lcfg->force)  ? "-d" : "",                  // max 2
		block_font,                                  // strlen
		label_font,                                  // strlen
		affix_font,                                  // strlen
		name_str                                     // strlen
	);

	free(block_font);
	free(label_font);
	free(affix_font);
	free(name_str);

	return cmd_len;
}

/*
 * Command line options and arguments string for lemonbar.
 * Allocated with malloc(), so please free() it at some point.
 * TODO not in use yet, should eventually replace lemon_cmd()
 */
char *lemon_arg(lemon_s *lemon)
{
	lemon_cfg_s *lcfg = &lemon->lemon_cfg;
	block_cfg_s *bcfg = &lemon->block_cfg;

	char w[8]; // TODO hardcoded (8 is what we want tho) 
	char h[8];

	snprintf(w, 8, "%d", lcfg->w);
	snprintf(h, 8, "%d", lcfg->h);

	char *block_font = optstr('f', lcfg->block_font, 0);
	char *label_font = optstr('f', lcfg->label_font, 0);
	char *affix_font = optstr('f', lcfg->affix_font, 0);
	char *name_str   = optstr('n', lcfg->name, 0);

	char *arg = malloc(sizeof(char) * 1024); // TODO hardcoded (1024 is what we want tho)

	snprintf(arg, 1024,
		"-g %sx%s+%d+%d -F%s -B%s -U%s -u%d %s %s %s %s %s %s",
		(lcfg->w > 0) ? w : "",                      // max 8
		(lcfg->h > 0) ? h : "",                      // max 8
		lcfg->x,                                     // max 8
		lcfg->y,                                     // max 8
		(bcfg->fg && bcfg->fg[0]) ? bcfg->fg : "-",  // strlen, max 9
		(lcfg->bg && lcfg->bg[0]) ? lcfg->bg : "-",  // strlen, max 9
		(bcfg->lc && bcfg->lc[0]) ? bcfg->lc : "-",  // strlen, max 9
		lcfg->lw,                                    // max 4
		(lcfg->bottom) ? "-b" : "",                  // max 2
		(lcfg->force)  ? "-d" : "",                  // max 2
		block_font,                                  // strlen, max 255
		label_font,                                  // strlen, max 255
		affix_font,                                  // strlen, max 255
		name_str                                     // strlen
	);

	free(block_font);
	free(label_font);
	free(affix_font);
	free(name_str);

	return arg;
}

// TODO - not in use yet!
//      - would be nice if we didn't have to hand in `in`, `out` and `err`
int open_child(child_s *child, int in, int out, int err)
{
	if (child->pid > 0)
	{
		// ALREADY OPEN
		return -1;
	}

	if (empty(child->cmd))
	{
		// NO COMMAND GIVEN
		return -1;
	}

	// Construct the command, if there is an additional argument string
	char *cmd = NULL;
	if (child->arg)
	{
		size_t len = strlen(child->cmd) + strlen(child->arg) + 4;
		cmd = malloc(sizeof(char) * len);
		snprintf(cmd, len, "%s %s", child->cmd, child->arg);
		fprintf(stderr, "open_child(): %s\n", cmd);
	}

	// Execute the block and retrieve its PID
	child->pid = popen_noshell(
			cmd ? cmd : child->cmd, 
			in  ? &(child->fp[FD_IN])  : NULL,
			out ? &(child->fp[FD_OUT]) : NULL,
		        err ? &(child->fp[FD_ERR]) : NULL);
	free(cmd);
	
	// Check if that worked
	if (child->pid == -1)
	{
		// FAILED TO OPEN
		return -1;
	}
	
	// TODO do we really ALWAYS want linebuf for ALL THREE streams?
	fp_linebuffered(child->fp[FD_IN]);
	fp_linebuffered(child->fp[FD_OUT]);
	fp_linebuffered(child->fp[FD_ERR]);
	return 0;
}

/*
 * Runs the bar process and opens file descriptors for reading and writing.
 * Returns 0 on success, -1 if bar could not be started.
 */
int open_lemon(lemon_s *lemon)
{
	child_s *child = &lemon->child;

	if (empty(child->cmd))
	{
		child->cmd = strdup(lemon->lemon_cfg.bin);
	}

	if (empty(child->arg))
	{
		child->arg = lemon_arg(lemon);
	}

	if (DEBUG)
	{
		fprintf(stderr, "Bar command: %s %s\n", child->cmd, child->arg);
	}
	
	return open_child(child, 1, 1, 0);
}

/*
 * Runs a block and creates a file descriptor (stream) for reading.
 * Returns 0 on success, -1 if block could not be executed.
 * TODO: Should this function check if the block is already open?
 */
int open_block(block_s *block)
{
	child_s *child = &block->child;

	// If no cmd given for child, use `bin` from config or `sid` from block
	if (empty(child->cmd))
	{
		child->cmd = block->block_cfg.bin ? strdup(block->block_cfg.bin) : strdup(block->sid);
	}

	if (child->input)
	{
		// Place a quoted version of `input` into `arg`
		free(child->arg);
		size_t arg_len = strlen(child->input) + 3;
		child->arg = malloc(sizeof(char) * arg_len);
		snprintf(child->arg, arg_len, "'%s'", child->input);
	}

	// Execute the block and retrieve its PID
	int success = open_child(child, 0, 1, 0);
	//fprintf(stderr, "OPENED %s: PID = %d, FD %s\n", block->sid, child->pid, (child->fp[FD_OUT]==NULL?"dead":"okay"));

	// Return 0 on success, -1 on error
	return success; 
}

/*
 * Closes the given bar by killing the process, closing its file descriptors
 * and setting them to NULL after.
 */
void close_lemon(lemon_s *lemon)
{
	child_s *child = &lemon->child;

	if (child->pid > 1)
	{
		kill(child->pid, SIGKILL);
		child->pid = 0;
	}
	if (child->fp[FD_IN] != NULL)
	{
		fclose(child->fp[FD_IN]);
		child->fp[FD_IN] = NULL;
	}
	if (child->fp[FD_OUT] != NULL)
	{
		fclose(child->fp[FD_OUT]);
		child->fp[FD_OUT] = NULL;
	}
}

/*
 * Closes the given block by killing the process, closing its file descriptor
 * and settings them to NULL after.
 */
void close_block(block_s *block)
{
	child_s *child = &block->child;

	if (child->pid > 1)
	{
		kill(child->pid, SIGTERM);
		//b->pid = 0; // TODO revert this, probably, just for testing!
	}
	if (child->fp[FD_OUT] != NULL)
	{
		fclose(child->fp[FD_OUT]);
		child->fp[FD_OUT] = NULL;
	}
}

/*
 * Convenience function: simply runs close_block() for all blocks.
 */
void close_blocks(state_s *state)
{
	for (size_t i = 0; i < state->num_blocks; ++i)
	{
		close_block(&state->blocks[i]);
	}
}

int open_spark(spark_s *spark)
{
	child_s *child = &spark->child;

	open_child(child, 0, 1, 0);
	if (child->pid == -1)
	{
		return -1;
	}

	fp_nonblocking(child->fp[FD_OUT]);
	return 0;
}

/*
 * Closes the trigger by closing its file descriptor
 * and sending a SIGTERM to the trigger command.
 * Also sets the file descriptor to NULL.
 */
void close_spark(spark_s *spark)
{
	child_s *child = &spark->child;

	// Is the trigger's command still running?
	if (child->pid > 1)
	{
		kill(child->pid, SIGTERM); // Politely ask to terminate
	}
	// If bar is set, then fd is a copy and will be closed elsewhere
	if (child->type == CHILD_LEMON) 
	//if (child->lemon)
	{
		return;
	}
	// Looks like we should actually close/free this fd after all
	if (child->fp[FD_OUT])
	{
		fclose(child->fp[FD_OUT]);
		child->fp[FD_OUT] = NULL;
		child->pid = 0;
	}
}

/*
 * Convenience function: simply opens all given triggers.
 * Returns the number of successfully opened triggers.
 */ 
size_t open_sparks(state_s *state)
{
	size_t num_sparks_opened = 0;
	for (size_t i = 0; i < state->num_sparks; ++i)
	{
		num_sparks_opened += (open_spark(&state->sparks[i]) == 0) ? 1 : 0;
	}
	return num_sparks_opened;
}

/*
 * Convenience function: simply closes all given triggers.
 */
void close_sparks(state_s *state)
{
	for (size_t i = 0; i < state->num_sparks; ++i)
	{
		close_spark(&state->sparks[i]);
	}
}

void free_spark(spark_s *t)
{
	// TODO
	/*
	free(t->cmd);
	t->cmd = NULL;

	t->block = NULL;
	t->lemon = NULL;
	*/
}

/*
 * Convenience function: simply frees all given blocks.
 */
void free_blocks(state_s *state)
{
	for (size_t i = 0; i < state->num_blocks; ++i)
	{
		free_block(&state->blocks[i]);
	}
}

/*
 * Convenience function: simply frees all given triggers.
 */
void free_sparks(state_s *state)
{
	for (size_t i = 0; i < state->num_sparks; ++i)
	{
		free_spark(&state->sparks[i]);
	}
}

/*
 * Executes the given block by calling open_block() on it and saves the output 
 * of the block, if any, in its `result` field. If the block was run for the 
 * first time, it will be marked as `used`. The `result_length` argument gives
 * the size of the buffer that will be used to fetch the block's output.
 * Returns 0 on success, -1 if the block could not be run or its output could
 * not be fetched.
 */
int run_block(block_s *b, size_t result_length)
{
	if (b->type == BLOCK_LIVE)
	{
		fprintf(stderr, "Block is live: `%s`\n", b->sid);
		return -1;
	}

	fprintf(stderr, "Attempting to open block `%s`\n", b->sid);

	open_block(b);
	if (b->child.fp[FD_OUT] == NULL)
	{
		fprintf(stderr, "Block is dead: `%s`\n", b->sid);
		close_block(b); // In case it has a PID already    TODO does this make sense?
		return -1;
	}
		
	// TODO maybe use getline() instead? It allocates a suitable buffer!
	char *result = malloc(result_length);
	if (fgets(result, result_length, b->child.fp[FD_OUT]) == NULL)
	{
		fprintf(stderr, "Unable to fetch input from block: `%s`\n", b->sid);
		if (feof(b->child.fp[FD_OUT]))   fprintf(stderr, "Reading from block failed (EOF): %s\n", b->sid);
		if (ferror(b->child.fp[FD_OUT])) fprintf(stderr, "Reading from block failed (err): %s\n", b->sid);
		close_block(b);
		return -1;
	}

	if (b->child.output != NULL)
	{
		free(b->child.output);
		b->child.output = NULL;
	}
	
	result[strcspn(result, "\n")] = 0; // Remove '\n'
	b->child.output = result; // Copy pointer to result over

	// Update the block's state accordingly
	//b->used = 1;     // Mark this block as having run at least once
	//b->waited = 0.0; // This block was last run... now!
	b->child.last = get_time();

	// Discard block's input, as it has now been processed
	free(b->child.input);
	b->child.input = NULL;

	// Close the block (unless it is a live block? We should rethink this)
	close_block(b);
	return 0;
}

/*
 * Given a block, it returns a pointer to a string that is the formatted result 
 * of this block's script output, ready to be fed to Lemonbar, including prefix,
 * label and suffix. The string is malloc'd and should be free'd by the caller.
 * If `len` is positive, it will be used as buffer size for the result string.
 * This means that `len` needs to be big enough to contain the fully formatted 
 * string this function is putting together, otherwise truncation will happen.
 * Alternatively, set `len` to 0 to let this function calculate the buffer.
 */
char *blockstr(const lemon_s *bar, const block_s *block, size_t len)
{
	char action_start[(5 * strlen(block->sid)) + 56]; // ... + (5 * 11) + 1
	action_start[0] = 0;
	char action_end[21]; // (5 * 4) + 1
	action_end[0] = 0;

	if (block->click_cfg.lmb)
	{
		strcat(action_start, "%{A1:");
		strcat(action_start, block->sid);
		strcat(action_start, "_lmb:}");
		strcat(action_end, "%{A}");
	}
	if (block->click_cfg.mmb)
	{
		strcat(action_start, "%{A2:");
		strcat(action_start, block->sid);
		strcat(action_start, "_mmb:}");
		strcat(action_end, "%{A}");
	}
	if (block->click_cfg.rmb)
	{
		strcat(action_start, "%{A3:");
		strcat(action_start, block->sid);
		strcat(action_start, "_rmb:}");
		strcat(action_end, "%{A}");
	}
	if (block->click_cfg.sup)
	{
		strcat(action_start, "%{A4:");
		strcat(action_start, block->sid);
		strcat(action_start, "_sup:}");
		strcat(action_end, "%{A}");
	}
	if (block->click_cfg.sdn)
	{
		strcat(action_start, "%{A5:");
		strcat(action_start, block->sid);
		strcat(action_start, "_sdn:}");
		strcat(action_end, "%{A}");
	}

	size_t diff;
	char *result = escape(block->child.output, '%', &diff);
	int padding = block->block_cfg.width + diff;

	size_t buf_len;

	if (len > 0)
	{
		// If len is given, we use that as buffer size
		buf_len = len;
	}
	else
	{
		// Required buffer mainly depends on the result and name of a block
		buf_len = 239; // format str = 100, known stuff = 138, '\0' = 1
		buf_len += strlen(action_start);
		buf_len += bar->block_cfg.prefix ? strlen(bar->block_cfg.prefix) : 0;
		buf_len += bar->block_cfg.suffix ? strlen(bar->block_cfg.suffix) : 0;
		buf_len += block->block_cfg.label ? strlen(block->block_cfg.label) : 0;
		buf_len += strlen(result);
	}

	const char *fg = strsel(block->block_cfg.fg, NULL, NULL);
	const char *bg = strsel(block->block_cfg.bg, bar->block_cfg.bg, NULL);
	const char *lc = strsel(block->block_cfg.lc, NULL, NULL);
	const char *label_fg = strsel(block->block_cfg.label_fg, bar->block_cfg.label_fg, fg);
	const char *label_bg = strsel(block->block_cfg.label_bg, bar->block_cfg.label_bg, bg);
	const char *affix_fg = strsel(block->block_cfg.affix_fg, bar->block_cfg.affix_fg, fg);
	const char *affix_bg = strsel(block->block_cfg.affix_bg, bar->block_cfg.affix_bg, bg);
        const int offset = (block->block_cfg.offset >= 0) ? block->block_cfg.offset : bar->block_cfg.offset;	
	const int ol = block->block_cfg.ol ? 1 : (bar->block_cfg.ol ? 1 : 0);
	const int ul = block->block_cfg.ul ? 1 : (bar->block_cfg.ul ? 1 : 0);

	char *str = malloc(buf_len);
	snprintf(str, buf_len,
		"%s%%{O%d}%%{F%s}%%{B%s}%%{U%s}%%{%co%cu}"        // start:  21
		"%%{T3}%%{F%s}%%{B%s}%s"                          // prefix: 13
		"%%{T2}%%{F%s}%%{B%s}%s"                          // label:  13
		"%%{T1}%%{F%s}%%{B%s}%*s"                         // block:  13
		"%%{T3}%%{F%s}%%{B%s}%s"                          // suffix: 13
		"%%{T-}%%{F-}%%{B-}%%{U-}%%{-o-u}%s",             // end:    27
		// Start
		action_start,                                     // strlen
		offset,                                           // max 4
		fg ? fg : "-",                                    // strlen, max 9
		bg ? bg : "-",                                    // strlen, max 9
		lc ? lc : "-",                                    // strlen, max 9
		ol ? '+' : '-',                                   // 1
		ul ? '+' : '-',                                   // 1
		// Prefix
		affix_fg ? affix_fg : "-",                        // strlen, max 9
		affix_bg ? affix_bg : "-",		          // strlen, max 9
		bar->block_cfg.prefix ? bar->block_cfg.prefix : "",    // strlen
		// Label
		label_fg ? label_fg : "-",                        // strlen, max 9
		label_bg ? label_bg : "-",                        // strlen, max 9
		block->block_cfg.label ? block->block_cfg.label : "",  // strlen
		// Block
		fg ? fg : "-",                                    // strlen, max 9
		bg ? bg : "-",                                    // strlen, max 9
		padding,                                          // max 4
		result,                                           // strlen
		// Suffix
		affix_fg ? affix_fg : "-",                        // strlen, max 9
		affix_bg ? affix_bg : "-",                        // strlen, max 9
		bar->block_cfg.suffix ? bar->block_cfg.suffix : "",    // strlen
		// End
		action_end                                        // 5*4
	);

	free(result);
	return str;
}

/*
 * Returns 'l', 'c' or 'r' for input values -1, 0 and 1 respectively.
 * For other input values, the behavior is undefined.
 */
char get_align(const int align)
{
	char a[] = {'l', 'c', 'r'};
	return a[align+1]; 
}

/*
 * Combines the results of all given blocks into a single string that can be fed
 * to Lemonbar. Returns a pointer to the string, allocated with malloc().
 */
char *barstr(const state_s *state)
{
	// For convenience...
	const lemon_s *bar = &state->lemon;
	const block_s *blocks = state->blocks;
	size_t num_blocks = state->num_blocks;

	// Short blocks like temperature, volume or battery, will usually use 
	// something in the range of 130 to 200 byte. So let's go with 256 byte.
	size_t bar_str_len = 256 * num_blocks; // TODO hardcoded value
	char *bar_str = malloc(bar_str_len);
	bar_str[0] = '\0';

	char align[5];
	int last_align = -1;

	const block_s *block = NULL;
	for (int i = 0; i < num_blocks; ++i)
	{
		block = &blocks[i];
		// TODO just quick hack to get this working, this shouldn't be required here!
		/*
		if (blocks[i].bin == NULL)
		{
			fprintf(stderr, "Block binary not given for '%s', skipping\n", blocks[i].name);
			continue;
		}
		*/

		// Live blocks might not have a result available
		if (block->child.output == NULL)
		{
			continue;
		}

		char *block_str = blockstr(bar, block, 0);
		size_t block_str_len = strlen(block_str);
		if (block->block_cfg.align != last_align)
		{
			last_align = block->block_cfg.align;
			snprintf(align, 5, "%%{%c}", get_align(last_align));
			strcat(bar_str, align);
		}
		// Let's check if this block string can fit in our buffer
		size_t free_len = bar_str_len - (strlen(bar_str) + 1);
		if (block_str_len > free_len)
		{
			// Let's make space for approx. two more blocks
			bar_str_len += 256 * 2; 
			bar_str = realloc(bar_str, bar_str_len);
		}
		strcat(bar_str, block_str);
		free(block_str);
	}
	strcat(bar_str, "\n");
	bar_str = realloc(bar_str, strlen(bar_str) + 1);
	return bar_str;
}

/*
 * TODO add comment, possibly some refactoring
 */
size_t feed_lemon(state_s *state, double delta, double tolerance, double *next)
{

	// Can't pipe to bar if its file descriptor isn't available
	if (state->lemon.child.fp[FD_IN] == NULL)
	{
		return -1;
	}
	
	// For convenience...
	lemon_s *bar = &state->lemon;
	block_s *blocks = state->blocks;
	size_t num_blocks = state->num_blocks;

	size_t num_blocks_executed = 0;	
	double until_next = DBL_MAX;
	double idle_left;

	block_s *block = NULL;
	double waited = 0.0;
	for (size_t i = 0; i < num_blocks; ++i)
	{
		block = &blocks[i];

		// Skip live blocks, they will update based on their output
		//if (blocks[i].live && blocks[i].result)
		// ^-- why did we do the '&& block[i].result' thing?
		if (block->type == BLOCK_LIVE)
		{
			// However, we count them as executed block so that
			// we actually end up updating the bar further down
			++num_blocks_executed;
			continue;
		}

		// Updated the time this block hasn't been run
		//blocks[i].waited += delta;
		waited = get_time() - block->child.last;

		// Calc how long until this block should be run
		//idle_left = blocks[i].reload - blocks[i].waited;
		idle_left = block->block_cfg.reload - waited;

		// Block was never run before OR block has input waiting OR
		// it's time to run this block according to it's reload option
		if (block->child.last == 0.0 || block->child.input || 
				(block->block_cfg.reload > 0.0 && idle_left < tolerance))
		{
			num_blocks_executed += 
				(run_block(block, BUFFER_SIZE) == 0) ? 1 : 0;
		}

		//idle_left = blocks[i].reload - blocks[i].waited; // Recalc!
		waited = get_time() - block->child.last;
		idle_left = block->block_cfg.reload - waited;

		// Possibly update the time until we should run feed_lemon again
		if (block->child.input == NULL && idle_left < until_next)
		{
			// If reload is 0, this block idles forever
			if (block->block_cfg.reload > 0.0)
			{
				until_next = (idle_left > 0.0) ? idle_left : 0.0;
			}
		}
	}
	*next = until_next;

	if (num_blocks_executed)
	{
		char *lemonbar_str = barstr(state);
		// TODO add error handling (EOF => bar dead?)
		fputs(lemonbar_str, bar->child.fp[FD_IN]);
		free(lemonbar_str);
	}
	return num_blocks_executed;
}

/*
 * Parses the format string for the bar, which should contain block names 
 * separated by whitespace and, optionally, up to two vertical bars to indicate 
 * alignment of blocks. For every block name found, the callback function `cb` 
 * will be run. Returns the number of block names found.
 */
size_t parse_format(const char *format, create_block_callback cb, void *data)
{
	if (format == NULL)
	{
		return 0;
	}

	size_t format_len = strlen(format) + 1;
	char block_name[BLOCK_NAME_MAX];
	block_name[0] = '\0';
	size_t block_name_len = 0;
	int block_align = -1;
	int num_blocks = 0;

	for (size_t i = 0; i < format_len; ++i)
	{
		switch (format[i])
		{
		case '|':
			// Next align
			block_align += block_align < 1;
		case ' ':
		case '\0':
			if (block_name_len)
			{
				// Block name complete, inform the callback
				cb(block_name, block_align, num_blocks++, data);
				// Prepare for the next block name
				block_name[0] = '\0';
				block_name_len = 0;
			}
			break;
		default:
			// Add the char to the current's block name
			block_name[block_name_len++] = format[i];
			block_name[block_name_len]   = '\0';
		}
	}

	// Return the number of blocks found
	return num_blocks;
}

/*
 * Finds and returns the block with the given `sid` -- or NULL.
 */
block_s *get_block(const state_s *state, const char *sid)
{
	// Iterate over all existing blocks and check for a name match
	for (size_t i = 0; i < state->num_blocks; ++i)
	{
		// If names match, return a pointer to this block
		if (equals(state->blocks[i].sid, sid))
		{
			return &state->blocks[i];
		}
	}
	return NULL;
}

/*
 * Add the block with the given SID to the collection of blocks, unless there 
 * is already a block with that SID present. 
 * Returns a pointer to the added (or existing) block or NULL in case of error.
 */
block_s *add_block(state_s *state, const char *sid)
{
	// See if there is an existing block by this name (and return, if so)
	//block_s *eb = get_block(state->blocks, state->num_blocks, name);
	block_s *eb = get_block(state, sid);
	if (eb)
	{
		return eb;
	}

	// Resize the block container to be able to hold one more block
	int current = state->num_blocks;
	state->num_blocks += 1;
	state->blocks = realloc(state->blocks, sizeof(block_s) * state->num_blocks);
	
	// Create the block, setting its name and default values
	state->blocks[current] = (block_s) { .sid = strdup(sid) };
	init_block(&state->blocks[current]);

	// Return a pointer to the new block
	return &state->blocks[current];
}

/*
 * inih doc: "Handler should return nonzero on success, zero on error."
 */
int lemon_cfg_handler(void *data, const char *section, const char *name, const char *value)
{
	state_s *state = (state_s*) data;

	// Only process if section is empty or specificially for bar
	if (empty(section) || equals(section, state->lemon.sid))
	{
		return lemon_ini_handler(&state->lemon, section, name, value);
	}

	return 1;
}

/*
 * inih doc: "Handler should return nonzero on success, zero on error."
 */
int block_cfg_handler(void *data, const char *section, const char *name, const char *value)
{
	state_s *state = (state_s*) data;

	// Abort if section is empty
	if (empty(section))
	{
		return 1;
	}
	
	// Abort if section is specifically for bar, not a block
	if (equals(section, state->lemon.sid))
	{
		return 1;
	}

	// Find the block whose name fits the section name
	block_s *block = get_block(state, section);

	// Abort if we couldn't find that block
	if (block == NULL)
	{
		return 1;
	}

	// Process via the appropriate handler
	return block_ini_handler(block, section, name, value);
}

/*
 * Load the config and parse the section for the bar, ignoring other sections.
 * Returns 0 on success, -1 on file open error, -2 on memory allocation error, 
 * -3 if no config file path was given in the preferences, or the line number 
 * of the first encountered parse error.
 */
static int load_lemon_cfg(state_s *state)
{
	// Abort if config file path empty or NULL
	if (empty(state->prefs.config))
	{
		return -3;
	}

	// Fire up the INI parser
	return ini_parse(state->prefs.config, lemon_cfg_handler, state);
}

/*
 * Load the config and parse all sections apart from the bar section.
 * Returns 0 on success, -1 on file open error, -2 on memory allocation error, 
 * -3 if no config file path was given in the preferences, or the line number 
 * of the first encountered parse error.
 */
static int load_block_cfg(state_s *state)
{
	// Abort if config file path empty or NULL
	if (empty(state->prefs.config))
	{
		return -3;
	}

	return ini_parse(state->prefs.config, block_cfg_handler, state);
}

/*
 * Find the event for the given lemon/block/spark or NULL if not found.
 */
event_s *get_event(state_s *state, void *thing, child_type_e ev_type, fdesc_type_e fd_type)
{
	for (size_t i = 0; i < state->num_events; ++i)
	{
		if (state->events[i].ev_type != ev_type)
		{
			continue;
		}
		if (state->events[i].fd_type != fd_type)
		{
			continue;
		}
		if (state->events[i].data == thing)
		{
			return &state->events[i];
		}
	}
	return NULL;
}

/*
 * TODO We could theoretically use this function to register events that aren't 
 *      part of the state's event array, as we don't perform any checks in this 
 *      regard -- what can/should we do about this?
 */
int register_event(state_s *state, event_s *ev)
{
	if (ev == NULL)
	{
		return -1;
	}

	if (ev->fd < 0)
	{
		return -1;
	}

	struct epoll_event eev = { 0 };
	eev.data.ptr = ev;
	eev.events = (ev->fd_type == FD_IN ? EPOLLOUT : EPOLLIN) | EPOLLET;

	if (epoll_ctl(state->epfd, EPOLL_CTL_ADD, ev->fd, &eev) == 0)
	{
		// Success
		ev->registered = 1;
		return 0;
	}
	if (errno == EEXIST)
	{
		// fd was already registered!
		ev->registered = 1;
	}
	// Some other error
	return -1;
}

/*
 * 
 */
int unregister_event(state_s *state, event_s *ev)
{
	if (ev == NULL)
	{
		return -1;
	}

	if (ev->fd < 0)
	{
		return -1;
	}

	if (epoll_ctl(state->epfd, EPOLL_CTL_DEL, ev->fd, NULL) == 0)
	{
		// Success!
		ev->fd = -1;
		ev->registered = 0;
		return 0;
	}
	if (errno == EBADF)
	{
		// fd isn't valid
		ev->fd = -1;
		ev->registered = 0;
	}
	else if (errno == ENOENT)
	{
		// fd wasn't registered to begin with
		ev->registered = 0;
	}
	// Some other error
	return -1;
}

/*
 * Register all events that have a valid file descriptor.
 */
size_t register_events(state_s *state)
{
	size_t num_registered = 0;

	event_s *event = NULL;
	for (size_t i = 0; i < state->num_events; ++i)
	{
		event = &state->events[i];
		fprintf(stderr, "Registering for fd=%d (%d)\n", event->fd, event->ev_type);
		int res = register_event(state, event);
		num_registered += (res == 0);
	}
	return num_registered;
}

event_s *add_event(state_s *state, child_type_e ev_type, fdesc_type_e fd_type, void *thing)
{
	// See if there is an existing event that matches the given params
	// TODO this iterates over all events, every time we call this function
	//      when that's not needed when we call it from create_events();
	//      hence we should make this optional via a flag or something...
	event_s *ee = get_event(state, thing, ev_type, fd_type);
	if (ee)
	{
		return ee;
	}

	// Resize the event array to be able to hold one more event
	int current = state->num_events;
	state->num_events += 1;
	state->events = realloc(state->events, sizeof(event_s) * state->num_events);
	 
	// Get the child of the thing
	child_s *child = NULL;
	switch (ev_type)
	{
		case CHILD_LEMON:
			child = &((lemon_s *)thing)->child;
			break;
		case CHILD_BLOCK:
			child = &((block_s *)thing)->child;
			break;
		case CHILD_SPARK:
			child = &((spark_s *)thing)->child;
			break;
	}

	// Create the event, setting all the important bits accordingly
	state->events[current] = (event_s) { 0 };
	state->events[current].ev_type = ev_type;
	state->events[current].fd_type = fd_type;
	state->events[current].data    = thing;
	state->events[current].fd      = child->fp[fd_type] ?
				fileno(child->fp[fd_type]) : -1;

	// Return a pointer to the new event
	return &state->events[current];
}

/*
 *
 */
size_t create_events(state_s *state)
{
	// Add LEMON
	// TODO do we also need to add an event for FD_ERR and/or FD_IN?
	add_event(state, CHILD_LEMON, FD_OUT, &state->lemon);

	// Add LIVE blocks
	block_s *block = NULL;
	for (size_t i = 0; i < state->num_blocks; ++i)
	{
		block = &state->blocks[i];

		if (block->type != BLOCK_LIVE)
		{
			continue;
		}

		fprintf(stderr, "Creating event for BLOCK %s\n", block->sid);
		add_event(state, CHILD_BLOCK, FD_OUT, block);
	}

	// Add SPARKs
	spark_s *spark = NULL;
	for (size_t i = 0; i < state->num_sparks; ++i)
	{
		spark = &state->sparks[i];

		fprintf(stderr, "Creating event for SPARK %s\n", spark->child.cmd);
		add_event(state, CHILD_SPARK, FD_OUT, spark);
	}
	
	return state->num_events;
}

size_t create_sparks(state_s *state)
{
	// No need for sparks if there aren't any blocks
	if (state->num_blocks == 0)
	{
		return 0;
	}

	// Use number of blocks as initial size 
	state->sparks = malloc(sizeof(spark_s) * state->num_blocks);
	size_t num_sparks = 0;
	
	block_s *block = NULL;
	// Go through all blocks, create sparks as appropriate
	for (size_t i = 0; i < state->num_blocks; ++i)
	{
		block = &state->blocks[i];
		
		// We only care about triggered/sparked blocks
		if (block->type != BLOCK_SPARKED)
		{
			continue;
		}

		state->sparks[num_sparks] = (spark_s) { 0 };
		init_spark(&state->sparks[num_sparks]);
		state->sparks[num_sparks].child.cmd = strdup(block->block_cfg.trigger);
		state->sparks[num_sparks].data = (void*) block;
		num_sparks += 1;
	}

	// Resize to whatever amount of memory we actually needed
	state->sparks = realloc(state->sparks, sizeof(spark_s) * num_sparks);
	state->num_sparks = num_sparks;
	return num_sparks;
}

/*
 * Read all pending lines from the given trigger and store only the last line 
 * in the corresponding block's input field. Previous lines will be discarded.
 * Returns the number of lines read from the trigger's file descriptor.
 */
size_t run_spark(spark_s *spark)
{
	if (spark->child.fp[FD_OUT] == NULL)
	{
		return 0;
	}

	// We're only going to deal with BLOCK sparks for now... TODO
	if (spark->type != CHILD_BLOCK)
	{
		return 0;
	}

	char res[BUFFER_SIZE];
	size_t num_lines = 0;

	while (fgets(res, BUFFER_SIZE, spark->child.fp[FD_OUT]) != NULL)
	{
		++num_lines;
	}

	if (num_lines)
	{
		block_s *block = (block_s*) spark->data;

		// Make sure we clear out any previous input
		free(block->child.input);
		block->child.input = NULL;

		// For live blocks, this will be the actual output
		if (block->type == BLOCK_LIVE)
		{
			block->child.output = strdup(res);
			// Remove '\n'
			block->child.output[strcspn(block->child.output, "\n")] = 0;
		}
		// For regular blocks, this will be input for the block
		else
		{
			block->child.input = strdup(res);
		}
	}
	
	return num_lines;
}

/*
 * Takes a string that might represent an action that was registered with one 
 * of the blocks and tries to find the associated block. If found, the command
 * associated with the action will be executed.
 * Returns 0 on success, -1 if the string was not a recognized action command
 * or the block that the action belongs to could not be found.
 */
int process_action(const state_s *state, const char *action)
{
	size_t len = strlen(action);
	if (len < 5)
	{
		return -1;	// Can not be an action command, too short
	}

	// A valid action command should have the format <blockname>_<cmd-type>
	// For example, for a block named `datetime` that was clicked with the 
	// left mouse button, `action` should be "datetime_lmb"

	char types[5][5] = {"_lmb", "_mmb", "_rmb", "_sup", "_sdn"};

	// Extract the type suffix, including the underscore
	char type[5]; 
	snprintf(type, 5, "%s", action + len - 5);

	// Extract everything _before_ the suffix (this is the block name)
	char block[len-4];
	snprintf(block, len - 4, "%s", action); 

	// We check if the action type is valid (see types)
	int b = 0;
	int found = 0;
	for (; b < 5; ++b)
	{
		if (equals(type, types[b]))
		{
			found = 1;
			break;
		}
	}

	// Not a recognized action type
	if (!found)
	{
		return -1;
	}

	// Find the source block of the action
	block_s *source = get_block(state, block);
	if (source == NULL)
	{
		return -1;
	}

	// Now to fire the right command for the action type
	switch (b) {
		case 0:
			run_cmd(source->click_cfg.lmb);
			return 0;
		case 1:
			run_cmd(source->click_cfg.mmb);
			return 0;
		case 2:
			run_cmd(source->click_cfg.rmb);
			return 0;
		case 3:
			run_cmd(source->click_cfg.sup);
			return 0;
		case 4:
			run_cmd(source->click_cfg.sdn);
			return 0;
		default:
			// Should never happen...
			return -1;
	}
}

/*
 * This callback is supposed to be called for every block name that is being 
 * extracted from the config file's 'format' option for the bar itself, which 
 * lists the blocks to be displayed on the bar. `name` should contain the name 
 * of the block as read from the format string, `align` should be -1, 0 or 1, 
 * meaning left, center or right, accordingly (indicating where the block is 
 * supposed to be displayed on the bar).
 */
static void found_block_handler(const char *name, int align, int n, void *data)
{
	// 'Unpack' the data
	state_s *state = (state_s*) data;
	
	// Find or add the block with the given name
	block_s *block = add_block(state, name);
	
	// Set the block's align to the given one
	block->block_cfg.align = align;
}

/*
 * Handles SIGINT signals (CTRL+C) by setting the static variable
 * `running` to 0, effectively ending the main loop, so that clean-up happens.
 */
void sigint_handler(int sig)
{
	running = 0;
	handled = sig;
}

void sigchld_handler(int sig)
{
	sigchld = 1;
	//sigchld = waitpid(-1, NULL, WNOHANG);
}

void reap_children(state_s *state)
{
	int pid = 0;
	while((pid = waitpid(-1, NULL, WNOHANG)))
	{
		fprintf(stderr, "This guy quit on us: %d\n", pid);
		block_s *block = NULL;
		for (size_t i = 0; i < state->num_blocks; ++i)
		{
			block = &state->blocks[i];
			if (block->child.pid != pid)
			{
				continue;
			}
			fprintf(stderr, "waitpid(): %s (pid=%d) dead\n", block->sid, block->child.pid);
			close_block(block);
			block->child.pid = 0;
		}
	}
}

// http://courses.cms.caltech.edu/cs11/material/general/usage.html
void help(const char *invocation)
{
	fprintf(stderr, "USAGE\n");
	fprintf(stderr, "\t%s [OPTIONS...]\n", invocation);
	fprintf(stderr, "\n");
	fprintf(stderr, "OPTIONS\n");
	fprintf(stderr, "\t-e\n");
	fprintf(stderr, "\t\tRun bar even if it is empty (no blocks).\n");
	fprintf(stderr, "\t-h\n");
	fprintf(stderr, "\t\tPrint this help text and exit.\n");
	fprintf(stderr, "\t-s\n");
	fprintf(stderr, "\t\tINI section name for the bar.\n");
}

int main(int argc, char **argv)
{
	/*
	 * SIGNALS
	 */

	// Prevent zombie children during runtime
	// https://en.wikipedia.org/wiki/Child_process#End_of_life
	
	// TODO However, maybe it would be a good idea to handle
	//      this signal, as children will send it to the parent
	//      process when they exit; so we could use this to detect
	//      blocks that have died (prematurely/unexpectedly), for 
	//      example those that failed the `execvp()` call from 
	//      within `fork()` in `popen_noshell()`... not sure.
	struct sigaction sa_chld = {
	//	.sa_handler = SIG_IGN
		.sa_handler = &sigchld_handler
	};
	if (sigaction(SIGCHLD, &sa_chld, NULL) == -1)
	{
		fprintf(stderr, "Failed to ignore children's signals\n");
	}

	// Make sure we still do clean-up on SIGINT (ctrl+c)
	// and similar signals that indicate we should quit.
	struct sigaction sa_int = {
		.sa_handler = &sigint_handler
	};
	if (sigaction(SIGINT, &sa_int, NULL) == -1)
	{
		fprintf(stderr, "Failed to register SIGINT handler\n");
	}
	if (sigaction(SIGQUIT, &sa_int, NULL) == -1)
	{
		fprintf(stderr, "Failed to register SIGQUIT handler\n");
	}
	if (sigaction (SIGTERM, &sa_int, NULL) == -1)
	{
		fprintf(stderr, "Failed to register SIGTERM handler\n");
	}
	if (sigaction (SIGPIPE, &sa_int, NULL) == -1)
	{
		fprintf(stderr, "Failed to register SIGPIPE handler\n");
	}

	/*
	 * CHECK IF X IS RUNNING
	 */

	char *display = getenv("DISPLAY");
	if (!display)
	{
		fprintf(stderr, "DISPLAY environment variable not set, aborting.\n");
		return EXIT_FAILURE;
	}
	if (!strstr(display, ":"))
	{
		fprintf(stderr, "DISPLAY environment variable invalid, aborting.\n");
		return EXIT_FAILURE;
	}

	/*
	 * INITIALIZE SUCCADE STATE
	 */

	state_s  state = { 0 };
	prefs_s *prefs = &(state.prefs); // For convenience
	lemon_s *lemon = &(state.lemon); // For convenience

	/*
	 * PARSE COMMAND LINE ARGUMENTS
	 */

	parse_args(argc, argv, prefs);
	char *default_cfg_path = NULL;

	/*
	 * PRINT HELP AND EXIT, MAYBE
	 */

	if (prefs->help)
	{
		help(argv[0]);
		return EXIT_SUCCESS;
	}

	/*
	 * PREFERENCES / DEFAULTS
	 */

	// If no custom config file given, set it to the default
	if (prefs->config == NULL)
	{
		// We use the additional variable for consistency with free()
		default_cfg_path = config_path(DEFAULT_CFG_FILE, SUCCADE_NAME);
		prefs->config = default_cfg_path; 
	}

	// If no custom INI section for bar given, set it to default
	if (prefs->section == NULL)
	{
		prefs->section = DEFAULT_LEMON_SECTION;
	}

	/*
	 * BAR
	 */

	// Copy the Section ID from the config for convenience and consistency
	lemon->sid = strdup(prefs->section);

	// Read the config file and parse bar's section
	if (load_lemon_cfg(&state) < 0)
	{
		fprintf(stderr, "Failed to load config file: %s\n", prefs->config);
		return EXIT_FAILURE;
	}

	// If no `bin` option was present in the config, set it to the default
	if (empty(lemon->lemon_cfg.bin))
	{
		// We use strdup() for consistency with free() later on
		lemon->lemon_cfg.bin = strdup(DEFAULT_LEMON_BIN);
	}

	// If no `name` option was present in the config, set it to the default
	if (empty(lemon->lemon_cfg.name))
	{
		// We use strdup() for consistency with free() later on
		lemon->lemon_cfg.name = strdup(DEFAULT_LEMON_NAME);
	}

	// Open (run) the lemon
	if (open_lemon(lemon) == -1)
	{
		fprintf(stderr, "Failed to open bar: %s\n", lemon->sid);
		return EXIT_FAILURE;
	}

	/*
	 * BLOCKS
	 */

	// Create blocks by parsing the format string
	size_t parsed = parse_format(lemon->lemon_cfg.format, found_block_handler, &state);

	fprintf(stderr, "Number of blocks: parsed = %zu, configured = %zu\n", 
			parsed, state.num_blocks);

	// Exit if no blocks could be loaded and 'empty' option isn't present
	if (state.num_blocks == 0 && prefs->empty == 0)
	{
		fprintf(stderr, "No blocks loaded, stopping %s.\n", SUCCADE_NAME);
		return EXIT_FAILURE;
	}

	if (load_block_cfg(&state) < 0)
	{
		fprintf(stderr, "Failed to load config file: %s\n", prefs->config);
		return EXIT_FAILURE;
	}

	if (DEBUG)
	{
		for (size_t i = 0; i < state.num_blocks; ++i)
		{
			fprintf(stderr, "Block #%zu: %s -> %s\n", i, 
					state.blocks[i].sid,
					state.blocks[i].block_cfg.bin);
		}
	}

	/*
	 * SPARKS - fire when their respective commands produce output
	 */

	create_sparks(&state);
	size_t num_sparks_opened = open_sparks(&state);
	
	if (DEBUG)
	{
		fprintf(stderr, "Number of sparks: parsed = %zu, opened = %zu\n", 
				state.num_sparks, num_sparks_opened);
	}

	/* 
	 * EVENTS - create epoll instance and save it in the state
	 */

	state.epfd = epoll_create(1);
	if (state.epfd < 0)
	{
		fprintf(stderr, "Could not create epoll file descriptor\n");
		return EXIT_FAILURE;
	}

	/*
	 * EVENTS - create event structs for all things that need 'em
	 */

	create_events(&state);

	/*
	 * EVENTS - register events with a valid file descriptor
	 */

	size_t num_events_registered = register_events(&state);
	
	if (DEBUG)
	{
		fprintf(stderr, "Number of events: created = %zu, registered = %zu\n",
				state.num_events, num_events_registered);
	}

	/*
	 * MAIN LOOP
	 */

	double now;
	double before = get_time();
	double delta;
	double wait = 0.0; // Will later be set to suitable value by feed_lemon()

	int max_events = state.num_sparks + 1;
	struct epoll_event tev[max_events];

	//char bar_output[BUFFER_SIZE];
	//bar_output[0] = '\0';

	running = 1;
	
	while (running)
	{
		now    = get_time();
		delta  = now - before;
		before = now;

		fprintf(stderr, "> wait = %f, delta = %f\n", wait, delta);
		if (sigchld)
		{
			reap_children(&state);
			sigchld = 0;
		}
		
		// Wait for trigger input - at least bartrig is always present
		// First time we call it, wait is 0, which is nice, because it allows us to 
		// start up all the blocks and shit without a delay
		int num_events = epoll_wait(state.epfd, tev, max_events, wait * MILLISEC_PER_SEC);

		// Mark all events with activity as dirty
		for (int i = 0; i < num_events; ++i)
		{
			event_s *ev = tev[i].data.ptr;

			if (tev[i].events & EPOLLIN)
			{
				fprintf(stderr, "*** EPOLLIN\n");
				ev->dirty = 1;
			}
			if (tev[i].events & EPOLLERR)
			{
				fprintf(stderr, "*** EPOLLERR\n");
				// TODO deal with this... but how?
			}
			if (tev[i].events & EPOLLHUP)
			{
				fprintf(stderr, "*** EPOLLHUP\n");
				// TODO deal with this... but how?
			}
		}

		// TODO Handle dirty events
		event_s *event = NULL;
		for (size_t i = 0; i < state.num_events; ++i)
		{
			event = &state.events[i];

			if (event->dirty)
			{
				//handle_event(&state.events[i]);
				switch (event->ev_type)
				{
					case CHILD_LEMON:
						fprintf(stderr, "*** LEMON EVENT\n");
						break;
					case CHILD_BLOCK:
						fprintf(stderr, "*** BLOCK EVENT\n");
						break;
					case CHILD_SPARK:
						fprintf(stderr, "*** SPARK EVENT\n");
						break;
				}
				event->dirty = 0;
			}
		}	

		// Handle BLOCKS
		// TODO open blocks that aren't open yet
		// TODO open sparks that aren't open yet
		// TODO blocks can have a trigger AND reload, don't run them twice..
		//      also, reload=0.0 alone doesn't mean STATIC, it could be a 
		//      triggered one. we need some more elegant logic here! 
		block_s *block = NULL;
		double now = get_time();
		double waited = 0.0;

		for (size_t i = 0; i < state.num_blocks; ++i)
		{
			block  = &state.blocks[i];
			waited = now - block->child.last;

			// TIMED BLOCK
			if (block->type == BLOCK_TIMED)
			{
				// Update the time this block has waited
				//state.blocks[i].child.waited += delta;

				// Block has never been run before, do it now!
				//if (state.blocks[i].used == 0)
				if (block->child.last == 0.0)
				{
					fprintf(stderr, "*** RUN TIMED BLOCK: %s\n", block->sid);
					//state.blocks[i].waited = 0.0;
					block->child.last = now;
					continue;
				}
				//if (state.blocks[i].waited >= state.blocks[i].reload)
				if (waited >= block->block_cfg.reload)
				{
					fprintf(stderr, "*** RUN TIMED BLOCK: %s\n", block->sid);
					//state.blocks[i].waited = 0.0;
					block->child.last = now;
					continue;
				}
			}

			// SPARKED BLOCK
			if (block->type == BLOCK_SPARKED)
			{
				fprintf(stderr, "*** RUN SPARKED BLOCK: %s\n", block->sid);
				//state.blocks[i].used = 1;
				block->child.last = now;
				//free(state.blocks[i].input);
				free(block->child.input);
				//state.blocks[i].input = NULL;
				block->child.input = NULL;
			}

			// STATIC BLOCK
			//else if (state.blocks[i].reload == 0.0)
			else if (block->type == BLOCK_ONCE)
			{
				if (block->child.last == 0.0)
				//if (state.blocks[i].used == 0)
				{
					fprintf(stderr, "*** RUN STATIC BLOCK: %s\n", block->sid);
					//state.blocks[i].used = 1;
					block->child.last = now;
					continue;
				}
			}
		}

		// Fetch input from all marked triggers
		/*
		for (int i = 0; i < state.num_sparks; ++i)
		{
			if (state.sparks[i].ready)
			{
				run_spark(&state.sparks[i]);
			}
		}
		*/

		/*
		// Let's see if Lemonbar produced any output
		if (bartrig.ready)
		{
			fgets(bar_output, BUFFER_SIZE, lemonbar.fp[FD_OUT]);
			bartrig.ready = 0;
		}
		*/

		/*
		// Let's process bar's output, if any
		if (strlen(bar_output))
		{
			if (process_action(&state, bar_output) < 0)
			{
				// It wasn't a recognized command, so chances are
				// that is was some debug/error output of bar.
				// TODO just use stderr in addition to stdout
				fprintf(stderr, "Lemonbar: %s", bar_output);
			}
			bar_output[0] = '\0';
		}
		*/

		// Let's update bar! 
		//feed_lemon(&state, delta, BLOCK_WAIT_TOLERANCE, &wait);
		fputs("HELLO WORLD\n", state.lemon.child.fp[FD_IN]);
		wait = 2.0;
	}

	/*
	 * CLEAN UP
	 */

	fprintf(stderr, "Performing clean-up ...\n");
	close(state.epfd);

	free(default_cfg_path);

	// Close triggers - it's important we free these first as they might
	// point to instances of bar and/or blocks, which will lead to errors
	close_sparks(&state);
	free_sparks(&state);
	free(state.sparks);
	
	/*
	close_spark(&bartrig);
	free_spark(&bartrig);
	*/

	// Close blocks
	close_blocks(&state);
	free_blocks(&state);
	free(state.blocks);

	// Close bar
	close_lemon(&state.lemon);
	free_lemon(&state.lemon);

	fprintf(stderr, "Clean-up finished, see you next time!\n");

	return EXIT_SUCCESS;
}

