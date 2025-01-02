#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <getopt.h>
#include <libgen.h>

#define INODE_FLAG 0b1
#define TYPE_FLAG 0b10
#define SIZE_FLAG 0b100
#define USER_FLAG 0b1000
#define GROUP_FLAG 0b10000
#define MODE_FLAG 0b100000
#define ALL_FLAGS 0b111111
#define USER_COLOR_FLAG 0b1000000
#define GROUP_COLOR_FLAG 0b10000000
#define TYPE_COLOR_FLAG 0b100000000
#define EXT_COLOR_FLAG 0b1000000000
#define FULL_PATH_FLAG 0b10000000000
#define LIMIT_FLAG 0b100000000000
#define DEPTH_FLAG 0b1000000000000
#define HIDDEN_FLAG 0b10000000000000

unsigned int flags = 0;
ssize_t max_depth;
ssize_t visited_limit;
ssize_t visited_num = 0;
char *color_white = "\033[0m";
char *color_file = "\033[38;5;50m";
char *color_dir = "\033[38;5;63m";
char *color_symlink = "\033[38;5;199m";
char *color_fifo = "\033[38;5;33m";     // Giallo per FIFO
char *color_other = "\033[38;5;31m";    // Rosso per altri

void print_help(const char *prog_name) {
    printf("Usage: %s [OPTIONS] <directory>\n", prog_name);
    printf("\n");
    
    printf("Options for displaying information:\n");
    printf("  -f, --fullpath        Print only the full path of the node\n");
    printf("  -c, --count           At the end print the number of visited node\n");
    printf("  -a, --all             Show all following information\n");
    printf("  -i, --inode           Show the inode id\n");
    printf("  -t, --type            Show the type of the inode (file, dir, symlink, etc.)\n");
    printf("  -s, --size            Show the size of the inode in bytes\n");
    printf("  -u, --user            Show the username of the owner\n");
    printf("  -g, --group           Show the group name of the owner\n");
    printf("  -m, --mode            Show the permissions in octal format\n");
    printf("\n");
    
    printf("Options for exploring:\n");
    printf("  -A, --AllFiles        Show also hidden files\n");
    printf("  -d [n], --depth [n]   Specify the maximum directory depth to explore\n");
    printf("  -l [n], --limit [n]   Limit the exploration at n node at least\n");
    printf("\n");
    
    printf("Options for colorizing the output:\n");
    printf("  -T, --TypeColor       Colorize output based on the inode type (file, dir, symlink, etc.)\n");
    printf("  -E, --ExtensionColor  Colorize output based on the file extensions\n");
    printf("  -U, --UserColor       Colorize output based on the username\n");
    printf("  -G, --GroupColor      Colorize output based on the group\n");
    printf("\n");
    printf("Other options:\n");
    printf("  -h, --help            Display this help message and exit\n");
    printf("\n");
    printf("Example usage:\n");
    printf("  %s -a -d 3 -T /path/to/directory\n", prog_name);
    printf("  %s -u -g -m /path/to/directory\n", prog_name);
}

inline char *generate_color(const int id) {
    char *gen_color = malloc(16);
    int base = 31 + (id % 200);
    snprintf(gen_color, 16, "\033[38;5;%dm", base);
    return gen_color;
}

inline char *generate_color_ext(char *ext) {
    unsigned int id = 0;
    if(ext){
        while (*ext) {
            id = (id * 31) + (unsigned char)(*ext);
            ext++;
        }
    }
    char *gen_color = malloc(16);
    int base = 31 + (id % 200);
    snprintf(gen_color, 16, "\033[38;5;%dm", base);
    return gen_color;
}

