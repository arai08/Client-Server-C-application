#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <signal.h>
#include <dirent.h>  // Directory handling
#include <time.h>

void crequest(int sock);
void error(const char *msg) {
    perror(msg);
    exit(1);
}

// Define a struct to hold directory names and their modification times
typedef struct {
    char name[256];
    time_t mod_time;
} DirEntry;

void handleDirlistA(int sock) {
    DIR *d;
    struct dirent *dir;
    char *path = getenv("HOME");  // Get path to home directory
    d = opendir(path);
    if (d) {
        char directories[1024] = ""; // Buffer to hold directory names
        while ((dir = readdir(d)) != NULL) {
            if (dir->d_type == DT_DIR) { // Check if it is a directory
                strcat(directories, dir->d_name);
                strcat(directories, "\n"); // Add new line for each directory
            }
        }
        closedir(d);
        // Sort directories alphabetically - simplified implementation
        // You might need to implement a proper sorting algorithm or use qsort
        write(sock, directories, strlen(directories)); // Send back to client
    } else {
        write(sock, "Failed to open directory", 24);
    }
}


// Comparator function for qsort
int compareByTime(const void *a, const void *b) {
    DirEntry *dirA = (DirEntry *)a;
    DirEntry *dirB = (DirEntry *)b;
    return (dirA->mod_time > dirB->mod_time) - (dirA->mod_time < dirB->mod_time);
}

void handleDirlistT(int sock) {
    DIR *d;
    struct dirent *dir;
    struct stat attrib; // Used to retrieve information about the file
    char *path = getenv("HOME");
    char dirPath[1024];
    DirEntry entries[256]; // Assume no more than 256 directories for simplicity
    int count = 0;

    d = opendir(path);
    if (d) {
        while ((dir = readdir(d)) != NULL && count < 256) {
            if (dir->d_type == DT_DIR) {
                strcpy(entries[count].name, dir->d_name);
                // Construct full path to the directory
                snprintf(dirPath, sizeof(dirPath), "%s/%s", path, dir->d_name);
                stat(dirPath, &attrib);
                entries[count].mod_time = attrib.st_mtime; // Use modification time
                count++;
            }
        }
        closedir(d);
        
        // Sort directories by time
        qsort(entries, count, sizeof(DirEntry), compareByTime);

        // Send sorted directory list to client
        char directories[1024] = "";
        for (int i = 0; i < count; i++) {
            strcat(directories, entries[i].name);
            strcat(directories, "\n");
        }
        write(sock, directories, strlen(directories));
    } else {
        write(sock, "Failed to open directory", 24);
    }
}

void handleW24fn(int sock, char *filename) {
    DIR *d;
    struct dirent *dir;
    struct stat attrib;
    char filePath[1024];
    char *path = getenv("HOME");  // Start search from the home directory
    char response[1024];

    int found = 0;
    d = opendir(path);
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            snprintf(filePath, sizeof(filePath), "%s/%s", path, dir->d_name);
            if (dir->d_type == DT_REG && strcmp(dir->d_name, filename) == 0) {
                // File found, get details
                stat(filePath, &attrib);
                snprintf(response, sizeof(response), "%s %ld %s %o", 
                         dir->d_name, attrib.st_size, ctime(&attrib.st_mtime), attrib.st_mode & 0777);
                write(sock, response, strlen(response));
                found = 1;
                break;
            }
        }
        closedir(d);
    }
    if (!found) {
        write(sock, "File not found", 14);
    }
}

