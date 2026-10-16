/* Preserve the unchanged syslog implementation. Clang considers its existing
 * declaration after a comment misleadingly indented; suppress only that
 * formatting warning for this production unit, with all sanitizer checks on. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#include "../syslog.c"
#pragma GCC diagnostic pop
