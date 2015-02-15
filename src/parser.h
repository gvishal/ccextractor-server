#ifndef PARSER_H
#define PARSER_H

#include <sys/types.h>

/** Forks parser process. Created process reads output from ccextractor,
 * parces it and stores in files and db.
 *
 * @param id client id
 * @param cce_output c-string with CCExtractor output file path
 * @param pipe_w write end (descriptor) of pipe to client
 * @return in parent process: client pid on success, -1 otherwise 
 */
pid_t fork_parser(id_t id, const char *cce_output, int pipe_w);

/** Opens CCExtractor output file. Reads it line by line in infinite
 * loop. Calls parse_line() when full line is received.
 *
 * @param cce_output c-string with CCExtractor output file path
 * @return -1 on error, 1 on success
 */
int parser_loop(const char *cce_output);

/** Appedns line to db, output files and website buffer. If line is XDS 
 * then checks if program changed.
 *
 * @param line line data
 * @param len size of line
 * @return -1 on error, 1 on success
 */
// TODO: remove len. line is c-string
int parse_line(char *line, size_t len);

/** Checks if line contains XDS data.
 *
 * @param line c-string with line to check
 * @return NULL if it is not XDS. Pointer to the beginning of XDS 
 * data (skipping time stuff) otherwise.
 */
char *is_xds(char *line);

/** Checks if XDS data tells that program is changed.
 *
 * @param line c-string with XDS data (from is_xds())
 * @return NULL if program didn't change, Pointer to malloc'ed string with
 * program name without 'special' symbols.
 */
// TODO: rename line to xds_data
char *is_program_changed(char *line);

/** Handles program change event ie reopens output files, updates db tables and
 * internal data, sends notification to web site and client process.
 *
 * Program is changed when
 * 1. XDS line received with different program name
 * 2. cfd.pr_report_time seconds passed sinse last XDS line with program name.
 * 3. cfg.pr_timeout seconds passed since last program change 
 * (if program name is not being reported) (see check_pr_timeout()) 
 *
 * @param new_name new program name. If NULL, then program name considered empty
 * @return -1 on error, 1 on success
 */
int set_pr(char *new_name);

/** Checks if cfg.pr_timeout seconds have passed since last program change and
 * sets empty program name.
 *
 * @return -1 on error, 1 on success
 */
int check_pr_timout();

/** Extracts cc text from parsed line and sends it to db.
 *
 * @param line cc line string
 * @param len lenght of data in line
 * @return -1 on error, 1 on success
 */
// TODO: remove len
int db_store_cc(char *line, size_t len);

/** Notify client process about programm change by sending path to new BIN file
 *
 * @return -1 on error, 1 on success
 */
int send_pr_to_parent();

/** Sends program id, output files dir, program name to web site buffer
 *
 * @param is_changed if TRUE then PROGRAM_CHANGED would be send, meaning that 
 * program changed and output files have changed. Otherwise, PROGRAM_NEW would 
 * be send meaning that program didn't change, but we received program name and
 * should update it's name in web site.
 * @return -1 on error, 1 on success
 */
int send_pr_to_buf(int is_changed);

/** Creates and opens web site buffer file. Inits global 'buf' var. Sets line 
 * bufferring (_IOLBF)
 *
 * @return -1 on error, 1 on success
 */
int open_buf_file();

/** Creates and opens TXT output file. Inits global 'buf' var. Sets line 
 * bufferring (_IOLBF)
 *
 * @return -1 on error, 1 on success
 */
int open_txt_file();

/** Creates and opens XDS output file. Inits global 'buf' var. Sets line 
 * bufferring (_IOLBF)
 *
 * @return -1 on error, 1 on success
 */
int open_xds_file();

/** Creates and opens SRT output file. Inits global 'buf' var. Sets line 
 * bufferring (_IOLBF)
 *
 * @return -1 on error, 1 on success
 */
int open_srt_file();

/** Ouputs line to web site buffer. 
 *
 * If the file has more than cfg.buf_max_lines
 * lines then cuts it to cfg.buf_min_lines from the beginning. 
 * Locks buf file before writing to prevent collisions with client process
 * If \p line or \p len is 0, then only \p mode would be written
 *
 * @param line data to write
 * @param len size of data in line buffer
 * @param mode command specifing data type 
 * @return -1 on error, 1 on success
 * @see delete_n_lines
 */
// TODO: rename mode to command
int append_to_buf(const char *line, size_t len, char mode);

/** Ouputs line to TXT file. Opens this file if it is not opened
 * 
 * @param line c-string with data to write
 * @return -1 on error, 1 on success
 */
int append_to_txt(const char *line);

/** Ouputs line to XDS file. Opens this file if it is not opened
 * 
 * @param line c-string with data to write
 * @return -1 on error, 1 on success
 */
int append_to_xds(const char *line);

/** Ouputs line to SRT file. Opens this file if it is not opened. 
 * Converts line from TXT format to SRT
 * 
 * @param line c-string with data to write
 * @return -1 on error, 1 on success
 */
int append_to_srt(const char *line);

/** Creates dirs in cfg.arch_dir for specified time. Created dir shall 
 * has the name in "Year/month/day" format
 *
 * @param path on success, it would point to malloc'ed dir path
 * @param start time to extract dirs names
 * @return -1 on error, 1 on success
 */
int creat_pr_dir(char **path, time_t *start);

/** Constructs path for specified program in following format:
 * "dir/progrman_id.extension"
 *
 * @return malloc'ed path string on success, NULL otherwise
 */
char *file_path(id_t prgm_id, const char *dir, const char *ext);

/**  Sets program end date in db, closes opened files
 */
void cleanup_parser();

/** SIGUSR2 handler. sets sigusr2_receiver flag which is checked while 
 * reading CCExtractor output. This signal is send from client process
 * when connected clinet closes connention. sigusr2_receiver is used to
 * finish reading data from CCExtractor output.
 */
void sigusr2_parser();

/** SIGUSR1 handler. Terminates clients process. This signal is send form
 * serever process when it terminates (on error or SIGINT)
 */
void sigusr1_parser();

#endif /* end of include guard: PARSER_H */

