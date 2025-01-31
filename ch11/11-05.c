#include "apue.h"
#include <pthread.h>
#include <limits.h>
#include <sys/time.h>

#define NTHR 8               /* number of threads */
#define NUMNUM 8000000L      /* number of numbers to sort */
#define TNUM (NUMNUM / NTHR) /* number to sort per thread */

long nums[NUMNUM];
long snums[NUMNUM];

#ifdef SOLARIS
#define heapsort qsort
#else
extern int heapsort(void *, size_t, size_t,
                    int (*)(const void *, const void *));
#endif

typedef struct _pthread_mybarrier_t
{
    pthread_cond_t cond;
    pthread_mutex_t cond_lock;
    pthread_mutex_t count_lock;
    int max_count;
    int curr_count;
} pthread_mybarrier_t;

int pthread_mybarrier_init(pthread_mybarrier_t *barrier, unsigned int max_count)
{
    int err;
    if (barrier == NULL)
        barrier = malloc(sizeof(pthread_mybarrier_t));
    if ((err = pthread_cond_init(&barrier->cond, NULL)) != 0)
        return (err);
    if ((err = pthread_mutex_init(&barrier->cond_lock, NULL)) != 0)
        return (err);
    if ((pthread_mutex_init(&barrier->count_lock, NULL)) != 0)
        return (err);
    barrier->max_count = max_count;
    barrier->curr_count = 0;
    return 0;
}

int pthread_mybarrier_wait(pthread_mybarrier_t *barrier)
{
    pthread_mutex_lock(&barrier->count_lock);
    pthread_mutex_lock(&barrier->cond_lock);
    barrier->curr_count++;
    if (barrier->curr_count == barrier->max_count)
    {
        barrier->curr_count = 0;
        pthread_cond_broadcast(&barrier->cond);

        pthread_mutex_unlock(&barrier->count_lock);
        pthread_mutex_unlock(&barrier->cond_lock);
        return 0;
    }
    pthread_mutex_unlock(&barrier->count_lock);

    pthread_cond_wait(&barrier->cond, &barrier->cond_lock);
    pthread_mutex_unlock(&barrier->cond_lock);
    return 0;
}

pthread_mybarrier_t b;

/*
 * Compare two long integers (helper function for heapsort)
 */
int complong(const void *arg1, const void *arg2)
{
    long l1 = *(long *)arg1;
    long l2 = *(long *)arg2;
    if (l1 == l2)
        return 0;
    else if (l1 < l2)
        return -1;
    else
        return 1;
}

/*
 * Worker thread to sort a portion of the set of numbers.
 */
void *
thr_fn(void *arg)
{
    long idx = (long)arg;
    qsort(&nums[idx], TNUM, sizeof(long), complong);
    pthread_mybarrier_wait(&b);
    /*
     * Go off and perform more work ...
     */
    return ((void *)0);
}

/*
 * Merge the results of the individual sorted ranges.
 */
void merge()
{
    long idx[NTHR];
    long i, minidx, sidx, num;
    for (i = 0; i < NTHR; i++)
        idx[i] = i * TNUM;
    for (sidx = 0; sidx < NUMNUM; sidx++)
    {
        num = LONG_MAX;
        for (i = 0; i < NTHR; i++)
        {
            if ((idx[i] < (i + 1) * TNUM) && (nums[idx[i]] < num))
            {
                num = nums[idx[i]];
                minidx = i;
            }
        }
        snums[sidx] = nums[idx[minidx]];
        idx[minidx]++;
    }
}

int main()
{
    unsigned long i;
    struct timeval start, end;
    long long startusec, endusec;
    double elapsed;
    int err;
    pthread_t tid;
    /*
     * Create the initial set of numbers to sort.
     */
    srandom(1);
    for (i = 0; i < NUMNUM; i++)
        nums[i] = random();
    /*
     * Create 8 threads to sort the numbers.
     */
    gettimeofday(&start, NULL);
    pthread_mybarrier_init(&b, NTHR + 1);
    for (i = 0; i < NTHR; i++)
    {
        err = pthread_create(&tid, NULL, thr_fn, (void *)(i * TNUM));
        if (err != 0)
            err_exit(err, "can’t create thread");
    }
    pthread_mybarrier_wait(&b);
    merge();
    gettimeofday(&end, NULL);
    /*
     * Print the sorted list.
     */
    startusec = start.tv_sec * 1000000 + start.tv_usec;
    endusec = end.tv_sec * 1000000 + end.tv_usec;
    elapsed = (double)(endusec - startusec) / 1000000.0;
    printf("sort took %.4f seconds\n", elapsed);
    for (i = 0; i < NUMNUM; i++)
        printf("%ld\n", snums[i]);
    exit(0);
}
