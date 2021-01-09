#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdint.h>
#include <dirent.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <pthread.h>

#define NUM_OF_THREADS 4
static int out_file;

void signal_handler(int signum)
{
    if (signum == SIGTERM)
    {
        printf("Ignoring SIGTERM!\n");
    }else if (signum == SIGINT)
    {
        printf("Ignoring SIGINT!\n");
    }
    signal(signum, signal_handler);
}

int validate_dir(const char* path)
{
    assert(path);

    struct stat st;
    if (stat(path, &st) < 0 )
    {
        printf("error stat\n");
        return 0;
    }

    return S_ISDIR(st.st_mode);
}

int validate_file(uint8_t* buf, size_t size)
{
    assert(buf && size > 0);

    size_t i = 0;
    for (; i < size; ++i)
    {
        if ((buf[i] < 0x20 || buf[i] > 0x7F) && !isspace(buf[i]))
        {
            return 0;
        }
    }

    return 1;
}

size_t count_words(uint8_t* buf, size_t size)
{
    assert(buf && size > 0);

    size_t i = 0, count = 0;
    while (i < size)
    {
        while (i < size && isspace(buf[i])) i++;

        if (i < size) count++;
        while (i < size && !isspace(buf[i])) i++;
    }

    return count;
}

typedef struct
{
    size_t from;
    size_t count;
    uint8_t *data;
    size_t result;
} work_t;

void* thread_work(void* args)
{
    work_t *work = (work_t*)args;
    size_t i = work->from;
    size_t total = work->from + work->count;
    while (i < total)
    {
        while (i < total && isspace(work->data[i])) i++;

        if (i < total) work->result++;
        while (i < total && !isspace(work->data[i])) i++;
    }

    return NULL;
}

void process_file(const char* file)
{
    int fd = open(file, O_RDONLY);
    if (fd < 0)
    {
        printf("open failed\n");
        return;
    }

    struct stat st;
    if (stat(file, &st) < 0 )
    {
        printf("stat2 failed\n");
        close(fd);
        return;
    }

    uint8_t *data = malloc(sizeof(uint8_t)*st.st_size);
    char line[256 + 1] = {0};
    if (read(fd, data, sizeof(uint8_t)*st.st_size) &&
        validate_file(data, st.st_size))
    {
        pthread_t workers[NUM_OF_THREADS];
        work_t work[NUM_OF_THREADS];
        size_t from = 0;
        size_t i = 0;
        size_t tb = 0;
        size_t bytes_to_parse = st.st_size/NUM_OF_THREADS;
        size_t padding = 0;
        for (; i < NUM_OF_THREADS; ++i)
        {
            size_t count = bytes_to_parse + padding;
            padding = 0;
            if (i == NUM_OF_THREADS-1)
                count += st.st_size%NUM_OF_THREADS;
            else
            {
                while (!isspace(data[from+count]))
                {
                    count--;
                    padding++;
                }
            }
            work[i].from = from;
            work[i].count =  count;
            work[i].data = data;
            work[i].result =  0;
            if (pthread_create(workers+i, NULL, thread_work, work+i) != 0)
            {
                printf("Error creating the thread!\n");
            }
            from += count;
            tb += count;
        }
        printf("size %zu\n", tb);

        size_t total_words = 0;
        for (i = 0; i < NUM_OF_THREADS; ++i)
        {
            pthread_join(workers[i], NULL);
            total_words += work[i].result;
        }

        sprintf(line, "%d, %s %zu\n", getpid(), file, total_words);
        /*
         * From the man pages of write, it is guaranteed to write at the end
         * of the file, so no syncronization is needed here.
         */
        write(out_file, line, strlen(line));
    }

    free(data);
    close(fd);
}

size_t count_files(const char *path)
{
    assert(path);
    size_t count = 0;
    DIR *dir_handle = opendir(path);
    if (!dir_handle)
    {
        printf("opendir1 failed!\n");
        return count;
    }
    struct dirent *dr;
    while ((dr = readdir(dir_handle)) != NULL)
        if (dr->d_type != DT_DIR)
            count++;
    closedir(dir_handle);
    return count;

}

int main(int argc, char** argv)
{
    if (argc != 2 || !validate_dir(argv[1]))
    {
        printf("Invalid args!\n");
        exit(1);
    }

    /* Create the output file with rw-rw-r-- permissions in octal */
    out_file = open("out.txt", O_CREAT | O_WRONLY | O_APPEND, 0664);
    if (out_file < 0 )
    {
        printf("Failed to create the output file!\n");
        exit(1);
    }

    pid_t parent_pid = getpid();
    size_t file_count = count_files(argv[1]);
    pid_t *processes = malloc(sizeof(pid_t)*file_count);

    DIR *dir_handle = opendir(argv[1]);
    struct dirent *dr = NULL;
    char rel_path[1024] = {0};
    size_t i = 0;
    while ((dr = readdir(dir_handle)) != NULL)
    {
        if (dr->d_type != DT_DIR)
        {
            processes[i] = fork();
            if (!processes[i])
            {
                sprintf(rel_path, "%s/%s", argv[1], dr->d_name);
                break; /* Stop forking and start working */
            }
            i++;
        }
    }
    closedir(dir_handle);
    if (parent_pid != getpid())
    {
        free(processes);

        process_file(rel_path);
    }else
    {
        signal(SIGINT,signal_handler);
        signal(SIGTERM,signal_handler);

        i = 0;
        int status;
        for (; i < file_count; ++i)
            waitpid(processes[i], &status, 0);
        free(processes);
    }

    close(out_file);
    exit(0);
}
