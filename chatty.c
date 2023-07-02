// usage: 
//  ( 1) chatty --session=<session name>                              ; continue a conversation in the session <session name> accepting user text from stdin and printing chat bot responses to stdout
//  ( 2) chatty                                                       ; automatically continue the most recent conversation
//  ( 3) chatty --once --prompt="<prompt file>"                       ; run a single conversation using the prompt file <prompt file> but don't save the conversation
//  ( 4) chatty --new-session=<session name> --prompt="<prompt file>" ; start a new conversation with session name <session name> and prompt from file <prompt file>
//  ( 5) chatty --retry                                               ; get a new response from the most recent conversation using the last user text
//  ( 6) chatty --session=<session name> --retry                      ; same as (6) but for a specific session
//  ( 7) chatty --prompt-from=<session name>                          ; get the prompt text from the session <session name>
//  ( 8) chatty --delete=<session name>                               ; delete the session <session name>
//  ( 9) chatty --delete-all                                          ; tell the user to delete the $XDG_DATA_HOME/chatty/sessions directory if they want to delete all sessions
//  (10) chatty --list                                                ; list all sessions
//  (11) chatty --export=<session name>                               ; export the session <session name> to stdout
//  (12) chatty --import=<session name>                               ; import the session <session name> from stdin
//  (13) chatty --rollback                                            ; remove the user text and response from the most recent conversation
//  (14) chatty --session=<session name> --rollback                   ; remove the user text and response from the session <session name>
//  (15) chatty --help                                                ; print this help message

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chatty_methods.h"

#define CHATTY_RETRY_MASK 1
#define CHATTY_NEW_SESSION_MASK 2
#define CHATTY_PROMPT_FROM_MASK 4
#define CHATTY_DELETE_MASK 8
#define CHATTY_DELETE_ALL_MASK 16
#define CHATTY_LIST_MASK 32
#define CHATTY_EXPORT_MASK 64
#define CHATTY_IMPORT_MASK 128
#define CHATTY_ROLLBACK_MASK 256
#define CHATTY_HELP_MASK 512
#define CHATTY_SESSION_MASK 1024
#define CHATTY_PROMPT_MASK 2048
#define CHATTY_ONCE_MASK 4096

struct
chatty_options
{
  char *progname;
  char *session;
  char *prompt;

  unsigned int mask;
};

void
chatty_options_argument_parse_or_die (struct chatty_options *options, const char *argument, char *subargument)
{
  const char *arguments [] = {
    "--retry",
    "--new-session",
    "--prompt-from",
    "--delete",
    "--delete-all",
    "--list",
    "--export",
    "--import",
    "--rollback",
    "--help",
    "--session",
    "--prompt",
    "--once",
  };
  const unsigned int arguments_count = sizeof (arguments) / sizeof (arguments [0]);

  const unsigned int argument_masks [] = {
    CHATTY_RETRY_MASK,
    CHATTY_NEW_SESSION_MASK,
    CHATTY_PROMPT_FROM_MASK,
    CHATTY_DELETE_MASK,
    CHATTY_DELETE_ALL_MASK,
    CHATTY_LIST_MASK,
    CHATTY_EXPORT_MASK,
    CHATTY_IMPORT_MASK,
    CHATTY_ROLLBACK_MASK,
    CHATTY_HELP_MASK,
    CHATTY_SESSION_MASK,
    CHATTY_PROMPT_MASK,
    CHATTY_ONCE_MASK,
  };

  static_assert (sizeof (argument_masks) / sizeof (argument_masks [0]) == sizeof (arguments) / sizeof (arguments [0]), "argument_masks and arguments must have the same number of elements");

  char **argument_subargument_pointer [] =
  {
    NULL, &options->session, &options->session, &options->session, NULL, NULL, &options->session, &options->session, NULL, NULL, &options->session, &options->prompt, NULL,
  };

  for (unsigned int i = 0; i < arguments_count; i++)
  {
    if (strcmp (arguments [i], argument) == 0)
    {
      if (options->mask & argument_masks [i])
      {
        fprintf (stderr, "%s: error: duplicate argument: %s\n", options->progname, argument);
        exit (1);
      }

      options->mask |= argument_masks [i];

      if (argument_subargument_pointer [i] != NULL)
      {
        *argument_subargument_pointer [i] = subargument;
      }

      return;
    }
  }

  fprintf (stderr, "%s: error: unknown argument: %s\n", options->progname, argument);
  exit (1);
}

