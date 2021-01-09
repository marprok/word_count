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
#define OUT_FILE_NAME "out.txt"
static int out_file;

/* The signal handler used to ignore SIGINT and SIGTERM */
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

/* Returns 1 if the path is a valid directory, 0 otherwise */
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

/*
 * Reads all the chars in a file and returns 1 if
 * all of them where readable and 0 otherwise.
 * It checks if a file is a text file.
 */
int validate_file(uint8_t* buf, size_t size)
{
    assert(buf && size > 0);

    size_t i = 0;
    for (; i < size; ++i)
    {
        /*
         * If the char is nit a readable ascii and it is not whitespace then
         * it is a good indication that the file is not a text file.
         */
        if ((buf[i] < 0x20 || buf[i] > 0x7F) && !isspace(buf[i]))
            return 0;
    }

    return 1;
}

/*
 * This struct represents the arguments that
 * each thread needs to complete it's job.
 */
typedef struct
{
    size_t from; /* Offsset of the first char to process */
    size_t count; /* How many chars to process */
    uint8_t *data; /* The actual char buffer */
    size_t result; /* the number of words found */
} work_t;

/*
 * This is the function that each thread runs.
 * It counts the number of words in a section of text.
 */
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

/*
 * This function splits the file into sections
 * and starts NUM_OF_THREADS threads to count the
 * number of words in each section.
 */
void process_file(const char* file)
{
    int fd = open(file, O_RDONLY);
    if (fd < 0)
    {
        printf("open failed\n");
        return;
    }

    /* We use stat to get the size of the file in bytes */
    struct stat st;
    if (stat(file, &st) < 0 )
    {
        printf("stat2 failed\n");
        close(fd);
        return;
    }

    /* The bytes of the file */
    uint8_t *data = malloc(sizeof(uint8_t)*st.st_size);
    /* The line to be writen in the output file */
    char line[256 + 1] = {0};
    /*
     * If the read went ok and the file is not a binary file
     * then we start the processing
     */
    if (read(fd, data, sizeof(uint8_t)*st.st_size) &&
        validate_file(data, st.st_size))
    {
        /* The worker threads */
        pthread_t workers[NUM_OF_THREADS];
        /* The args for each thread */
        work_t work[NUM_OF_THREADS];
        /* The byte offset inside the file */
        size_t from = 0;
        size_t i = 0;
        /* Make a guess about the initial load of each thread */
        size_t bytes_to_parse = st.st_size/NUM_OF_THREADS;
        /* The extra chars to process in case a section ended
         * in a non whitespace char. We have to do This
         * in order to avoid any errors in the word counting */
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
            /* Initialize the work for each thread */
            work[i].from = from;
            work[i].count =  count;
            work[i].data = data;
            work[i].result =  0;
            /* Start the thread */
            if (pthread_create(workers+i, NULL, thread_work, work+i) != 0)
            {
                printf("Error creating the thread!\n");
            }
            from += count;
        }

        /* Wait for each thread to finish its work */
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

    /* Clean up */
    free(data);
    close(fd);
}

/*
 * This function returns the number if non-directory entries in the
 * given directory.
 */
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
    const char* target = ".";
    if (argc > 1)
        target = argv[1];

    /* Chech that the given name is a valid directory */
    if (!validate_dir(target))
    {
        printf("Invalid args!\n");
        exit(1);
    }

    /* Create the output file with rw-rw-r-- permissions in octal */
    out_file = open(OUT_FILE_NAME, O_CREAT | O_WRONLY | O_APPEND, 0664);
    if (out_file < 0 )
    {
        printf("Failed to create the output file!\n");
        exit(1);
    }

    pid_t parent_pid = getpid();
    size_t file_count = count_files(argv[1]);
    /* The pids of the working processes */
    pid_t *processes = malloc(sizeof(pid_t)*file_count);

    /* Iterate over the directory and start the forking */
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
                /* If it is the child process */
                sprintf(rel_path, "%s/%s", argv[1], dr->d_name);
                break; /* Stop forking and start working */
            }
            i++;
        }
    }

    /* We do not need the directory open. */
    closedir(dir_handle);
    if (parent_pid != getpid())
    {
        /* Deallocate any uneeded resources */
        free(processes);
        /* Start the processing of the file */
        process_file(rel_path);
    }else
    {
        /* Override the signal handlers */
        signal(SIGINT,signal_handler);
        signal(SIGTERM,signal_handler);
        /* Wait for all the children to complete their job */
        i = 0;
        int status;
        for (; i < file_count; ++i)
            waitpid(processes[i], &status, 0);
        free(processes);
    }

    close(out_file);
    exit(0);
}
