#ifndef __DEBUG_H__
#define __DEBUG_H__

#ifdef DEBUG
#define dbg_string(s) print_string(s)
#define dbg_string_x(s) print_string_x(s)
#define dbg_byte(s) print_byte(s)
#define dbg_short(s) print_short(s)
#define dbg_char(s) write_char(s)
#else
#define dbg_string(s)
#define dbg_string_x(s)
#define dbg_byte(s)
#define dbg_short(s)
#define dbg_char(s)
#endif

#endif
