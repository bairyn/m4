/* GNU m4 -- A simple macro processor
   Copyright 1991, 1992, 1993, 1994 Free Software Foundation, Inc.
  
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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307  USA
*/

#include <stdio.h>
#include <sys/stat.h>

#if (defined __STDC__ && __STDC__) || defined PROTOTYPES
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#include "m4private.h"

/* File for debugging output.  */
M4_GLOBAL_DATA FILE *m4_debug = NULL;

/* Obstack for trace messages.  */
static struct obstack trace;

static void m4_debug_set_file M4_PARAMS((FILE *));

/*----------------------------------.
| Initialise the debugging module.  |
`----------------------------------*/

void
m4_debug_init (void)
{
  m4_debug_set_file (stderr);
  obstack_init (&trace);
}

/*-----------------------------------------------------------------.
| Function to decode the debugging flags OPTS.  Used by main while |
| processing option -d, and by the builtin debugmode ().	   |
`-----------------------------------------------------------------*/

int
m4_debug_decode (const char *opts)
{
  int level;

  if (opts == NULL || *opts == '\0')
    level = M4_DEBUG_TRACE_DEFAULT;
  else
    {
      for (level = 0; *opts; opts++)
	{
	  switch (*opts)
	    {
	    case 'a':
	      level |= M4_DEBUG_TRACE_ARGS;
	      break;

	    case 'e':
	      level |= M4_DEBUG_TRACE_EXPANSION;
	      break;

	    case 'q':
	      level |= M4_DEBUG_TRACE_QUOTE;
	      break;

	    case 't':
	      level |= M4_DEBUG_TRACE_ALL;
	      break;

	    case 'l':
	      level |= M4_DEBUG_TRACE_LINE;
	      break;

	    case 'f':
	      level |= M4_DEBUG_TRACE_FILE;
	      break;

	    case 'p':
	      level |= M4_DEBUG_TRACE_PATH;
	      break;

	    case 'c':
	      level |= M4_DEBUG_TRACE_CALL;
	      break;

	    case 'i':
	      level |= M4_DEBUG_TRACE_INPUT;
	      break;

	    case 'x':
	      level |= M4_DEBUG_TRACE_CALLID;
	      break;

	    case 'V':
	      level |= M4_DEBUG_TRACE_VERBOSE;
	      break;

	    default:
	      return -1;
	    }
	}
    }

  /* This is to avoid screwing up the trace output due to changes in the
     debug_level.  */

  obstack_free (&trace, obstack_finish (&trace));

  return level;
}

/*------------------------------------------------------------------------.
| Change the debug output stream to FP.  If the underlying file is the	  |
| same as stdout, use stdout instead so that debug messages appear in the |
| correct relative position.						  |
`------------------------------------------------------------------------*/

static void
m4_debug_set_file (FILE *fp)
{
  struct stat stdout_stat, debug_stat;

  if (m4_debug != NULL && m4_debug != stderr && m4_debug != stdout)
    fclose (m4_debug);
  m4_debug = fp;

  if (m4_debug != NULL && m4_debug != stdout)
    {
      if (fstat (fileno (stdout), &stdout_stat) < 0)
	return;
      if (fstat (fileno (m4_debug), &debug_stat) < 0)
	return;

      if (stdout_stat.st_ino == debug_stat.st_ino
	  && stdout_stat.st_dev == debug_stat.st_dev)
	{
	  if (m4_debug != stderr)
	    fclose (m4_debug);
	  m4_debug = stdout;
	}
    }
}

/*-----------------------------------------------------------.
| Serialize files.  Used before executing a system command.  |
`-----------------------------------------------------------*/

void
m4_debug_flush_files (void)
{
  fflush (stdout);
  fflush (stderr);
  if (m4_debug != NULL && m4_debug != stdout && m4_debug != stderr)
    fflush (m4_debug);
}

/*-------------------------------------------------------------------------.
| Change the debug output to file NAME.  If NAME is NULL, debug output is  |
| reverted to stderr, and if empty debug output is discarded.  Return TRUE |
| iff the output stream was changed.					   |
`-------------------------------------------------------------------------*/

boolean
m4_debug_set_output (const char *name)
{
  FILE *fp;

  if (name == NULL)
    m4_debug_set_file (stderr);
  else if (*name == '\0')
    m4_debug_set_file (NULL);
  else
    {
      fp = fopen (name, "a");
      if (fp == NULL)
	return FALSE;

      m4_debug_set_file (fp);
    }
  return TRUE;
}

/*-----------------------------------------------------------------------.
| Print the header of a one-line debug message, starting by "m4 debug".	 |
`-----------------------------------------------------------------------*/

void
m4_debug_message_prefix (void)
{
  fprintf (m4_debug, "m4 debug: ");
  if (debug_level & M4_DEBUG_TRACE_FILE)
    fprintf (m4_debug, "%s: ", m4_current_file);
  if (debug_level & M4_DEBUG_TRACE_LINE)
    fprintf (m4_debug, "%d: ", m4_current_line);
}

/* The rest of this file contains the functions for macro tracing output.
   All tracing output for a macro call is collected on an obstack TRACE,
   and printed whenever the line is complete.  This prevents tracing
   output from interfering with other debug messages generated by the
   various builtins.  */

/*---------------------------------------------------------------------.
| Tracing output is formatted here, by a simplified printf-to-obstack  |
| function trace_format ().  Understands only %S, %s, %d, %l (optional |
| left quote) and %r (optional right quote).			       |
`---------------------------------------------------------------------*/

