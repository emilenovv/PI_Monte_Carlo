#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>
#include <stdbool.h>
#include <pthread.h>

extern const char *__progname;

static const long long npoints = 10000000000;
static int num_threads = 1;

bool verbose = true;

typedef unsigned long long ull;

static ull total_points_in_circle = 0;


typedef struct data_t {
    pthread_t thread_id;
    uint32_t thread_num;
    ull cycles;
    ull square_side_length;
} data_t;

pthread_mutex_t lock;

void print_help()
{
    printf("%s: Usage: ./%s [-q] -t num_threads -s side_length\n", __progname, __progname);
    printf("-q\tsuppress verbosity.\n");
    printf("-t\tnum_threads   num_threads that will be executed.\n");
    printf("-s\tside_length   side_length of the square in pixels.\n");
}

bool is_positive_numeric(char *str)
{
    if (!*str)
        return false;

    while (*str)
    {
        if (!isdigit(*str))
            return false;
        else
            ++str;
    }

    return true;

}

void* count_dots(void* void_data)
{
    struct timespec start, finish;
    long double elapsed;
    clock_gettime(CLOCK_MONOTONIC, &start);
    data_t* thread_data = (data_t*) void_data;
    ull points_in_circle = 0;
    int error;
    unsigned int seed = time(NULL);
    const int square_side = thread_data->square_side_length;
    const ull center_x = square_side / 2;
    const ull center_y = square_side / 2;
    const ull radius = square_side / 2;
    const ull squared_radius = radius * radius;
    ull j = 0;
    ull x_coord, y_coord;

    if (verbose)
        printf("Thread_%d started!\n", thread_data->thread_num);

    // printf("cycles: %lli\n", thread_data->cycles);
    do
    {
        x_coord = rand_r(&seed) % square_side;
        y_coord = rand_r(&seed) % square_side;
        if ((x_coord - center_x) * (x_coord - center_x) + (y_coord - center_y) * (y_coord - center_y) < squared_radius)
            points_in_circle++;
        j++;
    } while (j < thread_data->cycles);

    /* Lock the shared variable */
    error = pthread_mutex_lock(&lock);
    if (error)
    {
        printf("[%s] [%d] pthread_mutex_lock failed with error code %d\n", __FUNCTION__, __LINE__, error);
    }
    // printf("points_in_circle: %lli\n", points_in_circle);
    total_points_in_circle += points_in_circle;
    error = pthread_mutex_unlock(&lock);
    if (error)
    {
        printf("[%s] [%d] pthread_mutex_unlock failed with error code %d\n", __FUNCTION__, __LINE__, error);
    }

    if (verbose)
        printf("Thread_%d stopped!\n", thread_data->thread_num);
    clock_gettime(CLOCK_MONOTONIC, &finish);
    elapsed = (finish.tv_sec - start.tv_sec);
    elapsed += (finish.tv_nsec - start.tv_nsec) / 1000000000.0;
    if (verbose)
        printf("Thread_%d execution time was (millis): %Lf\n", thread_data->thread_num, elapsed * 1000);
    return NULL;
}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        print_help();
        return 1;
    }

    struct timespec start, finish;
    long double elapsed;
    ull points_in_circle = 0, square_side = 0, work_per_thread;
    clock_gettime(CLOCK_MONOTONIC, &start);
    void* ret = NULL;
    int i, c, error, remaining_points = npoints % num_threads;

    opterr = 0;

    while ((c = getopt (argc, argv, "s:t:q")) != -1)
    {
        switch (c)
            {
                case 's':
                    if (is_positive_numeric(optarg))
                        square_side = atoi(optarg);
                    else
                    {
                        printf("Illegal argument \"%s\"!\n", optarg);
                        return 1;
                    }
                    break;
                case 't':
                    if (is_positive_numeric(optarg))
                        num_threads = atoi(optarg);
                    else
                    {
                        printf("Illegal argument \"%s\"!\n", optarg);
                        return 1;
                    }
                    break;
                case 'q':
                    verbose = false;
                    break;
                default:
                    print_help();
                    return 1;
            }
    }
    if (verbose)
        printf("Threads used in this run: %d\n", num_threads);

    error = pthread_mutex_init(&lock, NULL);
    if (error)
    {
        printf("[%s] [%d] pthread_mutex_init failed with error code %d\n", __FUNCTION__, __LINE__, error);
        goto cleanup;
    }
    data_t *thread_data;
    work_per_thread = npoints / num_threads;

    /* Allocate memory for the thread information */
    thread_data = calloc(num_threads, sizeof(data_t));
    if (thread_data == NULL)
    {
        printf("calloc failed!\n");
        goto cleanup;
    }

    /* Feed the thread information and create the threads */
    for (i = 0; i < num_threads; i++)
    {
        thread_data[i].cycles = work_per_thread;
        thread_data[i].square_side_length = square_side;
        thread_data[i].thread_num = i + 1;
        error = pthread_create(&(thread_data[i].thread_id), NULL, count_dots, &thread_data[i]);
        if (error)
        {
            printf("[%s] [%d] pthread_create failed with error code %d\n", __FUNCTION__, __LINE__, error);
            goto cleanup;
        }
    }

    /* If npoints is not divisible by num_threads calculate the remaining points in the main thread */
    const ull center_x = square_side / 2;
    const ull center_y = square_side / 2;
    const ull radius = square_side / 2;
    ull j = 0;
    ull x_coord, y_coord;
    do
    {
        x_coord = rand() % square_side;
        y_coord = rand() % square_side;
        if ((x_coord - center_x) * (x_coord - center_x) + (y_coord - center_y) * (y_coord - center_y) < (radius * radius))
            points_in_circle++;
        j++;
    } while (j < remaining_points);

    /* Lock the shared variable */
    error = pthread_mutex_lock(&lock);
    if (error)
    {
        printf("[%s] [%d] pthread_mutex_lock failed with error code %d\n", __FUNCTION__, __LINE__, error);
        goto cleanup;
    }
    total_points_in_circle += points_in_circle;
    error = pthread_mutex_unlock(&lock);
    if (error)
    {
        printf("[%s] [%d] pthread_mutex_unlock failed with error code %d\n", __FUNCTION__, __LINE__, error);
        goto cleanup;
    }

    /* Wait for the threads to complete their job */
    for (i = 0; i < num_threads; i++)
        pthread_join(thread_data[i].thread_id, &ret);


    /* Calculate the PI itself */
    printf("\n********************\n");
    printf("*** PI: %Lf ***\n", 4.0 * (long double)total_points_in_circle / (long double)npoints);
    printf("********************\n\n");

    /* Cleanup */
cleanup:
    /* Don't bother if mutex is not initialized */
    pthread_mutex_destroy(&lock);
    if (thread_data)
        free(thread_data);
    clock_gettime(CLOCK_MONOTONIC, &finish);
    elapsed = (finish.tv_sec - start.tv_sec);
    elapsed += (finish.tv_nsec - start.tv_nsec) / 1000000000.0;
    printf("Total execution time for current run was (millis): %Lf\n", elapsed * 1000);
    return 0;
}
