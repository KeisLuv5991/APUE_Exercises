#include "apue.h"
#include <dirent.h>
#include <limits.h>
#include <time.h>
#include <sys/time.h>
#include <ctype.h>
#include <fcntl.h>

/* function type that is called for each filename */
typedef int Myfunc(const char *, const struct stat *, int);
static Myfunc myfunc;
static int myftw(char *, Myfunc *);
static int dopath(Myfunc *);

static time_t predicate_time;
static int yflag, pflag, dflag, tflag, qflag, hflag;

int main(int argc, char *argv[])
{
    int c, ret, fd;
    char *tvalue = NULL;
    time_t predicate_sec = 7 * 24 * 60 * 60;
    struct tm tm;
    FILE *fp;

    opterr = 0;
    while ((c = getopt(argc, argv, "ypdt:qh")) != -1)
        switch (c)
        {
        case 'y':
            yflag = 1;
            break;
        case 'p':
            pflag = 1;
            break;
        case 'd':
            dflag = 1;
            break;
        case 't':
            tflag = 1;
            tvalue = optarg;
            break;
        case 'q':
            qflag = 1;
            break;
        case 'h':
            pflag = 1;
            break;
        case '?':
            if (optopt == 'c')
                fprintf(stderr, "Option -%c requires an argument.\n", optopt);
            else if (isprint(optopt))
                fprintf(stderr, "Unknown option `-%c'.\n", optopt);
            else
                fprintf(stderr,
                        "Unknown option character `\\x%x'.\n",
                        optopt);
            return 1;
        default:
            abort();
        }

    if (yflag && qflag)
        if ((fp = freopen("/dev/null", "r+", stdout)) == NULL)
            err_sys("freopen error");

    if (tflag)
    {
        strptime(tvalue, "%j:%H:%M:%S", &tm);
        predicate_sec = mktime(&tm);
    }
    predicate_time = time(NULL) - predicate_sec;

    ret = myftw(argv[optind], myfunc);

    exit(ret);
}

/*
 * Descend through the hierarchy, starting at "pathname".
 * The caller’s func() is called for every file.
 */
#define FTW_F 1   /* file other than directory */
#define FTW_D 2   /* directory */
#define FTW_DNR 3 /* directory that can’t be read */
#define FTW_NS 4  /* file that we can’t stat */

static char *fullpath; /* contains full pathname for every file */
static size_t pathlen;

static int /* we return whatever func() returns */
myftw(char *pathname, Myfunc *func)
{
    fullpath = path_alloc(&pathlen); /* malloc PATH_MAX+1 bytes */

    if (pathlen <= strlen(pathname))
    {
        pathlen = strlen(pathname) * 2;
        if ((fullpath = realloc(fullpath, pathlen)) == NULL)
            err_sys("realloc failed");
    }

    strcpy(fullpath, pathname);

    return (dopath(func));
}

/*
 * Descend through the hierarchy, starting at "fullpath".
 * If "fullpath" is anything other than a directory, we lstat() it,
 * call func(), and return. For a directory, we call ourself
 * recursively for each name in the directory.
 */
