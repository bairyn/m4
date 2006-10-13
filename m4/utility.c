/* GNU m4 -- A simple macro processor
   Copyright (C) 1989, 1990, 1991, 1992, 1993, 1994, 1998, 1999, 2003,
   2006 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301  USA
*/

#include <config.h>

#include "m4private.h"

#include "exitfail.h"
#include "progname.h"
#include "verror.h"
#include "xvasprintf.h"

static const char * skip_space (m4 *, const char *);



/* Give friendly warnings if a builtin macro is passed an
   inappropriate number of arguments.  MIN is the 0-based minimum
   number of acceptable arguments, MAX is the 0-based maximum number
   or UINT_MAX if not applicable, and SIDE_EFFECT is true if the macro
   has side effects even if min is not satisfied.  ARGC is the 1-based
   count of ARGV, where ARGV[0] is the name of the macro.  Return true
   if the macro is guaranteed to expand to the empty string, false
   otherwise.  */
bool
m4_bad_argc (m4 *context, int argc, m4_symbol_value **argv,
	     unsigned int min, unsigned int max, bool side_effect)
{
  if (argc - 1 < min)
    {
      m4_warn (context, 0, _("%s: too few arguments: %d < %d"),
	       M4ARG (0), argc - 1, min);
      return ! side_effect;
    }

  if (argc - 1 > max)
    {
      m4_warn (context, 0, _("%s: extra arguments ignored: %d > %d"),
	       M4ARG (0), argc - 1, max);
    }

  return false;
}

static const char *
skip_space (m4 *context, const char *arg)
{
  while (m4_has_syntax (M4SYNTAX, (unsigned char) *arg, M4_SYNTAX_SPACE))
    arg++;
  return arg;
}

/* The function m4_numeric_arg () converts ARG to an int pointed to by
   VALUEP. If the conversion fails, print error message for macro.
   Return true iff conversion succeeds.  */
/* FIXME: Convert this to use gnulib's xstrtoimax, xstrtoumax.
   Otherwise, we are arbitrarily limiting integer values.  */
bool
m4_numeric_arg (m4 *context, int argc, m4_symbol_value **argv,
		int arg, int *valuep)
{
  char *endp;

  if (*M4ARG (arg) == '\0')
    {
      *valuep = 0;
      m4_warn (context, 0, _("%s: empty string treated as 0"), M4ARG (0));
    }
  else
    {
      *valuep = strtol (skip_space (context, M4ARG (arg)), &endp, 10);
      if (*skip_space (context, endp) != 0)
	{
	  m4_warn (context, 0, _("%s: non-numeric argument `%s'"),
		   M4ARG (0), M4ARG (arg));
	  return false;
	}
    }
  return true;
}


/* Print ARGC arguments from the table ARGV to obstack OBS, separated by
   SEP, and quoted by the current quotes, if QUOTED is true.  */
void
m4_dump_args (m4 *context, m4_obstack *obs, int argc,
	      m4_symbol_value **argv, const char *sep, bool quoted)
{
  int i;
  size_t len = strlen (sep);

  for (i = 1; i < argc; i++)
    {
      if (i > 1)
	obstack_grow (obs, sep, len);

      m4_shipout_string (context, obs, M4ARG (i), 0, quoted);
    }
}

/* Issue an error.  The message is printf-style, based on FORMAT and
   any other arguments, and the program name and location (if we are
   currently parsing an input file) are automatically prepended.  If
   ERRNUM is non-zero, include strerror output in the message.  If
   STATUS is non-zero, or if errors are fatal, call exit immediately;
   otherwise, remember that an error occurred so that m4 cannot exit
   with success later on.*/