void createAndSendArchive(int sock, const char* minSize, const char* maxSize) {
    long size1 = atol(minSize);
    long size2 = atol(maxSize);
    char command[512];
    char tempFileName[512];
    char gzFileName[512];  // To hold the name of the gzipped file

    // Create a unique temporary file name to store the tar
    int fd = mkstemp(strcpy(tempFileName, "/tmp/w24fzXXXXXX"));
    if (fd == -1) {
        write(sock, "Failed to create temp file", 26);
        return;
    }
    close(fd);  // Close the file descriptor as we only need the name

    // Append ".tar" to the temporary file name for clarity in commands
    strcat(tempFileName, ".tar");

    // Prepare the gzip file name
    snprintf(gzFileName, sizeof(gzFileName), "%s.gz", tempFileName);

    // Form the command to find files, archive them, and gzip the archive
    snprintf(command, sizeof(command), 
             "find ~ -type f -size +%ldc -size -%ldc -exec tar -rvf %s {} + && gzip -f %s",
             size1, size2, tempFileName, tempFileName);

    // Execute the command
    system(command);

    // Check if the gzipped tar file has been successfully created and is not empty
    struct stat st;
    if (stat(gzFileName, &st) == 0 && st.st_size > 0) {
        // Send the tar.gz file
        FILE *fp = fopen(gzFileName, "rb");
        char buf[1024];
        int bytes_read;
        while ((bytes_read = fread(buf, 1, sizeof(buf), fp)) > 0) {
            write(sock, buf, bytes_read);
        }
        fclose(fp);
    } else {
        write(sock, "No file found", 13);
    }

    // Clean up: Remove the tar.gz file after sending
    unlink(gzFileName);
}

void handleW24fz(int sock, char* buffer) {
    char *size1 = strtok(buffer + 6, " ");
    char *size2 = strtok(NULL, " ");

    if (size1 && size2) {
        createAndSendArchive(sock, size1, size2);
    } else {
        write(sock, "Invalid command format", 22);
    }
}

void handleW24ft(int sock, char* buffer) {
    char command[1024];
    char tempFileName[512];
    char gzFileName[512];
    int fd, found = 0;
    struct stat st;

    // Prepare filenames for tar and gz
    fd = mkstemp(strcpy(tempFileName, "/tmp/w24ftXXXXXX"));
    if (fd == -1) {
        write(sock, "Failed to create temp file", 26);
        return;
    }
    close(fd);

    strcat(tempFileName, ".tar");
    snprintf(gzFileName, sizeof(gzFileName), "%s.gz", tempFileName);

    // Create find command
    snprintf(command, sizeof(command), "find ~ -type f \\( ");
    char* token = strtok(buffer + 6, " ");  // Skip command part 'w24ft '
    while (token != NULL) {
        snprintf(command + strlen(command), sizeof(command) - strlen(command), 
                 "-name \"*.%s\" ", token);
        token = strtok(NULL, " ");
        if (token != NULL) {
            strcat(command, "-o ");  // Append OR for next extension
        }
    }
    strcat(command, "\\) -exec tar -rvf ");
    strcat(command, tempFileName);
    strcat(command, " {} +; gzip -f ");
    strcat(command, tempFileName);

    // Execute the command
    system(command);

    // Check if the gzipped file is non-empty
    if (stat(gzFileName, &st) == 0 && st.st_size > 0) {
        FILE *fp = fopen(gzFileName, "rb");
        char buf[1024];
        int bytes_read;
        while ((bytes_read = fread(buf, 1, sizeof(buf), fp)) > 0) {
            write(sock, buf, bytes_read);
        }
        fclose(fp);
    } else {
        write(sock, "No file found", 13);
    }

    // Clean up
    unlink(gzFileName);
}

void handleW24fdb(int sock, char* date) {
    char command[1024];
    char tempFileName[512];
    char gzFileName[512];
    int fd;
    struct stat st;

    // Create a temporary filename for the tar file
    fd = mkstemp(strcpy(tempFileName, "/tmp/w24fdbXXXXXX"));
    if (fd == -1) {
        write(sock, "Failed to create temp file", 26);
        return;
    }
    close(fd);  // Close the file descriptor as we only need the name for now

    strcat(tempFileName, ".tar");
    snprintf(gzFileName, sizeof(gzFileName), "%s.gz", tempFileName);

    // Formulate the command to find files and compress them into a tar.gz
    snprintf(command, sizeof(command),
             "find ~ -type f ! -newermt '%s' -exec tar -rvf %s {} +; gzip -f %s",
             date, tempFileName, tempFileName);

    // Execute the command to find files and compress them
    system(command);

    // Check if the compressed file exists and is not empty
    if (stat(gzFileName, &st) == 0 && st.st_size > 0) {
        // File exists and is not empty, send it
        FILE *fp = fopen(gzFileName, "rb");
        char buf[1024];
        int bytes_read;
        while (bytes_read = fread(buf, 1, 1024, fp)) {
            write(sock, buf, bytes_read);
        }
        fclose(fp);
    } else {
        write(sock, "No file found", 13);
    }

    // Cleanup
    unlink(gzFileName);
}

