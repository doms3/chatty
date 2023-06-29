// usage: 
//  ( 1) chatty --session=<session name>                              ; continue a conversation in the session <session name> accepting user text from stdin and printing chat bot responses to stdout
//  ( 2) chatty                                                       ; automatically continue the most recent conversation
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

#define _GNU_SOURCE

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__unix__) || defined(__APPLE__) || defined(__MACH__)
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#else
#error "Unsupported platform!"
#endif

#ifdef __linux__
#include <linux/limits.h> 
#elif __APPLE__
#include <sys/syslimits.h>
#else
#error "Unsupported platform!"
#endif

#include "aichat.h"
#include "chatty_methods.h"

#define CHATTY_MAYBE_DIE(x) if ((x) < 0) do { fprintf(stderr, "%s: libaichat error code: %d\n", program_invocation_short_name, -(x)); exit(1); } while (0)

static char chatty_home_directory [PATH_MAX];
static char chatty_session_directory [PATH_MAX];

void
chatty_initialize_directories (void)
{
  const char *session_path = getenv ("XDG_DATA_HOME");

  if (session_path)
  {
    int length = snprintf (chatty_home_directory, PATH_MAX, "%s/chatty", session_path);
    if (length < 0) goto chatty_initialize_directories_system_error;
    if (length >= PATH_MAX) goto chatty_initialize_directories_length_error;

    length = snprintf (chatty_session_directory, PATH_MAX, "%s/sessions", chatty_home_directory);
    if (length < 0) goto chatty_initialize_directories_system_error;
    if (length >= PATH_MAX) goto chatty_initialize_directories_length_error;
    
    if (mkdir (chatty_home_directory, 0775) < 0)
    {
      if (errno != EEXIST) goto chatty_initialize_directories_system_error;
    }

    if (mkdir (chatty_session_directory, 0775) < 0)
    {
      if (errno != EEXIST) goto chatty_initialize_directories_system_error;
    }

    return;
  }

  session_path = getenv ("HOME");

  if (session_path)
  {
    char chatty_local_directory [PATH_MAX];
    char chatty_local_share_directory [PATH_MAX];

    int length = snprintf (chatty_local_directory, PATH_MAX, "%s/.local", session_path);
    if (length < 0) goto chatty_initialize_directories_system_error;
    if (length >= PATH_MAX) goto chatty_initialize_directories_length_error;

    length = snprintf (chatty_local_share_directory, PATH_MAX, "%s/share", chatty_local_directory);
    if (length < 0) goto chatty_initialize_directories_system_error;
    if (length >= PATH_MAX) goto chatty_initialize_directories_length_error;

    length = snprintf (chatty_home_directory, PATH_MAX, "%s/chatty", chatty_local_share_directory);
    if (length < 0) goto chatty_initialize_directories_system_error;
    if (length >= PATH_MAX) goto chatty_initialize_directories_length_error;

    length = snprintf (chatty_session_directory, PATH_MAX, "%s/sessions", chatty_home_directory);
    if (length < 0) goto chatty_initialize_directories_system_error;
    if (length >= PATH_MAX) goto chatty_initialize_directories_length_error;
  
    char *directories_to_create [] = { chatty_local_directory, chatty_local_share_directory, chatty_home_directory, chatty_session_directory, NULL };
    char **iterator = directories_to_create;
  
    while (*iterator)
    {
      if (mkdir (*iterator, 0775) < 0)
      {
        if (errno != EEXIST) goto chatty_initialize_directories_system_error;
      }
      
      iterator++;
    }

    return;
  }

  fprintf (stderr, "%s: could not find session directory, one of $HOME and $XDG_DATA_HOME must be set\n", program_invocation_short_name);
  exit(1);
chatty_initialize_directories_system_error:
  fprintf (stderr, "%s: %s\n", program_invocation_short_name, strerror (errno));
  exit (1);
chatty_initialize_directories_length_error:
  fprintf (stderr, "%s: path exceeds maximum length\n", program_invocation_short_name);
  exit (1);
}