void
m4_error (m4 *context, int status, int errnum, const char *format, ...)
{
  va_list args;
  int line = m4_get_current_line (context);
  assert (m4_get_current_file (context) || ! line);
  va_start (args, format);
  if (status == EXIT_SUCCESS && m4_get_fatal_warnings_opt (context))
    status = EXIT_FAILURE;
  verror_at_line (status, errnum, line ? m4_get_current_file (context) : NULL,
		  line, format, args);
  m4_set_exit_status (context, EXIT_FAILURE);
}

/* Issue an error.  The message is printf-style, based on FORMAT and
   any other arguments, and the program name and location (from FILE
   and LINE) are automatically prepended.  If ERRNUM is non-zero,
   include strerror output in the message.  If STATUS is non-zero, or
   if errors are fatal, call exit immediately; otherwise, remember
   that an error occurred so that m4 cannot exit with success later
   on.*/
void
m4_error_at_line (m4 *context, int status, int errnum, const char *file,
		  int line, const char *format, ...)
{
  va_list args;
  va_start (args, format);
  if (status == EXIT_SUCCESS && m4_get_fatal_warnings_opt (context))
    status = EXIT_FAILURE;
  verror_at_line (status, errnum, line ? file : NULL,
		  line, format, args);
  m4_set_exit_status (context, EXIT_FAILURE);
}

/* Issue a warning, if they are not being suppressed.  The message is
   printf-style, based on FORMAT and any other arguments, and the
   program name, location (if we are currently parsing an input file),
   and "Warning:" are automatically prepended.  If ERRNUM is non-zero,
   include strerror output in the message.  If warnings are fatal,
   call exit immediately, otherwise exit status is unchanged.  */
void
m4_warn (m4 *context, int errnum, const char *format, ...)
{
  if (!m4_get_suppress_warnings_opt (context))
    {
      va_list args;
      int status = EXIT_SUCCESS;
      int line = m4_get_current_line (context);
      char *full_format = xasprintf(_("Warning: %s"), format);

      assert (m4_get_current_file (context) || ! line);
      va_start (args, format);
      if (m4_get_fatal_warnings_opt (context))
	status = EXIT_FAILURE;
      /* If the full_format failed (unlikely though that may be), at
	 least fall back on the original format.  */
      verror_at_line (status, errnum,
		      line ? m4_get_current_file (context) : NULL, line,
		      full_format ? full_format : format, args);
      free (full_format);
    }
}

/* Issue a warning, if they are not being suppressed.  The message is
   printf-style, based on FORMAT and any other arguments, and the
   program name, location (from FILE and LINE), and "Warning:" are
   automatically prepended.  If ERRNUM is non-zero, include strerror
   output in the message.  If warnings are fatal, call exit
   immediately, otherwise exit status is unchanged.  */
void
m4_warn_at_line (m4 *context, int errnum, const char *file, int line,
		 const char *format, ...)
{
  if (!m4_get_suppress_warnings_opt (context))
    {
      va_list args;
      int status = EXIT_SUCCESS;
      char *full_format = xasprintf(_("Warning: %s"), format);
      va_start (args, format);
      if (m4_get_fatal_warnings_opt (context))
	status = EXIT_FAILURE;
      /* If the full_format failed (unlikely though that may be), at
	 least fall back on the original format.  */
      verror_at_line (status, errnum, line ? file : NULL, line,
		      full_format ? full_format : format, args);
      free (full_format);
    }
}


/* Wrap the gnulib progname module, to avoid exporting a global
   variable from a library.  Retrieve the program name for use in
   error messages and the __program__ macro.  */
const char *
m4_get_program_name (void)
{
  return program_name;
}

/* Wrap the gnulib progname module, to avoid exporting a global
   variable from a library.  Set the program name for use in error
   messages and the __program__ macro to argv[0].  */
void
m4_set_program_name (const char *name)
{
  set_program_name (name);
}

/* Wrap the gnulib exitfail module, to avoid exporting a global
   variable from a library.  Set the exit status for use in gnulib
   modules and atexit handlers.  */
void
m4_set_exit_failure (int status)
{
  exit_failure = status;
}
