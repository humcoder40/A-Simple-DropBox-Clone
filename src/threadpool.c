#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include "common.h"
#include "queue.h"

// declare global queues
Queue clientQueue;
Queue taskQueue;

#define NUM_WORKERS 3

// forward declaration for metadata functions (from metadata.c)
User* create_user_if_missing(const char *username);
int add_usage(const char *username, size_t bytes);
void reduce_usage(const char *username, size_t bytes);

// helper: build final path
static void final_path(char *out, size_t out_sz, const char *username, const char *filename) {
    snprintf(out, out_sz, "%s/%s/%s", STORAGE_ROOT, username, filename);
}

// safe rename/move tmp -> final
static int move_tmp_to_final(const char *tmp, const char *final) {
    if (rename(tmp, final) == 0) return 0;
    // fallback: copy then unlink
    FILE *in = fopen(tmp, "rb");
    if (!in) return -1;
    FILE *out = fopen(final, "wb");
    if (!out) { fclose(in); return -1; }
    char buf[8192];
    size_t r;
    while ((r = fread(buf,1,sizeof(buf), in)) > 0) fwrite(buf,1,r,out);
    fclose(in); fclose(out);
    unlink(tmp);
    return 0;
}

static void handle_upload(Task *t) {
    // t->tmp_path must contain path to the file written by client thread
    struct stat st;
    if (stat(t->tmp_path, &st) != 0) {
        t->status = -1;
        snprintf(t->errmsg, sizeof(t->errmsg), "temp file missing");
        return;
    }
    size_t fsize = st.st_size;

    // check quota and update metadata
    if (add_usage(t->username, fsize) != 0) {
        t->status = -2;
        snprintf(t->errmsg, sizeof(t->errmsg), "quota exceeded");
        unlink(t->tmp_path);
        return;
    }

    // ensure user dir
    char final[PATH_MAX_LEN];
    final_path(final, sizeof(final), t->username, t->filename);

    // move tmp -> final
    if (move_tmp_to_final(t->tmp_path, final) != 0) {
        t->status = -3;
        snprintf(t->errmsg, sizeof(t->errmsg), "move failed: %s", strerror(errno));
        // revert usage
        reduce_usage(t->username, fsize);
        unlink(t->tmp_path);
        return;
    }
    t->status = 0;
}

static void handle_download(Task *t) {
    char final[PATH_MAX_LEN];
    final_path(final, sizeof(final), t->username, t->filename);

    struct stat st;
    if (stat(final, &st) != 0) {
        t->status = -1;
        snprintf(t->errmsg, sizeof(t->errmsg), "file not found");
        return;
    }

    // create a temp file to store the copy worker will produce for client thread to read
    char outtmp[PATH_MAX_LEN];
    snprintf(outtmp, sizeof(outtmp), "%s/%s.download.%d.tmp", STORAGE_ROOT, t->username, (int)getpid());
    FILE *in = fopen(final, "rb");
    if (!in) { t->status = -2; snprintf(t->errmsg, sizeof(t->errmsg), "open fail"); return; }
    FILE *out = fopen(outtmp, "wb");
    if (!out) { fclose(in); t->status = -3; snprintf(t->errmsg, sizeof(t->errmsg), "create tmp fail"); return; }
    char buf[8192];
    size_t r;
    while ((r = fread(buf,1,sizeof(buf), in)) > 0) fwrite(buf,1,r,out);
    fclose(in); fclose(out);
    // worker fills tmp_path with outtmp, client thread will read and send file and then unlink it
    strncpy(t->tmp_path, outtmp, sizeof(t->tmp_path)-1);
    t->status = 0;
}

static void handle_delete(Task *t) {
    char final[PATH_MAX_LEN];
    final_path(final, sizeof(final), t->username, t->filename);
    struct stat st;
    if (stat(final, &st) != 0) {
        t->status = -1;
        snprintf(t->errmsg, sizeof(t->errmsg), "file not found");
        return;
    }
    size_t fsize = st.st_size;
    if (unlink(final) != 0) {
        t->status = -2;
        snprintf(t->errmsg, sizeof(t->errmsg), "delete failed");
        return;
    }
    reduce_usage(t->username, fsize);
    t->status = 0;
}

static void handle_list(Task *t) {
    // produce a small temporary listing file with one line per file: name size
    char dir[PATH_MAX_LEN];
    snprintf(dir, sizeof(dir), "%s/%s", STORAGE_ROOT, t->username);
    FILE *out;
    char outtmp[PATH_MAX_LEN];
    snprintf(outtmp, sizeof(outtmp), "%s/%s.list.%d.tmp", STORAGE_ROOT, t->username, (int)getpid());
    out = fopen(outtmp, "w");
    if (!out) { t->status = -1; snprintf(t->errmsg, sizeof(t->errmsg), "list tmp create fail"); return; }
    // iterate directory
    #include <dirent.h>
    struct dirent *de;
    DIR *dr = opendir(dir);
    if (!dr) {
        fclose(out);
        t->status = -2;
        snprintf(t->errmsg, sizeof(t->errmsg), "open dir fail");
        return;
    }
    while ((de = readdir(dr)) != NULL) {
        if (de->d_type == DT_REG) {
            char fpath[PATH_MAX_LEN];
            snprintf(fpath, sizeof(fpath), "%s/%s", dir, de->d_name);
            struct stat st;
            if (stat(fpath, &st) == 0) {
                fprintf(out, "%s %zu\n", de->d_name, (size_t)st.st_size);
            }
        }
    }
    closedir(dr);
    fclose(out);
    strncpy(t->tmp_path, outtmp, sizeof(t->tmp_path)-1);
    t->status = 0;
}

static void *worker_thread(void *arg) {
    (void)arg;
    while (1) {
        Task *t = (Task*) queue_pop(&taskQueue);
        // perform requested operation
        if (!t) continue;
        // perform operation
        switch (t->type) {
            case T_UPLOAD: handle_upload(t); break;
            case T_DOWNLOAD: handle_download(t); break;
            case T_DELETE: handle_delete(t); break;
            case T_LIST: handle_list(t); break;
            default:
                t->status = -99;
                snprintf(t->errmsg, sizeof(t->errmsg), "unknown task");
        }
        // signal client thread waiting on this task
        pthread_mutex_lock(&t->resp_mtx);
        t->done = 1;
        pthread_cond_signal(&t->resp_cond);
        pthread_mutex_unlock(&t->resp_mtx);
        // worker does not free t here: client thread will free it after sending data back
    }
    return NULL;
}

void start_worker_pool() {
    pthread_t workers[NUM_WORKERS];
    for (int i = 0; i < NUM_WORKERS; ++i) {
        pthread_create(&workers[i], NULL, worker_thread, NULL);
    }
}
