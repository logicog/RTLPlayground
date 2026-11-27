#ifndef _CMD_PARSE_HISTORY_H_
#define _CMD_PARSE_HISTORY_H_

/* "history" command and the command history ring buffer */
void cmd_parse_history(void);
void cmd_history_save(void);
void clear_command_history(void) __banked;

#endif
