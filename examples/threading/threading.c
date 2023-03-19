#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{
    struct thread_data* data = (struct thread_data*)thread_param;

    usleep(data->wait_to_obtain_ms * 1000);

    int status = pthread_mutex_lock(data->mutex);
    if (status != 0) {
        ERROR_LOG("Failed to lock mutex: %s", strerror(status));
        data->thread_complete_success = false;
        goto exit;
    }

    usleep(data->wait_to_release_ms * 1000);
    
    status = pthread_mutex_unlock(data->mutex);
    if (status != 0) {
        ERROR_LOG("Failed to unlock mutex: %s", strerror(status));
        data->thread_complete_success = false;
        goto exit;
    }

    data->thread_complete_success = true;
    
exit:
    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex, int wait_to_obtain_ms, int wait_to_release_ms)
{
    struct thread_data* data = (struct thread_data*)malloc(sizeof(*data));
    if (!data) {
        ERROR_LOG("Out of memory");
        return false;
    }
    data->mutex = mutex;
    data->wait_to_obtain_ms = wait_to_obtain_ms;
    data->wait_to_release_ms = wait_to_release_ms;
    data->thread_complete_success = false;

    int status = pthread_create(thread, NULL, threadfunc, data);
    if (status != 0) {
        ERROR_LOG("Failed to create thread: %s", strerror(status));
        return false;
    }

    return true;
}

