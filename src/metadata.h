#ifndef METADATA_H
#define METADATA_H

int user_exists(const char *username, const char *password);
int signup_user(const char *username, const char *password);
void ensure_dir(const char *path);

#endif