void handleW24fda(int sock, char* date) {
    char command[1024];
    char tempFileName[512];
    char gzFileName[512];
    int fd;
    struct stat st;

    // Create a temporary filename for the tar file
    fd = mkstemp(strcpy(tempFileName, "/tmp/w24fdaXXXXXX"));
    if (fd == -1) {
        write(sock, "Failed to create temp file", 26);
        return;
    }
    close(fd);  // Close the file descriptor as we only need the name for now

    strcat(tempFileName, ".tar");
    snprintf(gzFileName, sizeof(gzFileName), "%s.gz", tempFileName);

    // Formulate the command to find files and compress them into a tar.gz
    snprintf(command, sizeof(command),
             "find ~ -type f -newermt '%s' -exec tar -rvf %s {} +; gzip -f %s",
             date, tempFileName, tempFileName);

    // Execute the command to find files and compress them
    system(command);

    // Check if the compressed file exists and is not empty
    if (stat(gzFileName, &st) == 0 && st.st_size > 0) {
        // File exists and is not empty, send it
        FILE *fp = fopen(gzFileName, "rb");
        char buf[1024];
        int bytes_read;
        while (bytes_read = fread(buf, 1, 1024, fp)) {
            write(sock, buf, bytes_read);
        }
        fclose(fp);
    } else {
        write(sock, "No file found", 13);
    }

    // Cleanup
    unlink(gzFileName);
}


/*****************************************************
 * Function to handle interactions with a client
 *****************************************************/
void crequest(int sock) {
    int n;
    char buffer[256];
    char command[10];
    char filename[246];

    // Determine command type
    sscanf(buffer, "%s %s", command, filename);

    while(1) {
        bzero(buffer, 256);
        n = read(sock, buffer, 255);
        if (n < 0) error("ERROR reading from socket");
        printf("Received command: %s\n", buffer);

        if (strncmp("quitc", buffer, 5) == 0) {
            printf("Server: client disconnected\n");
            break;
        } else if (strncmp("dirlist -a", buffer, 10) == 0) {
            handleDirlistA(sock);
        } else if (strncmp("dirlist -t", buffer, 10) == 0) {
            handleDirlistT(sock);
        } else if (strncmp("w24fn", command, 5) == 0) {
            handleW24fn(sock, filename);
        } else if (strncmp("w24fz", buffer, 5) == 0) {
            handleW24fz(sock, buffer);
        } else if (strncmp("w24ft", buffer, 5) == 0) {
            handleW24ft(sock, buffer);
        } else if (strncmp("w24fdb", buffer, 6) == 0) {
            handleW24fdb(sock, buffer + 7);  // Pass the date part to the handler
        } else if (strncmp("w24fda", buffer, 6) == 0) {
            handleW24fda(sock, buffer + 7);
        } else {
            n = write(sock, "Command not recognized", 23);
            if (n < 0) error("ERROR writing to socket");
        }
    }
}


int main(int argc, char *argv[]) {
    int sockfd, newsockfd, portno;
    int connections_server = 0;
    int connections_mirror = 0;
    int connections_total = 0;
    socklen_t clilen;
    char buffer[256];
    struct sockaddr_in serv_addr, cli_addr;
    int n, pid;

    if (argc < 2) {
        fprintf(stderr,"ERROR, no port provided\n");
        exit(1);
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
       error("ERROR opening socket");

    bzero((char *) &serv_addr, sizeof(serv_addr));
    portno = atoi(argv[1]);

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
             error("ERROR on binding");

    listen(sockfd,5);
    clilen = sizeof(cli_addr);

    printf("Waiting for connections...\n");
    while (1) {
        newsockfd = accept(sockfd, 
                (struct sockaddr *) &cli_addr, 
                &clilen);
        if (newsockfd < 0) 
            error("ERROR on accept");

        pid = fork();
        if (pid < 0)
            error("ERROR on fork");

        if (pid == 0)  {
            close(sockfd);
            crequest(newsockfd);
            exit(0);
        }
        else close(newsockfd);
    } /* end of while */
    close(sockfd);
    return 0; /* we never get here */
}