void
chatty_list_sessions (void)
{
  DIR *directory = opendir (chatty_session_directory);

  if (directory == NULL)
  {
    if (errno == ENOENT) return;
    fprintf (stderr, "%s: cannot access '%s': %s\n", program_invocation_short_name, chatty_session_directory, strerror (errno));
    exit (1);
  }

  bool found_last_session = false;
  char *last_session_path = NULL;
  int length = asprintf (&last_session_path, "%s/.last_session", chatty_home_directory);

  if (length < 0)
  {
    fprintf (stderr, "%s: %s\n", program_invocation_short_name, strerror (errno));
    exit (1);
  }

  FILE *file = fopen (last_session_path, "r+");
  free (last_session_path); last_session_path = NULL;

  if (file == NULL)
  {
    found_last_session = true; // don't bother trying to find the last session if we can't open the file
  }

  struct stat file_stat;
  if (fstat (fileno (file), &file_stat) < 0)
  {
    found_last_session = true; // don't bother trying to find the last session if we can't stat the file
  }

  unsigned int inode = file_stat.st_ino;

  struct dirent *entry;
  while ((entry = readdir (directory)))
  {
    if (entry->d_type != DT_REG) continue;

    printf ("%s", entry->d_name);

    if (found_last_session == false)
    {
      // we could have a false positive here if the $CHATTY_HOME/sessions folder
      // is on a different filesystem than the $CHATTY_HOME/.last_session file but that's fine here
      if (inode == entry->d_ino)
      {
        printf (" (last session)");
        found_last_session = true;
      }
    }

    printf ("\n");
  }

  closedir (directory);
}

void
chatty_delete_all_sessions (void)
{
  printf ("To delete all sessions, delete the directory '%s'\n", chatty_session_directory);
}

static char *
chatty_get_session_path_or_die (const char *session)
{
  char *session_path = NULL;
  int length;

  if (session)
  {
    length = asprintf (&session_path, "%s/%s", chatty_session_directory, session);
  }
  else
  {
    length = asprintf (&session_path, "%s/.last_session", chatty_home_directory);
  }

  if (length < 0)
  {
    fprintf (stderr, "%s: %s\n", program_invocation_short_name, strerror (errno));
    exit (1);
  }

  return session_path;
}

void
chatty_delete_session (const char *session)
{
  char *session_path = chatty_get_session_path_or_die (session);

  if (remove (session_path) != 0)
  {
    if (errno == ENOENT)
    {
      fprintf (stderr, "%s: session '%s' does not exist\n", program_invocation_short_name, session);
      exit (1);
    }

    fprintf (stderr, "%s: cannot delete '%s': %s\n", program_invocation_short_name, session_path, strerror (errno));
    exit (1);
  }

  free (session_path);
}

static void
chatty_extend_session_helper (struct aichat_session *session)
{
  struct aichat_api_call_results results;
  aichat_session_extend (session, &results);

  CHATTY_MAYBE_DIE (results.error);
  CHATTY_MAYBE_DIE (aichat_session_print_last_message (session, stdout));

  putchar ('\n');
}

static FILE *
chatty_open_session_file_or_die (const char *session, const char *mode, const char *err)
{
  char *session_path = chatty_get_session_path_or_die (session);
  FILE *file = fopen (session_path, mode);
  
  free (session_path);
  if (file == NULL)
  {
    if (errno == EEXIST || errno == ENOENT)
    {
      fprintf (stderr, "%s", program_invocation_short_name);
      
      if (session)
      {
        if (errno == EEXIST) fprintf (stderr, ": session '%s' already exists", session);
        else                 fprintf (stderr, ": session '%s' does not exist", session);
      }
      else
      {
        if (errno == EEXIST) fprintf (stderr, ": there is no last session");
        else                 fprintf (stderr, ": last session already exists");
      }

      if (*err)
      {
        fprintf (stderr, ": %s", err);
      }

      fprintf (stderr, "\n");
      exit (1);
    }

    fprintf (stderr, "%s: cannot open session: %s\n", program_invocation_short_name, strerror (errno));
    exit (1);
  }

  return file;
}


static void
chatty_set_last_session (const char *session)
{
  char *session_path = chatty_get_session_path_or_die (session);
  char *last_session_path = chatty_get_session_path_or_die (NULL);

  if (remove (last_session_path) != 0)
  {
    if (errno != ENOENT) goto chatty_set_last_session_error;
  }

  if (symlink (session_path, last_session_path) != 0) goto chatty_set_last_session_error;

  free (session_path);
  free (last_session_path);
  return;

chatty_set_last_session_error:
  fprintf (stderr, "%s: could not update last session: %s\n", program_invocation_short_name, strerror (errno));
  free (session_path);
  free (last_session_path);
  exit (1);
}

