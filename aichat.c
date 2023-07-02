#define _GNU_SOURCE

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <curl/curl.h>

#include <json-c/json.h>

#include "aichat.h"

struct
aichat_api_call_state
{
  json_object  *object;
  json_tokener *tokener;
};

static char *
aichat_session_current_buffer_position (struct aichat_session *session)
{
  return session->buffer + (AICHAT_SESSION_BUFFER_SIZE - session->buffer_remaining);
}

static bool
aichat_session_can_accomodate (struct aichat_session *session, const char *text)
{
  // we need enough space to add the text and a null terminator
  return session->buffer_remaining >= strlen (text) + 1;
}

void
aichat_session_initialize (struct aichat_session *session)
{
  session->message_count = 0;
  session->buffer_remaining = AICHAT_SESSION_BUFFER_SIZE;

  session->model = AICHAT_MODEL_GPT_3_5_TURBO;
  session->temperature = 0.7;
}

int
aichat_session_initialize_from_json_file (struct aichat_session *session, FILE *file)
{
  aichat_session_initialize (session);

  char *buffer = NULL;
  long unsigned int current_size = 0;

  FILE *buffer_file = open_memstream (&buffer, &current_size);

  char c = fgetc (file);

  while (c != EOF)
  {
    fputc (c, buffer_file);
    c = fgetc (file);
  }

  fflush (buffer_file);

  json_object *object = json_tokener_parse (buffer);
  fclose (buffer_file); free (buffer);

  if (object == NULL)
  {
    return -AICHAT_ERROR_JSON_PARSE;
  }

  json_object *messages = json_object_object_get (object, "messages");

  if (messages == NULL) goto aichat_session_initialize_from_json_file_error;

  int message_count = json_object_array_length (messages);

  for (int i = 0; i < message_count; i++)
  {
    json_object *message = json_object_array_get_idx (messages, i);

    if (message == NULL) goto aichat_session_initialize_from_json_file_error;

    json_object *role    = json_object_object_get (message, "role");
    json_object *content = json_object_object_get (message, "content");

    if (role == NULL || content == NULL) goto aichat_session_initialize_from_json_file_error;

    const char *role_string = json_object_get_string (role);
    const char *content_string = json_object_get_string (content);

    if (role_string == NULL || content_string == NULL) goto aichat_session_initialize_from_json_file_error;

    enum aichat_role role_enum;

    if (strcmp (role_string, "user") == 0)
    {
      role_enum = AICHAT_ROLE_USER;
    }
    else if (strcmp (role_string, "system") == 0)
    {
      role_enum = AICHAT_ROLE_SYSTEM;
    }
    else if (strcmp (role_string, "assistant") == 0)
    {
      role_enum = AICHAT_ROLE_ASSISTANT;
    }
    else goto aichat_session_initialize_from_json_file_error;
   
    int result = aichat_session_add_message (session, role_enum, content_string);

    if (result < 0)
    {
      return result;
    }
  }

  json_object_put (object);
  return 0;

aichat_session_initialize_from_json_file_error:
  json_object_put (object);
  return -AICHAT_ERROR_JSON_PARSE;
}


int
aichat_session_add_message (struct aichat_session *session, enum aichat_role role, const char *text)
{
  if (session->message_count >= AICHAT_SESSION_MAX_MESSAGES)
    return -AICHAT_ERROR_SESSION_FULL;
  
  // including the null terminator we need to have space for the message
  if (aichat_session_can_accomodate (session, text) == false)
    return -AICHAT_ERROR_SESSION_BUFFER_FULL;

  struct aichat_message *message = &session->messages[session->message_count];
  message->role = role;
  message->text = aichat_session_current_buffer_position (session);

  // make sure to include the null terminator
  strcpy (message->text, text); // this will copy the null terminator as well
  session->buffer_remaining -= strlen (text) + 1; // +1 for the null terminator
  session->message_count++; // increment the message count

  return 0;
}

int
aichat_session_add_message_from_file (struct aichat_session *session, enum aichat_role role, FILE *file)
{
  if (session->message_count >= AICHAT_SESSION_MAX_MESSAGES)
    return -AICHAT_ERROR_SESSION_FULL;

  struct aichat_message *message = &session->messages[session->message_count];
  message->role = role;
  message->text = aichat_session_current_buffer_position (session);

  unsigned int read = fread (message->text, 1, session->buffer_remaining, file);

  if (ferror (file) != 0)
    return -AICHAT_ERROR_IO;

  if (read == session->buffer_remaining)
    return -AICHAT_ERROR_SESSION_BUFFER_FULL;

  message->text[read] = '\0';

  session->buffer_remaining -= read + 1;
  session->message_count++;

  return 0;
}

int
aichat_session_print_last_message (struct aichat_session *session, FILE *file)
{
  if (session->message_count == 0)
    return -AICHAT_ERROR_SESSION_NO_MESSAGES;

  struct aichat_message *message = &session->messages[session->message_count - 1];

  fprintf (file, "%s", message->text);

  return 0;
}

