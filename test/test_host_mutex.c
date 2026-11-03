/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "../runtime/syslib/kernel_mutex.c"
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); return 1; } } while (0)
static guest_mutex *mutex;
static guest_cond *condition;
static int ready;
static void *notify(void *unused)
{
    (void)unused;
    if (mutex_lock("host test", &mutex)) return (void *)1;
    ready = 1;
    if (cond_signal(&condition) || mutex_unlock(&mutex)) return (void *)1;
    return NULL;
}
static void *unlock_without_ownership(void *result)
{
    *(int *)result = mutex_unlock(&mutex);
    return NULL;
}
int main(void)
{
    for (int type = TYPE_ERRORCHECK; type <= TYPE_NORMAL; type++) {
        CHECK(mutex_create(&mutex, type, 0) == 0);
        CHECK(mutex_unlock(&mutex) == BSD_EPERM);
        CHECK(mutex_lock("host test", &mutex) == 0);
        pthread_t other; int result = 0;
        CHECK(pthread_create(&other, NULL, unlock_without_ownership, &result) == 0);
        CHECK(pthread_join(other, NULL) == 0 && result == BSD_EPERM);
        CHECK(mutex_trylock(&mutex) == (type == TYPE_RECURSIVE ? 0 : BSD_EBUSY));
        if (type == TYPE_RECURSIVE) CHECK(mutex_unlock(&mutex) == 0);
        if (type == TYPE_RECURSIVE) { CHECK(mutex_lock("host test", &mutex) == 0); CHECK(mutex_unlock(&mutex) == 0); }
        CHECK(mutex_unlock(&mutex) == 0); CHECK(mutex_destroy(&mutex) == 0);
    }
    CHECK(mutex_create(&mutex, TYPE_RECURSIVE, 0) == 0);
    CHECK(cond_create(&condition) == 0);
    CHECK(mutex_lock("host test", &mutex) == 0); CHECK(mutex_lock("host test", &mutex) == 0);
    pthread_t worker; CHECK(pthread_create(&worker, NULL, notify, NULL) == 0);
    while (!ready) CHECK(cond_wait(&condition, &mutex, NULL) == 0);
    CHECK(mutex->depth == 2);
    CHECK(mutex_unlock(&mutex) == 0); CHECK(mutex_unlock(&mutex) == 0);
    void *answer; CHECK(pthread_join(worker, &answer) == 0 && !answer);
#if defined(__APPLE__) && defined(__MACH__)
    pid_t child = fork(); CHECK(child >= 0);
    if (!child) { mutex_create(&mutex, TYPE_ADAPTIVE, 0); _exit(0); }
    int status; CHECK(waitpid(child, &status, 0) == child); CHECK(WIFSIGNALED(status));
    for (int protocol = 1; protocol <= 2; protocol++) {
        child = fork(); CHECK(child >= 0);
        if (!child) { mutex_create(&mutex, TYPE_ERRORCHECK, protocol); _exit(0); }
        CHECK(waitpid(child, &status, 0) == child); CHECK(WIFSIGNALED(status));
    }
    child = fork(); CHECK(child >= 0);
    if (!child) { struct timespec deadline = {0}; cond_wait(&condition, &mutex, &deadline); _exit(0); }
    CHECK(waitpid(child, &status, 0) == child); CHECK(WIFSIGNALED(status));
#endif
    CHECK(cond_destroy(&condition) == 0); CHECK(mutex_destroy(&mutex) == 0);
    puts("host mutex: supported types, foreign-owner rejection and recursive condition ownership passed");
    return 0;
}