void
chatty_extend_session (const char *sessionname)
{
  const char *enoent = sessionname ? "use the --new-session option to create a new session" : "select a session using --session or create a new session using --new-session";

  FILE *file = chatty_open_session_file_or_die (sessionname, "r+", enoent);
  
  struct aichat_session session;
  CHATTY_MAYBE_DIE (aichat_session_initialize_from_json_file (&session, file));
  CHATTY_MAYBE_DIE (aichat_session_add_message_from_file (&session, AICHAT_ROLE_USER, stdin));

  chatty_extend_session_helper (&session);

  rewind (file);
  CHATTY_MAYBE_DIE (aichat_session_write_to_json_file (&session, file));
  fclose (file);

  if (sessionname) chatty_set_last_session (sessionname);
}


void
chatty_retry_session (const char *sessionname)
{
  const char *enoent = sessionname ? "" : "select a session using --session";

  FILE *file = chatty_open_session_file_or_die (sessionname, "r+", enoent);
  
  struct aichat_session session;
  CHATTY_MAYBE_DIE (aichat_session_initialize_from_json_file (&session, file));
  CHATTY_MAYBE_DIE (aichat_session_remove_last_message (&session));

  chatty_extend_session_helper (&session);

  rewind (file);
  CHATTY_MAYBE_DIE (aichat_session_write_to_json_file (&session, file));
  fclose (file);

  if (sessionname) chatty_set_last_session (sessionname);
}

void
chatty_create_session (const char *sessionname, const char *promptfile)
{
  FILE *prompt = fopen (promptfile, "r");

  if (prompt == NULL)
  {
    fprintf (stderr, "%s: cannot open '%s': %s\n", program_invocation_short_name, promptfile, strerror (errno));
    exit (1);
  }
  
  struct aichat_session session;
  aichat_session_initialize (&session);
  CHATTY_MAYBE_DIE (aichat_session_add_message_from_file (&session, AICHAT_ROLE_SYSTEM, prompt));
  fclose (prompt);

  CHATTY_MAYBE_DIE (aichat_session_add_message_from_file (&session, AICHAT_ROLE_USER, stdin));
  FILE *file = chatty_open_session_file_or_die (sessionname, "wx", "use the --session option to extend an existing session");
  
  chatty_extend_session_helper (&session);
  rewind (file);
  CHATTY_MAYBE_DIE (aichat_session_write_to_json_file (&session, file));
  fclose (file);

  if (sessionname) chatty_set_last_session (sessionname);
}

void
chatty_once (const char *promptfile)
{
  FILE *prompt = fopen (promptfile, "r");

  if (prompt == NULL)
  {
    fprintf (stderr, "%s: cannot open '%s': %s\n", program_invocation_short_name, promptfile, strerror (errno));
    exit (1);
  }

  struct aichat_session session;
  aichat_session_initialize (&session);
  CHATTY_MAYBE_DIE (aichat_session_add_message_from_file (&session, AICHAT_ROLE_SYSTEM, prompt));
  fclose (prompt);

  CHATTY_MAYBE_DIE (aichat_session_add_message_from_file (&session, AICHAT_ROLE_USER, stdin));
  chatty_extend_session_helper (&session);
}

void
chatty_import_session (const char *session)
{
  FILE *file = chatty_open_session_file_or_die (session, "wx", "use the --session option to extend an existing session");

  struct aichat_session chat_session;
  CHATTY_MAYBE_DIE (aichat_session_initialize_from_json_file (&chat_session, stdin));
  struct aichat_message *last_message = chat_session.messages + chat_session.message_count - 1;

  if (last_message->role != AICHAT_ROLE_ASSISTANT)
  {
    fprintf (stderr, "%s: last message in session must be from the assistant\n", program_invocation_short_name);
    exit (1);
  }

  CHATTY_MAYBE_DIE (aichat_session_write_to_json_file (&chat_session, file));
  fclose (file);
}

void
chatty_export_session (const char *session)
{
  FILE *file = chatty_open_session_file_or_die (session, "r", "");

  struct aichat_session chat_session;
  CHATTY_MAYBE_DIE (aichat_session_initialize_from_json_file (&chat_session, file));

  fclose (file);

  CHATTY_MAYBE_DIE (aichat_session_write_to_json_file (&chat_session, stdout));
}

