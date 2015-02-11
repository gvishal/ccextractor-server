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

int db_store_cc(char *line, size_t len);

int send_pr_to_parent();
int send_pr_to_buf(int is_changed);

int open_buf_file();
int open_txt_file();
int open_xds_file();
int open_srt_file();

int append_to_buf(const char *line, size_t len, char mode);
int append_to_txt(const char *line);
int append_to_xds(const char *line);
int append_to_srt(const char *line);

int creat_pr_dir(char **path, time_t *start);
char *file_path(id_t prgm_id, const char *dir, const char *ext);

void cleanup_parser();
void sigusr2_parser();
void sigusr1_parser();

#endif /* end of include guard: PARSER_H */

