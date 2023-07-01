#pragma once

/***
 * About the token limit for the OpenAI API
 *
 * The OpenAI API has a limit of 4096 tokens per request but it is generally
 * difficult to know how many tokens a request will use. It seems that a token
 * is roughly 2.5 characters when code is sent to the API while it is 4 characters
 * when text is sent to the API. There are tokens with up to 128 characters and
 * with as little as 1 character.
 *
 * We will support a reasonable maximum message length of 4096 * 8 = 32768 characters
 * but the majority of conversations will hit the token limit long before that
 * so we must have robust handling of the token limit. We may consider limiting
 * the message length to say 4096 * 2 = 8192 characters so that the majority of
 * reasonable conversations will hit our limit before the token limit.
 *
 * On the other hand, there can be messages that contain only 1 token and therefore
 * we can have as many as 4096 messages in a single session.
 ***/

#define AICHAT_MAX_TOKENS 4096
#define AICHAT_MAX_CHARACTERS_PER_TOKEN 8

#define AICHAT_SESSION_BUFFER_SIZE (AICHAT_MAX_TOKENS * AICHAT_MAX_CHARACTERS_PER_TOKEN)
#define AICHAT_SESSION_MAX_MESSAGES 4096 

// define the error codes
#define AICHAT_ERROR_SESSION_FULL 1
#define AICHAT_ERROR_SESSION_BUFFER_FULL 2
#define AICHAT_ERROR_INVALID_CHARACTERS 3
#define AICHAT_ERROR_NOT_IMPLEMENTED 4
#define AICHAT_ERROR_CURL_INIT 5
#define AICHAT_ERROR_SESSION_NO_MESSAGES 6
#define AICHAT_ERROR_SESSION_LAST_MESSAGE_ASSISTANT 7
#define AICHAT_ERROR_JSON_PARSE_RESPONSE 8
#define AICHAT_ERROR_API_ERROR 9
#define AICHAT_ERROR_API_RESPONSE 10
#define AICHAT_ERROR_IO 11
#define AICHAT_ERROR_SESSION_EMPTY 12
#define AICHAT_ERROR_INVALID_JSON 13
#define AICHAT_ERROR_MEMORY 14

enum aichat_role { AICHAT_ROLE_SYSTEM, AICHAT_ROLE_USER, AICHAT_ROLE_ASSISTANT };
enum aichat_model { AICHAT_MODEL_GPT_3_5_TURBO };

struct
aichat_message
{
  enum aichat_role role;
  char *text;
};

struct
aichat_session
{
  struct aichat_message messages [AICHAT_SESSION_MAX_MESSAGES];
  unsigned int message_count;

  char buffer [AICHAT_SESSION_BUFFER_SIZE];
  unsigned int buffer_remaining;

  enum aichat_model model;
  double temperature;
};

struct
aichat_api_call_results 
{
  int error;

  int prompt_tokens;
  int completion_tokens;
};

void aichat_session_initialize (struct aichat_session *session);
int aichat_session_initialize_from_json_file (struct aichat_session *session, FILE *file);
int aichat_session_write_to_json_file (struct aichat_session *session, FILE *file);
int aichat_session_add_message (struct aichat_session *session, enum aichat_role role, const char *text);
int aichat_session_add_message_from_file (struct aichat_session *session, enum aichat_role role, FILE *file);
int aichat_session_extend (struct aichat_session *session, struct aichat_api_call_results *results);
int aichat_session_print_last_message (struct aichat_session *session, FILE *file);
int aichat_session_remove_last_message (struct aichat_session *session);
