#pragma once


void chatty_initialize_directories (void);
void chatty_list_sessions (void);
void chatty_delete_all_sessions (void);
void chatty_delete_session (const char *session);
void chatty_extend_last_session (void);
void chatty_extend_session (const char *session);
void chatty_create_session (const char *session, const char *promptfile);
void chatty_once (const char *promptfile);
void chatty_retry_last_session (void);
void chatty_retry_session (const char *session);
void chatty_import_session (const char *session);
void chatty_export_session (const char *session);