static int /* we return whatever func() returns */
dopath(Myfunc *func)
{
    struct stat statbuf, tempstatbuf;
    struct dirent *dirp;
    DIR *dp;
    int ret, n;
    char *temppath;

    if (lstat(fullpath, &statbuf) < 0) /* stat error */
        return (func(fullpath, &statbuf, FTW_NS));

    if (S_ISDIR(statbuf.st_mode) == 0) /* not a directory */
        return (func(fullpath, &statbuf, FTW_F));

    /*
     * It’s a directory. First call func() for the directory,
     * then process each filename in the directory.
     */
    temppath = malloc(strlen(fullpath));
    strcpy(temppath, fullpath);
    tempstatbuf = statbuf;

    n = strlen(fullpath);
    if (n + NAME_MAX + 2 > pathlen)
    { /* expand path buffer */
        pathlen *= 2;
        if ((fullpath = realloc(fullpath, pathlen)) == NULL)
            err_sys("realloc failed");
    }

    fullpath[n++] = '/';
    fullpath[n] = 0;

    if ((dp = opendir(fullpath)) == NULL) /* can’t read directory */
        return (func(fullpath, &statbuf, FTW_DNR));

    while ((dirp = readdir(dp)) != NULL)
    {
        if (strcmp(dirp->d_name, ".") == 0 ||
            strcmp(dirp->d_name, "..") == 0)
            continue;                       /* ignore dot and dot-dot */
        strcpy(&fullpath[n], dirp->d_name); /* append name after "/" */
        if ((ret = dopath(func)) != 0)      /* recursive */
            break;                          /* time to leave */
    }

    if ((ret = func(temppath, &tempstatbuf, FTW_D)) != 0)
        return (ret);

    fullpath[n - 1] = 0; /* erase everything from slash onward */
    if (closedir(dp) < 0)
        err_ret("can’t close directory %s", fullpath);

    return (ret);
}

static int
myfunc(const char *pathname, const struct stat *statptr, int type)
{
    time_t atime, mtime, lasttime;
    char response[10];
    struct stat statbuf;
    struct dirent *dirp;
    DIR *dp;
    int dirdelflag = 1;

    switch (type)
    {
    case FTW_F:
        atime = (statptr->st_atim).tv_sec;
        mtime = (statptr->st_mtim).tv_sec;
        if (atime < mtime)
            lasttime = mtime;
        else
            lasttime = atime;
        if (lasttime <= predicate_time)
        {
            printf("%s\t\t - file - ", pathname);
            if (!pflag)
            {
                if (yflag)
                {
                    if (unlink(pathname) < 0)
                        err_ret("can't unlink file %s", pathname);
                    printf("file deleted.");
                }
                else
                {
                    printf("(y/n) : ");
                    fgets(response, 10, stdin);
                    if (response[0] == 'y')
                    {
                        if (unlink(pathname) < 0)
                            err_ret("can't unlink file %s", pathname);
                        printf("file deleted.");
                    }
                    else if (response[0] == 'n')
                        printf("file not deleted.");
                    else
                        printf("Not valid response.");
                }
            }
            printf("\n");
        }
        break;
    case FTW_D:
        atime = (statptr->st_atim).tv_sec;
        mtime = (statptr->st_mtim).tv_sec;
        if (atime < mtime)
            lasttime = mtime;
        else
            lasttime = atime;
        if ((dp = opendir(pathname)) == NULL) /* can’t read directory */
            err_ret("can’t read directory %s", pathname);

        while ((dirp = readdir(dp)) != NULL)
        {
            if (strcmp(dirp->d_name, ".") == 0 ||
                strcmp(dirp->d_name, "..") == 0)
                continue; /* ignore dot and dot-dot */
            dirdelflag = 0;
            break;
        }
        if (dirdelflag && !dflag)
        {
            if (lasttime <= predicate_time)
            {
                printf("%s\t\t - directory - ", pathname);
                if (!pflag)
                {
                    if (yflag)
                    {
                        if (rmdir(pathname) < 0)
                            err_ret("can't remove directory %s", pathname);
                        printf("directory deleted.");
                    }
                    else
                    {
                        printf("(y/n) : ");
                        fgets(response, 10, stdin);
                        if (response[0] == 'y')
                        {
                            if (rmdir(pathname) < 0)
                                err_ret("can't remove directory %s", pathname);
                            printf("directory deleted.");
                        }
                        else if (response[0] == 'n')
                            printf("directory not deleted.");
                        else
                            printf("Not valid response.");
                    }
                }
                printf("\n");
            }
        }
        break;
    case FTW_DNR:
        err_ret("can’t read directory %s", pathname);
        break;
    case FTW_NS:
        err_ret("stat error for %s", pathname);
        break;
    default:
        err_dump("unknown type %d for pathname %s", type, pathname);
    }
    return (0);
}