void print_details(const char *restrict name, const char *restrict path, const struct stat *statbuf, const char *restrict prefix, const ssize_t level) {
    if(flags & LIMIT_FLAG && visited_num == visited_limit){
        return;
    }
    visited_num += 1;
    char *actual_color;
    if(flags & TYPE_COLOR_FLAG){
        if (S_ISREG(statbuf->st_mode)) {
            actual_color = color_file;
        } else if (S_ISDIR(statbuf->st_mode)) {
            actual_color = color_dir;
        } else if (S_ISLNK(statbuf->st_mode)) {
            actual_color = color_symlink;
        } else if (S_ISFIFO(statbuf->st_mode)) {
            actual_color = color_fifo;
        } else {
            actual_color = color_other;
        }
    }else if(flags & USER_COLOR_FLAG){
        actual_color = generate_color(statbuf->st_uid);
    }else if(flags & GROUP_COLOR_FLAG){
        actual_color = generate_color(statbuf->st_gid);
    }else if(flags & EXT_COLOR_FLAG){
        char *ext = strrchr(name, '.');
        actual_color = generate_color_ext(ext);
    }else{
        actual_color = color_white;
    }
    if(flags & FULL_PATH_FLAG)
        printf("%s%s%s\n", actual_color, prefix, path);
    else
        printf("%s%s%s\n", actual_color, prefix, name);
    if(flags & INODE_FLAG)
        printf("%s\tInode: %ld\n", prefix, (long)statbuf->st_ino);
    if(flags & TYPE_FLAG){
        printf("%s\tType: ", prefix);
        if (S_ISREG(statbuf->st_mode)) {
            printf("file\n");
        } else if (S_ISDIR(statbuf->st_mode)) {
            printf("directory\n");
        } else if (S_ISLNK(statbuf->st_mode)) {
            printf("symbolic link\n");
        } else if (S_ISFIFO(statbuf->st_mode)) {
            printf("FIFO\n");
        } else {
            printf("other\n");
        }
    }
    if(flags & SIZE_FLAG)
        printf("%s\tSize: %lld byte\n", prefix, (long long)statbuf->st_size);
    if(flags & USER_FLAG){
        struct passwd *pw = getpwuid(statbuf->st_uid);
        if (pw != NULL) {
            printf("%s\tOwner: %d %s\n", prefix, statbuf->st_uid, pw->pw_name);
        } else {
            printf("%s\tOwner: %d\n", prefix, statbuf->st_uid);
        }
    }
    if(flags & GROUP_FLAG){
        struct group *gr = getgrgid(statbuf->st_gid);
        if (gr != NULL) {
            printf("%s\tGroup: %d %s\n", prefix, statbuf->st_gid, gr->gr_name);
        } else {
            printf("%s\tGroup: %d\n", prefix, statbuf->st_gid);
        }
    }
    if (flags & MODE_FLAG){
        printf("%s\tMode: ", prefix);
        printf((statbuf->st_mode & S_IRUSR) ? "r" : "-");
        printf((statbuf->st_mode & S_IWUSR) ? "w" : "-");
        printf((statbuf->st_mode & S_IXUSR) ? "x" : "-");
        printf((statbuf->st_mode & S_IRGRP) ? "r" : "-");
        printf((statbuf->st_mode & S_IWGRP) ? "w" : "-");
        printf((statbuf->st_mode & S_IXGRP) ? "x" : "-");
        printf((statbuf->st_mode & S_IROTH) ? "r" : "-");
        printf((statbuf->st_mode & S_IWOTH) ? "w" : "-");
        printf((statbuf->st_mode & S_IXOTH) ? "x" : "-");
        if (statbuf->st_mode & S_ISUID) {
            printf(" (setuid bit set)");
        }
        if (statbuf->st_mode & S_ISGID) {
            printf(" (setgid bit set)");
        }
        if (statbuf->st_mode & S_ISVTX) {
            printf(" (sticky bit set)");
        }
        printf("\n");
    }
    if (S_ISDIR(statbuf->st_mode)) {
        if((flags & DEPTH_FLAG) && max_depth == level)
            return;
        DIR *dir;
        dir = opendir(path);
        if (dir == NULL) {
            perror("opendir");
            return;
        }
        struct dirent *entry;
        if((entry = readdir(dir)) != NULL){
            ssize_t new_level = level + 1;
            ssize_t new_tabs = level * 4;
            char new_buff[PATH_MAX];
            strncpy(new_buff, prefix, new_tabs);
            new_buff[new_tabs] = '\t';
            new_tabs += 1;
            new_buff[new_tabs] = '\t';
            new_tabs += 1;
            new_buff[new_tabs] = '|';
            new_tabs += 1;
            new_buff[new_tabs] = '\t';
            new_buff[new_tabs + 1] = '\0';
            printf("%s\tContent:\n", prefix);
            char dir_path[PATH_MAX];
            struct stat dir_stat;
            do{
                // Skip the special entries "." and ".."
                if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                    continue;
                }
                if(!(flags & HIDDEN_FLAG))
                    if(entry->d_name[0] == '.')
                        continue;
                // Construct the full path
                snprintf(dir_path, sizeof(dir_path), "%s/%s", path, entry->d_name);
                if (lstat(dir_path, &dir_stat) == -1) {
                    perror("lstat");
                    continue;
                }
                print_details(entry->d_name, dir_path, &dir_stat, new_buff, level + 1);
            }while ((entry = readdir(dir)) != NULL);
            new_buff[new_tabs] = '-';
            new_buff[new_tabs - 1] = '-';
            printf("%s%s------------------------------\n", actual_color, new_buff);
        }
        closedir(dir);
    }
    printf("%s%s\n%s", actual_color, prefix, color_white);
    if(flags & (USER_COLOR_FLAG | GROUP_COLOR_FLAG | EXT_COLOR_FLAG)){
        free(actual_color);
    }
}