void
chatty_options_initialize_from_arguments_or_die (struct chatty_options *options, int argc, char **argv)
{
  if (argc == 0)
  {
    exit (1);
  }

  options->progname = argv [0];
  
  options->session = NULL;
  options->prompt = NULL;
  options->mask = 0;

  for (int i = 1; i < argc; i++)
  {
    char *argument = argv [i];
    char *subargument = NULL;

    if ((subargument = strchr (argument, '=')))
    {
      *subargument = '\0'; subargument++;
    }

    chatty_options_argument_parse_or_die (options, argument, subargument);
  }

  if (options->mask & CHATTY_HELP_MASK)
  {
    printf("usage: chatty [options]\n\n");
    printf("Options:\n");
    printf("  --help\n");
    printf("    Display this help message and exit.\n\n");
    printf("  --session=<session name>\n");
    printf("    Continue a conversation in the specified session <session name>. Accepts user\n");
    printf("    input from stdin and prints chat bot responses to stdout.\n\n");
    printf("  --once --prompt=\"<prompt file>\"\n");
    printf("    Run a single conversation using the prompt file <prompt file> without saving\n");
    printf("    the conversation.\n\n");
    printf("  --new-session=<session name> --prompt=\"<prompt file>\"\n");
    printf("    Start a new conversation with a session named <session name> and use the\n");
    printf("    prompt from file <prompt file>.\n\n");
    printf("  --retry\n");
    printf("    Get a new response from the most recent conversation using the last input.\n\n");
    printf("  --session=<session name> --retry\n");
    printf("    Same as --retry, but for a specific session.\n\n");
    printf("  --prompt-from=<session name>\n");
    printf("    Retrieve the prompt text from the specified session <session name>.\n\n");
    printf("  --list\n");
    printf("    List all available sessions.\n\n");
    printf("  --delete=<session name>\n");
    printf("    Delete the session <session name>.\n\n");
    printf("  --delete-all\n");
    printf("    In order to delete all sessions, please delete the $XDG_DATA_HOME/chatty/sessions\n");
    printf("    directory manually.\n\n");
    printf("  --export=<session name>\n");
    printf("    Export the specified session <session name> and print it to stdout.\n\n");
    printf("  --import=<session name>\n");
    printf("    Import a session named <session name> from stdin.\n\n");
    printf("  --rollback\n");
    printf("    Remove the input and response from the most recent conversation.\n\n");
    printf("  --session=<session name> --rollback\n");
    printf("    Remove the input and response from the specified session <session name>.\n\n");
    printf("If no options are provided, the program will automatically continue the most recent conversation.\n");
    exit (0);
  }

  if (options->mask & CHATTY_NEW_SESSION_MASK)
  {
    if ((options->mask & CHATTY_PROMPT_MASK) == 0 || options->prompt == NULL || *options->prompt == '\0')
    {
      fprintf (stderr, "%s: error: --new-session requires --prompt\n", options->progname);
      exit (1);
    }
  }

  if (options->mask & CHATTY_ONCE_MASK)
  {
    if ((options->mask & CHATTY_PROMPT_MASK) == 0 || options->prompt == NULL || *options->prompt == '\0')
    {
      fprintf (stderr, "%s: error: --once requires --prompt\n", options->progname);
      exit (1);
    }
  }

  unsigned int uses_session_mask = CHATTY_NEW_SESSION_MASK | CHATTY_PROMPT_FROM_MASK | CHATTY_DELETE_MASK | CHATTY_EXPORT_MASK | CHATTY_SESSION_MASK | CHATTY_IMPORT_MASK;

  if (options->mask & uses_session_mask)
  {
    if (options->session == NULL)
    {
      fprintf (stderr, "%s: error: session name must be provided.\n", options->progname);
      exit (1);
    }

    if (strlen (options->session) == 0)
    {
      fprintf (stderr, "%s: error: session name must not be empty.\n", options->progname);
      exit (1);
    }

    if (strcmp (options->session, ".") == 0 || strcmp (options->session, "..") == 0 || strchr (options->session, '/') != NULL || strchr (options->session, '\\') != NULL)
    {
      fprintf (stderr, "%s: error: session name must not be \".\" or \"..\" and must not contain a slash or a backslash\n", options->progname);
      exit (1);
    }
  }

  if (__builtin_popcount (options->mask) <= 1)
  {
    return;
  }

  unsigned int allowed_multiple_masks [] = { 
    CHATTY_NEW_SESSION_MASK | CHATTY_PROMPT_MASK, 
    CHATTY_ONCE_MASK | CHATTY_PROMPT_MASK,
    CHATTY_SESSION_MASK | CHATTY_RETRY_MASK,
    CHATTY_SESSION_MASK | CHATTY_ROLLBACK_MASK
  };
  
  unsigned int allowed_multiple_masks_count = sizeof (allowed_multiple_masks) / sizeof (allowed_multiple_masks [0]);

  for (unsigned int i = 0; i < allowed_multiple_masks_count; i++)
  {
    if (options->mask == allowed_multiple_masks [i])
    {
      return;
    }
  }
  
  fprintf (stderr, "%s: error: invalid combination of arguments\n", options->progname);
  exit (1);
}

int
main (int argc, char **argv)
{
  struct chatty_options options;
  chatty_options_initialize_from_arguments_or_die (&options, argc, argv);
  chatty_initialize_directories ();

  unsigned int mask = options.mask;

  if (mask == 0)
  {
    chatty_extend_session (NULL);
  }
  else if (mask & CHATTY_SESSION_MASK)
  {
    chatty_extend_session (options.session);
  }
  else if (mask & CHATTY_RETRY_MASK)
  {
    if (mask & CHATTY_SESSION_MASK)
    {
      chatty_retry_session (options.session);
    }
    else
    {
      chatty_retry_session (NULL);
    }
  }
  else if (mask & CHATTY_NEW_SESSION_MASK)
  {
    chatty_create_session (options.session, options.prompt);
  }
  else if (mask & CHATTY_DELETE_MASK)
  {
    chatty_delete_session (options.session);
  }
  else if (mask & CHATTY_DELETE_ALL_MASK)
  {
    chatty_delete_all_sessions ();
  }
  else if (mask & CHATTY_LIST_MASK)
  {
    chatty_list_sessions ();
  }
  else if (mask & CHATTY_ONCE_MASK)
  {
    chatty_once (options.prompt);
  }
  else if (mask & CHATTY_EXPORT_MASK)
  {
    chatty_export_session (options.session);
  }
  else if (mask & CHATTY_IMPORT_MASK)
  {
    chatty_import_session (options.session);
  }
  else
  {
    fprintf (stderr, "%s: chatty mask %u not implemented\n", options.progname, mask);
  }

  return 0;
}