int
aichat_session_remove_last_message (struct aichat_session *session)
{
  if (session->message_count == 0)
    return -AICHAT_ERROR_SESSION_NO_MESSAGES;

  struct aichat_message *message = &session->messages[session->message_count - 1];

  session->buffer_remaining += strlen (message->text) + 1;
  session->message_count--;

  return 0;
}

static json_object *
aichat_message_to_json_object (struct aichat_message *message)
{
  json_object *jobj = json_object_new_object();

  switch (message->role)
  {
    case AICHAT_ROLE_SYSTEM:
      json_object_object_add (jobj, "role", json_object_new_string ("system"));
      break;
    case AICHAT_ROLE_USER:
      json_object_object_add (jobj, "role", json_object_new_string ("user"));
      break;
    case AICHAT_ROLE_ASSISTANT:
      json_object_object_add (jobj, "role", json_object_new_string ("assistant"));
      break;
  }

  json_object_object_add (jobj, "content", json_object_new_string (message->text));
  return jobj;
}

char *
aichat_message_to_json (struct aichat_message *message, unsigned long int *length)
{

  json_object *jobj = aichat_message_to_json_object (message);
  char *json = strdup (json_object_to_json_string_length (jobj, JSON_C_TO_STRING_PLAIN, length));

  json_object_put (jobj);
  return json;
}

static json_object *
aichat_session_to_json_object (struct aichat_session *session)
{
  json_object *jobj = json_object_new_object();

  const char *model_string = session->model == AICHAT_MODEL_GPT_3_5_TURBO ? "gpt-3.5-turbo" : "gpt-3.5-turbo-16k";
  json_object_object_add (jobj, "model", json_object_new_string (model_string));

  json_object_object_add (jobj, "temperature", json_object_new_double (session->temperature));

  json_object *jmsgs = json_object_new_array ();

  for (unsigned int i = 0; i < session->message_count; i++)
  {
    json_object *jmsg = aichat_message_to_json_object (&session->messages[i]);
    json_object_array_add (jmsgs, jmsg);
  }

  json_object_object_add (jobj, "messages", jmsgs);

  return jobj;
}

int
aichat_session_write_to_json_file (struct aichat_session *session, FILE *file)
{
  json_object *jobj = aichat_session_to_json_object (session);
  fprintf (file, "%s", json_object_to_json_string_ext (jobj, JSON_C_TO_STRING_PRETTY));
  json_object_put (jobj);
  return 0;
}

char *
aichat_session_to_json (struct aichat_session *session, unsigned long int *length)
{
  json_object *jobj = aichat_session_to_json_object (session);
  char *json = strdup (json_object_to_json_string_length (jobj, JSON_C_TO_STRING_PLAIN, length));

  json_object_put (jobj);
  return json;
}


unsigned long int
aichat_api_call_write_callback (char *buffer, unsigned long int size, unsigned long int n, void *userdata)
{
  /* calculate the real size of the incoming buffer */
  unsigned long int realsize = size * n;
  
  struct aichat_api_call_state *state = (struct aichat_api_call_state *) userdata;

  /* parse the received data */
  state->object = json_tokener_parse_ex (state->tokener, buffer, realsize);

  /* check for errors */
  enum json_tokener_error jerr = json_tokener_get_error (state->tokener);
  if (jerr != json_tokener_success && jerr != json_tokener_continue) {
      return 0; /* tell libcurl to stop the download */
  }

  return realsize;
}

struct aichat_api_call_state *
aichat_api_call_state_initialize (void)
{
  struct aichat_api_call_state *state = malloc (sizeof (struct aichat_api_call_state));
  state->tokener = json_tokener_new ();
  state->object = NULL;
  return state;
}

void
aichat_api_call_state_free (struct aichat_api_call_state *state)
{
  json_tokener_free (state->tokener);
  json_object_put (state->object);
  free (state);
}