#if (defined __STDC__ && __STDC__) || defined PROTOTYPES
static void
m4_trace_format (const char *fmt, ...)
#else
static void
m4_trace_format (va_alist)
     va_dcl
#endif
{
#if ! ((defined __STDC__ && __STDC__) || defined PROTOTYPES)
  const char *fmt;
#endif
  va_list args;
  char ch;

  int d;
  char nbuf[32];
  const char *s;
  int slen;
  int maxlen;

#if (defined __STDC__ && __STDC__) || defined PROTOTYPES
  va_start (args, fmt);
#else
  va_start (args);
  fmt = va_arg (args, const char *);
#endif

  while (TRUE)
    {
      while ((ch = *fmt++) != '\0' && ch != '%')
	obstack_1grow (&trace, ch);

      if (ch == '\0')
	break;

      maxlen = 0;
      switch (*fmt++)
	{
	case 'S':
	  maxlen = max_debug_argument_length;
	  /* fall through */

	case 's':
	  s = va_arg (args, const char *);
	  break;

	case 'l':
	  s = (debug_level & M4_DEBUG_TRACE_QUOTE) ? (char *)lquote.string : "";
	  break;

	case 'r':
	  s = (debug_level & M4_DEBUG_TRACE_QUOTE) ? (char *)rquote.string : "";
	  break;

	case 'd':
	  d = va_arg (args, int);
	  sprintf (nbuf, "%d", d);
	  s = nbuf;
	  break;

	default:
	  s = "";
	  break;
	}

      slen = strlen (s);
      if (maxlen == 0 || maxlen > slen)
	obstack_grow (&trace, s, slen);
      else
	{
	  obstack_grow (&trace, s, maxlen);
	  obstack_grow (&trace, "...", 3);
	}
    }

  va_end (args);
}

/*------------------------------------------------------------------.
| Format the standard header attached to all tracing output lines.  |
`------------------------------------------------------------------*/

static void
m4_trace_header (int id)
{
  m4_trace_format ("m4trace:");
  if (debug_level & M4_DEBUG_TRACE_FILE)
    m4_trace_format ("%s:", m4_current_file);
  if (debug_level & M4_DEBUG_TRACE_LINE)
    m4_trace_format ("%d:", m4_current_line);
  m4_trace_format (" -%d- ", m4_expansion_level);
  if (debug_level & M4_DEBUG_TRACE_CALLID)
    m4_trace_format ("id %d: ", id);
}

/*----------------------------------------------------.
| Print current tracing line, and clear the obstack.  |
`----------------------------------------------------*/

static void
m4_trace_flush (void)
{
  char *line;

  obstack_1grow (&trace, '\0');
  line = obstack_finish (&trace);
  M4_DEBUG_PRINT1 ("%s\n", line);
  obstack_free (&trace, line);
}

/*-------------------------------------------------------------.
| Do pre-argument-collction tracing for macro NAME.  Used from |
| expand_macro ().					       |
`-------------------------------------------------------------*/

void
m4_trace_prepre (const char *name, int id)
{
  m4_trace_header (id);
  m4_trace_format ("%s ...", name);
  m4_trace_flush ();
}

/*-----------------------------------------------------------------------.
| Format the parts of a trace line, that can be made before the macro is |
| actually expanded.  Used from expand_macro ().			 |
`-----------------------------------------------------------------------*/

void
m4_trace_pre (const char *name, int id, int argc, m4_token_data **argv)
{
  int i;
  const m4_builtin *bp;

  m4_trace_header (id);
  m4_trace_format ("%s", name);

  if (argc > 1 && (debug_level & M4_DEBUG_TRACE_ARGS))
    {
      m4_trace_format ("(");

      for (i = 1; i < argc; i++)
	{
	  if (i != 1)
	    m4_trace_format (", ");

	  switch (M4_TOKEN_DATA_TYPE (argv[i]))
	    {
	    case M4_TOKEN_TEXT:
	      m4_trace_format ("%l%S%r", M4_TOKEN_DATA_TEXT (argv[i]));
	      break;

	    case M4_TOKEN_FUNC:
	      bp = m4_builtin_find_by_func (NULL, M4_TOKEN_DATA_FUNC(argv[i]));
	      if (bp == NULL)
		{
		  M4ERROR ((warning_status, 0, _("\
INTERNAL ERROR: Builtin not found in builtin table! (m4_trace_pre ())")));
		  abort ();
		}
	      m4_trace_format ("<%s>", bp->name);
	      break;

	    default:
	      M4ERROR ((warning_status, 0, _("\
INTERNAL ERROR: Bad token data type (m4_trace_pre ())")));
	      abort ();
	    }

	}
      m4_trace_format (")");
    }

  if (debug_level & M4_DEBUG_TRACE_CALL)
    {
      m4_trace_format (" -> ???");
      m4_trace_flush ();
    }
}

/*-------------------------------------------------------------------.
| Format the final part of a trace line and print it all.  Used from |
| expand_macro ().						     |
`-------------------------------------------------------------------*/

void
m4_trace_post (const char *name, int id, int argc, m4_token_data **argv,
	    const char *expanded)
{
  if (debug_level & M4_DEBUG_TRACE_CALL)
    {
      m4_trace_header (id);
      m4_trace_format ("%s%s", name, (argc > 1) ? "(...)" : "");
    }

  if (expanded && (debug_level & M4_DEBUG_TRACE_EXPANSION))
    m4_trace_format (" -> %l%S%r", expanded);
  m4_trace_flush ();
}