int main(int argc, char *argv[]) {
    int opt;
    struct option long_options[] = {
        {"AllFiles", no_argument, 0, 'A'},
        {"all", no_argument, 0, 'a'},
        {"count", no_argument, 0, 'c'},
        {"inode", no_argument, 0, 'i'},
        {"user", no_argument, 0, 'u'},
        {"group", no_argument, 0, 'g'},
        {"permissions", no_argument, 0, 'p'},
        {"type", no_argument, 0, 't'},
        {"size", no_argument, 0, 's'},
        {"help", no_argument, 0, 'h'},
        {"depth", required_argument, 0, 'd'},
        {"limit", required_argument, 0, 'l'},
        {"fullpath", no_argument, 0, 'f'},
        {"UserColor", no_argument, 0, 'U'},
        {"GroupColor", no_argument, 0, 'G'},
        {"TypeColor", no_argument, 0, 'T'},
        {"ExtensionColor", no_argument, 0, 'E'},
        {0, 0, 0, 0}
    };
    unsigned count = 0;
    while ((opt = getopt_long(argc, argv, "aAciugftsmhUGTEl:d:", long_options, NULL)) != -1) {
        switch (opt) {
            case 'c':
                count = 1;
                break;
            case 'A':
                flags |= HIDDEN_FLAG;
                break;
            case 'a':
                flags |= ALL_FLAGS;
                break;
            case 'i':
                flags |= INODE_FLAG;
                break;
            case 'u':
                flags |= USER_FLAG;
                break;
            case 'g':
                flags |= GROUP_FLAG;
                break;
            case 't':
                flags |= TYPE_FLAG;
                break;
            case 's':
                flags |= SIZE_FLAG;
                break;
            case 'h':
                print_help(argv[0]);
                return EXIT_SUCCESS;
            case 'f':
                flags |= FULL_PATH_FLAG;
                break;
            case 'm':
                flags |= MODE_FLAG;
                break;
            case 'd':
                flags |= DEPTH_FLAG;
                max_depth = atoi(optarg);
                break;
            case 'l':
                flags |= LIMIT_FLAG;
                visited_limit = atoi(optarg);
                break;
            case 'U':
                flags |= USER_COLOR_FLAG;
                break;
            case 'G':
                flags |= GROUP_COLOR_FLAG;
                break;
            case 'T':
                flags |= TYPE_COLOR_FLAG;
                break;
            case 'E':
                flags |= EXT_COLOR_FLAG;
                break;
            default:
                fprintf(stderr, "Unknown option. Use -h or --help for usage.\n");
                return EXIT_FAILURE;
        }
    }
    char *dir_path;
    if (optind >= argc || strcmp(argv[optind], ".") == 0) {
        char buff[PATH_MAX];
        dir_path = getcwd(buff, PATH_MAX);
        if(dir_path == NULL){
            perror("getcwd");
            return EXIT_FAILURE;
        }
    }else{
        dir_path = argv[optind];
    }
    // Print the details of the root directory
    struct stat statbuf;
    if (lstat(dir_path, &statbuf) == -1) {
        perror("lstat");
        return EXIT_FAILURE;
    }
    char *path_copy = strdup(dir_path);  // Copia della stringa per modificare senza alterare l'originale
    char *name = basename(path_copy); 
    print_details(name, dir_path, &statbuf, "", 0);
    free(path_copy);
    if(count){
        printf("%ld\n", visited_num);
    }
    return EXIT_SUCCESS;
}