char *
aichat_api_call_state_resolve (struct aichat_api_call_state *state, struct aichat_api_call_results *results)
{
  enum json_tokener_error jerr = json_tokener_get_error (state->tokener);

  if (jerr != json_tokener_success)
  {
    results->error = AICHAT_ERROR_JSON_PARSE;
    return NULL;
  }

  // check if the response is an error
  json_object *jerror = NULL;
  if (json_object_object_get_ex (state->object, "error", &jerror))
  {
    results->error = AICHAT_ERROR_API_ERROR;
    return NULL;
  }

  // otherwise extract .usage.prompt_tokens, .usage.completion_tokens and store in results
  json_object *jusage = NULL;
  if (json_object_object_get_ex (state->object, "usage", &jusage))
  {
    json_object *jprompt_tokens = NULL;
    json_object *jcompletion_tokens = NULL;

    if (json_object_object_get_ex (jusage, "prompt_tokens", &jprompt_tokens))
    {
      results->prompt_tokens = json_object_get_int (jprompt_tokens);
    }

    if (json_object_object_get_ex (jusage, "completion_tokens", &jcompletion_tokens))
    {
      results->completion_tokens = json_object_get_int (jcompletion_tokens);
    }
  }

  // extract the response from .choices[0].message.content and return a strdup of it
  json_object *jchoices = NULL;
  if (json_object_object_get_ex (state->object, "choices", &jchoices))
  {
    json_object *jchoice = json_object_array_get_idx (jchoices, 0);
    json_object *jmessage = NULL;
    if (json_object_object_get_ex (jchoice, "message", &jmessage))
    {
      json_object *jcontent = NULL;
      if (json_object_object_get_ex (jmessage, "content", &jcontent))
      {
        const char *content = json_object_get_string (jcontent);
        results->error = 0;
        return strdup (content);
      }
    }
  }

  // if we get here, something went wrong so return NULL and set error
  results->error = AICHAT_ERROR_API_RESPONSE;
  return NULL;
}

char *
aichat_api_call_do (const char *data, unsigned long int data_strlen, const char *key, struct aichat_api_call_results *results)
{
  // now we need to send the json to the api using curl printing the response to stdout
  CURL *curl = curl_easy_init ();

  if (curl == NULL)
  {
    results->error = AICHAT_ERROR_CURL_INITIALIZATION;
    return NULL;
  }

  struct aichat_api_call_state *state = aichat_api_call_state_initialize ();

  // set the appropriate headers
  struct curl_slist *headers = NULL;
  headers = curl_slist_append (headers, "Content-Type: application/json");
  headers = curl_slist_append (headers, "Accept: application/json");

  if (key)
  {
    char *authorization;
    int length = asprintf (&authorization, "Authorization: Bearer %s", key);

    if (length < 0)
    {
      results->error = AICHAT_ERROR_MEMORY;
      return NULL;
    }

    // add the header
    headers = curl_slist_append (headers, authorization);

    free (authorization);
  }

  curl_easy_setopt (curl, CURLOPT_URL, "https://api.openai.com/v1/chat/completions");
  curl_easy_setopt (curl, CURLOPT_POSTFIELDS, data);
  curl_easy_setopt (curl, CURLOPT_POSTFIELDSIZE, data_strlen);
  curl_easy_setopt (curl, CURLOPT_WRITEDATA, state);
  curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, aichat_api_call_write_callback);
  curl_easy_setopt (curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_perform (curl);
  curl_easy_cleanup (curl);

  curl_slist_free_all (headers);

  char *new_message = aichat_api_call_state_resolve (state, results);
  aichat_api_call_state_free (state);

  return new_message;
}

int
aichat_session_extend (struct aichat_session *session, struct aichat_api_call_results *results)
{
  if (session->message_count == 0)
    return -AICHAT_ERROR_SESSION_NO_MESSAGES;

  if (session->messages[session->message_count - 1].role == AICHAT_ROLE_ASSISTANT)
    return -AICHAT_ERROR_SESSION_LAST_MESSAGE_ASSISTANT;

  unsigned long int data_strlen;
  char *data = aichat_session_to_json (session, &data_strlen);
  const char *key = getenv ("OPENAI_API_KEY");

  char *next_message = aichat_api_call_do (data, data_strlen, key, results);
  free (data);

  if (next_message == NULL)
    return -results->error;

  int retval = aichat_session_add_message (session, AICHAT_ROLE_ASSISTANT, next_message);
  free (next_message);

  return retval;
}

const char *
aichat_strerror (int error_code)
{
  if (error_code >= 0) return "No error";
  error_code = -error_code;

  switch (error_code)
  {
    case AICHAT_ERROR_SESSION_FULL:
      return "Reached internal limit of messages in session";
    case AICHAT_ERROR_SESSION_BUFFER_FULL:
      return "Reached internal limit of combined length of messages in session";
    case AICHAT_ERROR_INVALID_CHARACTERS:
      return "Message contains invalid characters";
    case AICHAT_ERROR_NOT_IMPLEMENTED:
      return "Not implemented";
    case AICHAT_ERROR_CURL_INITIALIZATION:
      return "Failed to initialize libcurl";
    case AICHAT_ERROR_SESSION_NO_MESSAGES:
      return "Session has no messages";
    case AICHAT_ERROR_SESSION_LAST_MESSAGE_ASSISTANT:
      return "Last message in session was not from the assistant";
    case AICHAT_ERROR_JSON_PARSE:
      return "Failed to parse JSON response from API";
    case AICHAT_ERROR_API_ERROR:
      return "API returned an error";
    case AICHAT_ERROR_API_RESPONSE:
      return "API returned an unexpected response";
    case AICHAT_ERROR_IO:
      return "I/O error";
    case AICHAT_ERROR_MEMORY:
      return "Memory allocation error";
    default:
      return "Unknown error";
  }

  return "Unknown error";
}

