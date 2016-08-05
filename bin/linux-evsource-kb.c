/*
 * 2014 (c) Øyvind Kolås
Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#include <sys/time.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <signal.h>
#include "mmm.h"
#include "host.h"
#include "linux-evsource.h"

#ifndef MIN
#define MIN(a,b)  (((a)<(b))?(a):(b))
#endif
#ifndef MAX
#define MAX(a,b)  (((a)>(b))?(a):(b))
#endif

static int evsource_kb_has_event (void);
static char *evsource_kb_get_event (void);
static void evsource_kb_destroy (int sign);
static int evsource_kb_get_fd (void);

/* kept out of struct to be reachable by atexit */
static EvSource ev_src_kb = {
  NULL,
  (void*)evsource_kb_has_event,
  (void*)evsource_kb_get_event,
  (void*)evsource_kb_destroy,
  (void*)evsource_kb_get_fd,
  NULL
}; 

static struct termios orig_attr;

int is_active (Host *host);
static void real_evsource_kb_destroy (int sign)
{
  static int done = 0;
  
  if (sign == 0)
    return;

  if (done)
    return;
  done = 1;

  /* will be called from atexit without self */
  switch (sign)
  {
    case  -11:break;
    case   SIGSEGV: fprintf (stderr, " SIGSEGV\n");break;
    case   SIGABRT: fprintf (stderr, " SIGABRT\n");break;
    case   SIGBUS: fprintf (stderr, " SIGBUS\n");break;
    case   SIGKILL: fprintf (stderr, " SIGKILL\n");break;
    case   SIGINT: fprintf (stderr, " SIGINT\n");break;
    case   SIGTERM: fprintf (stderr, " SIGTERM\n");break;
    case   SIGQUIT: fprintf (stderr, " SIGQUIT\n");break;
    default: fprintf (stderr, "sign: %i\n", sign);
             fprintf (stderr, "%i %i %i %i %i %i %i\n", SIGSEGV, SIGABRT, SIGBUS, SIGKILL, SIGINT, SIGTERM, SIGQUIT);
  }
  tcsetattr (STDIN_FILENO, TCSAFLUSH, &orig_attr);
}


static void evsource_kb_destroy (int sign)
{
  real_evsource_kb_destroy (-11);
}

static int evsource_kb_init ()
{
//  ioctl(STDIN_FILENO, KDSKBMODE, K_RAW);
  atexit ((void*) real_evsource_kb_destroy);
  signal (SIGSEGV, (void*) real_evsource_kb_destroy);
  signal (SIGABRT, (void*) real_evsource_kb_destroy);
  signal (SIGBUS,  (void*) real_evsource_kb_destroy);
  signal (SIGKILL, (void*) real_evsource_kb_destroy);
  signal (SIGINT,  (void*) real_evsource_kb_destroy);
  signal (SIGTERM, (void*) real_evsource_kb_destroy);
  signal (SIGQUIT, (void*) real_evsource_kb_destroy);

  struct termios raw;
  if (tcgetattr (STDIN_FILENO, &orig_attr) == -1)
    {
      fprintf (stderr, "error initializing keyboard\n");
      return -1;
    }
  raw = orig_attr;

  cfmakeraw (&raw);

  raw.c_cc[VMIN] = 1; raw.c_cc[VTIME] = 0; /* 1 byte, no timer */
  if (tcsetattr (STDIN_FILENO, TCSAFLUSH, &raw) < 0)
    return 0; // XXX? return other value?

  return 0;
}

static int evsource_kb_has_event (void)
{
  struct timeval tv;
  int retval;

  fd_set rfds;
  FD_ZERO (&rfds);
  FD_SET(STDIN_FILENO, &rfds);
  tv.tv_sec = 0; tv.tv_usec = 0;
  retval = select (STDIN_FILENO+1, &rfds, NULL, NULL, &tv);
  return retval == 1;
}

/* note that a nick can have multiple occurences, the labels
 * should be kept the same for all occurences of a combination.
 *
 * this table is taken from nchanterm.
 */
