#define _DEFAULT_SOURCE

/*
* Programming Assignment 02: lsv1.0.0
* This is the source file of version 1.0.0
* Read the write-up of the assignment to add the features to this base version
* Usage:
*       $ lsv1.0.0 
*       % lsv1.0.0  /home
*       $ lsv1.0.0  /home/kali/   /etc/
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <sys/ioctl.h>

extern int errno;

void do_ls(const char *dir, int horizontal);
void long_ls(const char *dir);

typedef struct
{
    char *name;
    struct stat info;
} FileInfo;

static int terminal_width(void)
{
    struct winsize window;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &window) == 0 &&
        window.ws_col > 0)
        return window.ws_col;
    return 80;
}

static void print_names(char **names, size_t count, size_t longest,
                        int horizontal)
{
    size_t spacing = 2;
    size_t column_width = longest + spacing;
    size_t columns = (size_t)terminal_width() / column_width;

    if (columns == 0)
        columns = 1;
    if (columns > count)
        columns = count;

    if (horizontal)
    {
        size_t position = 0;
        for (size_t i = 0; i < count; i++)
        {
            if (position > 0 && position + column_width >
                (size_t)terminal_width())
            {
                putchar('\n');
                position = 0;
            }
            printf("%-*s", (int)(i == count - 1 ? strlen(names[i]) :
                   column_width), names[i]);
            position += column_width;
        }
        if (count > 0)
            putchar('\n');
        return;
    }

    {
        size_t rows = (count + columns - 1) / columns;
        for (size_t row = 0; row < rows; row++)
        {
            for (size_t column = 0; column < columns; column++)
            {
                size_t index = row + column * rows;
                if (index >= count)
                    continue;
                printf("%-*s", (int)(column == columns - 1 ?
                       strlen(names[index]) : column_width), names[index]);
            }
            putchar('\n');
        }
    }
}

static void mode_to_string(mode_t mode, char *result)
{
    result[0] = S_ISDIR(mode) ? 'd' : S_ISLNK(mode) ? 'l' :
                S_ISCHR(mode) ? 'c' : S_ISBLK(mode) ? 'b' :
                S_ISFIFO(mode) ? 'p' : S_ISSOCK(mode) ? 's' : '-';
    result[1] = mode & S_IRUSR ? 'r' : '-';
    result[2] = mode & S_IWUSR ? 'w' : '-';
    result[3] = mode & S_IXUSR ? 'x' : '-';
    result[4] = mode & S_IRGRP ? 'r' : '-';
    result[5] = mode & S_IWGRP ? 'w' : '-';
    result[6] = mode & S_IXGRP ? 'x' : '-';
    result[7] = mode & S_IROTH ? 'r' : '-';
    result[8] = mode & S_IWOTH ? 'w' : '-';
    result[9] = mode & S_IXOTH ? 'x' : '-';
    if (mode & S_ISUID) result[3] = mode & S_IXUSR ? 's' : 'S';
    if (mode & S_ISGID) result[6] = mode & S_IXGRP ? 's' : 'S';
    if (mode & S_ISVTX) result[9] = mode & S_IXOTH ? 't' : 'T';
    result[10] = '\0';
}

int main(int argc, char const *argv[])
{
    int option;
    int long_format = 0;
    int horizontal = 0;

    while ((option = getopt(argc, (char * const *)argv, "lx")) != -1)
    {
        if (option == 'l')
            long_format = 1;
        else if (option == 'x')
            horizontal = 1;
        else
        {
            fprintf(stderr, "Usage: %s [-l] [-x] [directory ...]\n", argv[0]);
            return EXIT_FAILURE;
        }
    }

    if (optind == argc)
    {
        if (long_format)
            long_ls(".");
        else
            do_ls(".", horizontal);
    }
    else
    {
        for (int i = optind; i < argc; i++)
        {
            printf("Directory listing of %s : \n", argv[i]);
            if (long_format)
                long_ls(argv[i]);
            else
                do_ls(argv[i], horizontal);
	    puts("");
        }
    }
    return 0;
}

void do_ls(const char *dir, int horizontal)
{
    DIR *dp = opendir(dir);
    struct dirent *entry;
    char **names = NULL;
    size_t count = 0;
    size_t capacity = 0;
    size_t longest = 0;

    if (dp == NULL)
    {
        fprintf(stderr, "Cannot open directory : %s\n", dir);
        return;
    }

    errno = 0;
    while ((entry = readdir(dp)) != NULL)
    {
        size_t length;
        if (entry->d_name[0] == '.')
            continue;

        if (count == capacity)
        {
            size_t new_capacity = capacity == 0 ? 16 : capacity * 2;
            char **new_names = realloc(names, new_capacity * sizeof(*names));
            if (new_names == NULL)
            {
                perror("realloc failed");
                break;
            }
            names = new_names;
            capacity = new_capacity;
        }

        names[count] = strdup(entry->d_name);
        if (names[count] == NULL)
        {
            perror("strdup failed");
            break;
        }
        length = strlen(names[count]);
        if (length > longest)
            longest = length;
        count++;
    }

    if (errno != 0)
        perror("readdir failed");
    closedir(dp);

    if (count > 0)
        print_names(names, count, longest, horizontal);

    for (size_t i = 0; i < count; i++)
        free(names[i]);
    free(names);
}

void long_ls(const char *dir)
{
    DIR *dp = opendir(dir);
    struct dirent *entry;
    FileInfo *files = NULL;
    size_t count = 0;
    size_t capacity = 0;
    int links_width = 1;
    int size_width = 1;

    if (dp == NULL)
    {
        fprintf(stderr, "Cannot open directory : %s\n", dir);
        return;
    }

    errno = 0;
    while ((entry = readdir(dp)) != NULL)
    {
        char full_path[4096];
        int path_length;
        struct stat info;

        if (entry->d_name[0] == '.')
            continue;
        path_length = snprintf(full_path, sizeof(full_path), "%s/%s",
                               dir, entry->d_name);
        if (path_length < 0 || (size_t)path_length >= sizeof(full_path) ||
            lstat(full_path, &info) == -1)
            continue;

        if (count == capacity)
        {
            size_t new_capacity = capacity == 0 ? 16 : capacity * 2;
            FileInfo *new_files = realloc(files, new_capacity * sizeof(*files));
            if (new_files == NULL)
            {
                perror("realloc failed");
                break;
            }
            files = new_files;
            capacity = new_capacity;
        }

        files[count].name = strdup(entry->d_name);
        if (files[count].name == NULL)
        {
            perror("strdup failed");
            break;
        }
        files[count].info = info;
        count++;
    }

    if (errno != 0)
        perror("readdir failed");
    closedir(dp);

    for (size_t i = 0; i < count; i++)
    {
        int links = snprintf(NULL, 0, "%lu",
                             (unsigned long)files[i].info.st_nlink);
        int size = snprintf(NULL, 0, "%lld",
                            (long long)files[i].info.st_size);
        if (links > links_width) links_width = links;
        if (size > size_width) size_width = size;
    }

    for (size_t i = 0; i < count; i++)
    {
        char permissions[11];
        char timestamp[64];
        const struct passwd *user;
        const struct group *group;
        char full_path[4096];
        const char *owner;
        const char *group_name;
        int path_length = snprintf(full_path, sizeof(full_path), "%s/%s",
                                   dir, files[i].name);
        time_t modified = files[i].info.st_mtime;

        mode_to_string(files[i].info.st_mode, permissions);
        snprintf(timestamp, sizeof(timestamp), "%s", ctime(&modified));
        timestamp[strcspn(timestamp, "\n")] = '\0';
        user = getpwuid(files[i].info.st_uid);
        group = getgrgid(files[i].info.st_gid);
        owner = user == NULL ? "?" : user->pw_name;
        group_name = group == NULL ? "?" : group->gr_name;

        printf("%s %*lu %-8s %-8s %*lld %s %s",
               permissions, links_width,
               (unsigned long)files[i].info.st_nlink, owner, group_name,
               size_width, (long long)files[i].info.st_size,
               timestamp, files[i].name);

        if (path_length > 0 && (size_t)path_length < sizeof(full_path) &&
            S_ISLNK(files[i].info.st_mode))
        {
            char target[4096];
            ssize_t target_length = readlink(full_path, target,
                                             sizeof(target) - 1);
            if (target_length >= 0)
            {
                target[target_length] = '\0';
                printf(" -> %s", target);
            }
        }
        putchar('\n');
    }

    for (size_t i = 0; i < count; i++)
        free(files[i].name);
    free(files);
}