typedef struct MmmKeyCode {
  char *nick;          /* programmers name for key */
  char *label;         /* utf8 label for key */
  char  sequence[10];  /* terminal sequence */
} MmmKeyCode;
static const MmmKeyCode ufb_keycodes[]={  
  {"up",                  "↑",     "\e[A"},
  {"down",                "↓",     "\e[B"}, 
  {"right",               "→",     "\e[C"}, 
  {"left",                "←",     "\e[D"}, 

  {"shift-up",            "⇧↑",    "\e[1;2A"},
  {"shift-down",          "⇧↓",    "\e[1;2B"},
  {"shift-right",         "⇧→",    "\e[1;2C"},
  {"shift-left",          "⇧←",    "\e[1;2D"},

  {"alt-up",              "^↑",    "\e[1;3A"},
  {"alt-down",            "^↓",    "\e[1;3B"},
  {"alt-right",           "^→",    "\e[1;3C"},
  {"alt-left",            "^←",    "\e[1;3D"},

  {"alt-shift-up",        "alt-s↑", "\e[1;4A"},
  {"alt-shift-down",      "alt-s↓", "\e[1;4B"},
  {"alt-shift-right",     "alt-s→", "\e[1;4C"},
  {"alt-shift-left",      "alt-s←", "\e[1;4D"},

  {"control-up",          "^↑",    "\e[1;5A"},
  {"control-down",        "^↓",    "\e[1;5B"},
  {"control-right",       "^→",    "\e[1;5C"},
  {"control-left",        "^←",    "\e[1;5D"},

  /* putty */
  {"control-up",          "^↑",    "\eOA"},
  {"control-down",        "^↓",    "\eOB"},
  {"control-right",       "^→",    "\eOC"},
  {"control-left",        "^←",    "\eOD"},

  {"control-shift-up",    "^⇧↑",   "\e[1;6A"},
  {"control-shift-down",  "^⇧↓",   "\e[1;6B"},
  {"control-shift-right", "^⇧→",   "\e[1;6C"},
  {"control-shift-left",  "^⇧←",   "\e[1;6D"},

  {"control-up",          "^↑",    "\eOa"},
  {"control-down",        "^↓",    "\eOb"},
  {"control-right",       "^→",    "\eOc"},
  {"control-left",        "^←",    "\eOd"},

  {"shift-up",            "⇧↑",    "\e[a"},
  {"shift-down",          "⇧↓",    "\e[b"},
  {"shift-right",         "⇧→",    "\e[c"},
  {"shift-left",          "⇧←",    "\e[d"},

  {"insert",              "ins",   "\e[2~"},
  {"delete",              "del",   "\e[3~"},
  {"page-up",             "PgUp",  "\e[5~"},
  {"page-down",           "PdDn",  "\e[6~"},
  {"home",                "Home",  "\eOH"},
  {"end",                 "End",   "\eOF"},
  {"home",                "Home",  "\e[H"},
  {"end",                 "End",   "\e[F"},
  {"control-delete",      "^del",  "\e[3;5~"},
  {"shift-delete",        "⇧del",  "\e[3;2~"},
  {"control-shift-delete","^⇧del", "\e[3;6~"},

  {"F1",        "F1",  "\e[11~"},
  {"F2",        "F2",  "\e[12~"},
  {"F3",        "F3",  "\e[13~"},
  {"F4",        "F4",  "\e[14~"},
  {"F1",        "F1",  "\eOP"},
  {"F2",        "F2",  "\eOQ"},
  {"F3",        "F3",  "\eOR"},
  {"F4",        "F4",  "\eOS"},
  {"F5",        "F5",  "\e[15~"},
  {"F6",        "F6",  "\e[16~"},
  {"F7",        "F7",  "\e[17~"},
  {"F8",        "F8",  "\e[18~"},
  {"F9",        "F9",  "\e[19~"},
  {"F9",        "F9",  "\e[20~"},
  {"F10",       "F10", "\e[21~"},
  {"F11",       "F11", "\e[22~"},
  {"F12",       "F12", "\e[23~"},
  {"tab",       "↹",  {9, '\0'}},
  {"shift-tab", "shift+↹",  "\e[Z"},
  {"backspace", "⌫",  {127, '\0'}},
  {"space",     "␣",   " "},
  {"\e",        "␛",  "\e"},
  {"return",    "⏎",  {10,0}},
  {"return",    "⏎",  {13,0}},
  /* this section could be autogenerated by code */
  {"control-a", "^A",  {1,0}},
  {"control-b", "^B",  {2,0}},
  {"control-c", "^C",  {3,0}},
  {"control-d", "^D",  {4,0}},
  {"control-e", "^E",  {5,0}},
  {"control-f", "^F",  {6,0}},
  {"control-g", "^G",  {7,0}},
  {"control-h", "^H",  {8,0}}, /* backspace? */
  {"control-i", "^I",  {9,0}},
  {"control-j", "^J",  {10,0}},
  {"control-k", "^K",  {11,0}},
  {"control-l", "^L",  {12,0}},
  {"control-n", "^N",  {14,0}},
  {"control-o", "^O",  {15,0}},
  {"control-p", "^P",  {16,0}},
  {"control-q", "^Q",  {17,0}},
  {"control-r", "^R",  {18,0}},
  {"control-s", "^S",  {19,0}},
  {"control-t", "^T",  {20,0}},
  {"control-u", "^U",  {21,0}},
  {"control-v", "^V",  {22,0}},
  {"control-w", "^W",  {23,0}},
  {"control-x", "^X",  {24,0}},
  {"control-y", "^Y",  {25,0}},
  {"control-z", "^Z",  {26,0}},
  {"alt-0",     "%0",  "\e0"},
  {"alt-1",     "%1",  "\e1"},
  {"alt-2",     "%2",  "\e2"},
  {"alt-3",     "%3",  "\e3"},
  {"alt-4",     "%4",  "\e4"},
  {"alt-5",     "%5",  "\e5"},
  {"alt-6",     "%6",  "\e6"},
  {"alt-7",     "%7",  "\e7"}, /* backspace? */
  {"alt-8",     "%8",  "\e8"},
  {"alt-9",     "%9",  "\e9"},
  {"alt-+",     "%+",  "\e+"},
  {"alt--",     "%-",  "\e-"},
  {"alt-/",     "%/",  "\e/"},
  {"alt-a",     "%A",  "\ea"},
  {"alt-b",     "%B",  "\eb"},
  {"alt-c",     "%C",  "\ec"},
  {"alt-d",     "%D",  "\ed"},
  {"alt-e",     "%E",  "\ee"},
  {"alt-f",     "%F",  "\ef"},
  {"alt-g",     "%G",  "\eg"},
  {"alt-h",     "%H",  "\eh"}, /* backspace? */
  {"alt-i",     "%I",  "\ei"},
  {"alt-j",     "%J",  "\ej"},
  {"alt-k",     "%K",  "\ek"},
  {"alt-l",     "%L",  "\el"},
  {"alt-n",     "%N",  "\em"},
  {"alt-n",     "%N",  "\en"},
  {"alt-o",     "%O",  "\eo"},
  {"alt-p",     "%P",  "\ep"},
  {"alt-q",     "%Q",  "\eq"},
  {"alt-r",     "%R",  "\er"},
  {"alt-s",     "%S",  "\es"},
  {"alt-t",     "%T",  "\et"},
  {"alt-u",     "%U",  "\eu"},
  {"alt-v",     "%V",  "\ev"},
  {"alt-w",     "%W",  "\ew"},
  {"alt-x",     "%X",  "\ex"},
  {"alt-y",     "%Y",  "\ey"},
  {"alt-z",     "%Z",  "\ez"},
  /* Linux Console  */
  {"home",      "Home", "\e[1~"},
  {"end",       "End",  "\e[4~"},
  {"F1",        "F1",   "\e[[A"},
  {"F2",        "F2",   "\e[[B"},
  {"F3",        "F3",   "\e[[C"},
  {"F4",        "F4",   "\e[[D"},
  {"F5",        "F5",   "\e[[E"},
  {"F6",        "F6",   "\e[[F"},
  {"F7",        "F7",   "\e[[G"},
  {"F8",        "F8",   "\e[[H"},
  {"F9",        "F9",   "\e[[I"},
  {"F10",       "F10",  "\e[[J"},
  {"F11",       "F11",  "\e[[K"},
  {"F12",       "F12",  "\e[[L"}, 
  {NULL, }
};

static int fb_keyboard_match_keycode (const char *buf, int length, const MmmKeyCode **ret)
{
  int i;
  int matches = 0;

  if (!strncmp (buf, "\e[M", MIN(length,3)))
    {
      if (length >= 6)
        return 9001;
      return 2342;
    }
  for (i = 0; ufb_keycodes[i].nick; i++)
    if (!strncmp (buf, ufb_keycodes[i].sequence, length))
      {
        matches ++;
        if (strlen (ufb_keycodes[i].sequence) == length && ret)
          {
            *ret = &ufb_keycodes[i];
            return 1;
          }
      }
  if (matches != 1 && ret)
    *ret = NULL;
  return matches==1?2:matches;
}

static int ufb_utf8_len (const unsigned char first_byte)
{
  if      ((first_byte & 0x80) == 0)
    return 1; /* ASCII */
  else if ((first_byte & 0xE0) == 0xC0)
    return 2;
  else if ((first_byte & 0xF0) == 0xE0)
    return 3;
  else if ((first_byte & 0xF8) == 0xF0)
    return 4;
  return 1;
}

static char *evsource_kb_get_event (void)
{
  unsigned char buf[20];
  int length;


  for (length = 0; length < 10; length ++)
    if (read (STDIN_FILENO, &buf[length], 1) != -1)
      {
        const MmmKeyCode *match = NULL;

        if (!is_active (ev_src_kb.priv))
           return NULL;

        /* special case ESC, so that we can use it alone in keybindings */
        if (length == 0 && buf[0] == 27)
          {
            struct timeval tv;
            fd_set rfds;
            FD_ZERO (&rfds);
            FD_SET (STDIN_FILENO, &rfds);
            tv.tv_sec = 0;
            tv.tv_usec = 1000 * 120;
            if (select (1, &rfds, NULL, NULL, &tv) == 0)
              return strdup ("\033");
          }

        switch (fb_keyboard_match_keycode ((void*)buf, length + 1, &match))
          {
            case 1: /* unique match */
              if (!match)
                return NULL;
              return strdup (match->nick);
              break;
            case 0: /* no matches, bail*/
              { 
                static char ret[256];
                if (length == 0 && ufb_utf8_len (buf[0])>1) /* read a 
                                                             * single unicode
                                                             * utf8 character
                                                             */
                  {
                    read (STDIN_FILENO, &buf[length+1], ufb_utf8_len(buf[0])-1);
                    buf[ufb_utf8_len(buf[0])]=0;
                    strcpy (ret, (void*)buf);
                    return strdup(ret); //XXX: simplify
                  }
                if (length == 0) /* ascii */
                  {
                    buf[1]=0;
                    strcpy (ret, (void*)buf);
                    return strdup(ret);
                  }
                sprintf (ret, "unhandled %i:'%c' %i:'%c' %i:'%c' %i:'%c' %i:'%c' %i:'%c' %i:'%c'",
                    length >=0 ? buf[0] : 0, 
                    length >=0 ? buf[0]>31?buf[0]:'?' : ' ', 
                    length >=1 ? buf[1] : 0, 
                    length >=1 ? buf[1]>31?buf[1]:'?' : ' ', 
                    length >=2 ? buf[2] : 0, 
                    length >=2 ? buf[2]>31?buf[2]:'?' : ' ', 
                    length >=3 ? buf[3] : 0, 
                    length >=3 ? buf[3]>31?buf[3]:'?' : ' ',
                    length >=4 ? buf[4] : 0, 
                    length >=4 ? buf[4]>31?buf[4]:'?' : ' ',
                    length >=5 ? buf[5] : 0, 
                    length >=5 ? buf[5]>31?buf[5]:'?' : ' ',
                    length >=6 ? buf[6] : 0, 
                    length >=6 ? buf[6]>31?buf[6]:'?' : ' '
                    );
                return strdup(ret);
              }
              return NULL;
            default: /* continue */
              break;
          }
      }
    else
      return strdup("key read eek");
  return strdup("fail");
}

static int evsource_kb_get_fd (void)
{
  return STDIN_FILENO;
}


EvSource *evsource_kb_new (void)
{
  if (evsource_kb_init() == 0)
  {
    return &ev_src_kb;
  }
  return NULL;
}
