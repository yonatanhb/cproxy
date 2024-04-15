#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#define BUFFER_SIZE 1024

/**
 * Error msg
 * @param msg
 */
void error(const char *msg) {
    perror(msg);
    exit(1);
}

/**
 * Gets a path and creates folders in the system.
 * @param fullLocalPath
 */
void createDirectories(const char *fullLocalPath) {
    // Extract the directory path from the filepath
    char *dirPath = strdup(fullLocalPath);
    if (dirPath == NULL) {
        error("Memory allocation error");
    }

    // Remove the last component (file or directory) from the path
    char *lastSlash = strrchr(dirPath, '/');
    if (lastSlash != NULL) {
        *lastSlash = '\0'; // Null-terminate the string at the last slash
    }

    // Split the directory path into individual components
    char *token = strtok(dirPath, "/");
    char *parentPath = strdup(token);
    while (token != NULL) {
        // Create the parent directory if it doesn't exist
        if (access(parentPath, F_OK) == -1) {
            if (mkdir(parentPath, 0755) == -1) {
                free(dirPath);
                free(parentPath);
                perror("Error creating directories");
                return;
            }
        }
        token = strtok(NULL, "/");
        if (token != NULL) {
            // Update parentPath with the next directory component
            parentPath = (char*) realloc(parentPath, strlen(parentPath) + strlen(token) + 2);
            strcat(parentPath, "/");
            strcat(parentPath, token);
        }
    }

    // Free allocated memory
    free(dirPath);
    free(parentPath);
}

/**
 * Creates an HTTP request
 * @param hostname
 * @param port
 * @param filepath
 * @param request
 */
void constructRequest(const char *hostname, const char *filepath, char *request) {
    // Construct the HTTP GET request
    snprintf(request, BUFFER_SIZE, "GET %s HTTP/1.0\r\nHost: %s\r\n\r\n", filepath, hostname);
}

/**
 * Reads an HTTP response from the socket.
 * Prints the content to the screen.
 * Saves data to a file.
 * @param fd
 * @param fullLocalPath
 * @return
 */
ssize_t getResponse(int fd, char* fullLocalPath) {
    unsigned char responseBuffer[BUFFER_SIZE];
    ssize_t bytesRead, bodyStart, totalBytesRead = 0;
    int headerEnd = 0;  // Flag to indicate the end of headers
    int pageFound = 1;
    int localFile = -1;

    while ((bytesRead = read(fd, responseBuffer, BUFFER_SIZE)) > 0) {
        write(STDOUT_FILENO, responseBuffer, bytesRead);
        // Check for the status code only in the first iteration
        // if file OK create directories and open file for writing.
        if (totalBytesRead == 0) {
            if (bytesRead >= 15 && (strncmp((const char*)responseBuffer, "HTTP/1.1 200 ", 13) != 0 && strncmp((const char*)responseBuffer, "HTTP/1.0 200 ", 13) != 0)) {
                pageFound = 0;
            } else {
                createDirectories(fullLocalPath);
                // Open the local file for writing
                localFile = open(fullLocalPath, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
                if (localFile == -1) {
                    close(fd);
                    error("Error opening local file for writing");
                }
            }
        }

        // Find the position where headers end and the body starts
        bodyStart = 0;
        for (ssize_t i = 3; i < bytesRead; ++i) {
            if (responseBuffer[i - 3] == '\r' && responseBuffer[i - 2] == '\n' &&
                responseBuffer[i - 1] == '\r' && responseBuffer[i] == '\n') {
                headerEnd = 1;  // Set the flag indicating the end of headers
                bodyStart = i + 1;  // Set the position where the body starts
                break;
            }
        }

        //Write body to file.
        if (headerEnd && pageFound) {
            write(localFile, responseBuffer + bodyStart, bytesRead - bodyStart);
        }

        totalBytesRead += bytesRead;
    }

    if (bytesRead == -1) {
        close(fd);
        error("Error reading from server socket");
    }

    close(fd);
    return totalBytesRead;
}

/**
 * Gets a URL and retrieves the field data.
 * @param url
 * @param hostname
 * @param port
 * @param filepath
 */
void parseURL(const char *url, char **hostname, int *port, char **filepath) {
    // Skip "http://"
    url += strlen("http://");

    // Find the end of the hostname
    const char *endOfHostname = strchr(url, '/');
    size_t hostnameLength = (endOfHostname) ? (size_t)(endOfHostname - url) : strlen(url);

    // Allocate memory for hostname
    *hostname = (char *)malloc(hostnameLength + 1);
    if (*hostname == NULL) {
        perror("Memory allocation error");
        exit(EXIT_FAILURE);
    }
    strncpy(*hostname, url, hostnameLength);
    (*hostname)[hostnameLength] = '\0'; // Null-terminate the string

    // Find the filepath
    const char *startOfFilePath = (endOfHostname) ? endOfHostname : "/";
    size_t filepathLength = strlen(startOfFilePath);

    // Allocate memory for filepath
    *filepath = (char *)malloc(filepathLength + 1);
    if (*filepath == NULL) {
        perror("Memory allocation error");
        exit(EXIT_FAILURE);
    }
    strncpy(*filepath, startOfFilePath, filepathLength);
    (*filepath)[filepathLength] = '\0'; // Null-terminate the string

    // Extract port if specified
    char *colon = strchr(*hostname, ':');
    if (colon) {
        *port = atoi(colon + 1);
        *colon = '\0'; // Separate hostname and port
    } else {
        // Default port for HTTP
        *port = 80;
    }
}

/**
 * Gets a path and prints the file.
 * @param pathfile
 */
void printFile(const char *pathfile) {
    FILE *file = fopen(pathfile, "r");
    if (file == NULL) {
        perror("Error opening file");
        return;
    }

    char line[BUFFER_SIZE]; // Adjust the buffer size as needed

    while (fgets(line, sizeof(line), file) != NULL) {
        printf("%s", line);
    }

    fclose(file);
}

/**
 * Function to get the size of the file
 */
ssize_t getFileSize(const char *pathfile) {
    FILE *file = fopen(pathfile, "r");
    if (file == NULL) {
        perror("Error opening file");
        return 0; // Return 0 characters in case of an error
    }

    fseek(file, 0, SEEK_END); // Move the file pointer to the end of the file
    ssize_t charCount = ftell(file); // Get the current position, which is the file size
    fclose(file);

    return charCount;
}

/**
 * Function to get the full local path.
 */
char* getFullPath(const char *partialPath) {
    // Get the current working directory
    char currentDir[1024];
    if (getcwd(currentDir, sizeof(currentDir)) == NULL) {
        perror("getcwd");
        exit(EXIT_FAILURE);
    }

    // Concatenate the partial path to the current directory
    char *fullPath = (char*)malloc(strlen(currentDir) + strlen(partialPath) + 2); // +2 for '/' and null terminator
    if (fullPath == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    snprintf(fullPath, strlen(currentDir) + strlen(partialPath) + 2, "%s/%s", currentDir, partialPath);

    return fullPath;
}

/**
 * Runs a system function to open a file in the browser.
 * @param fullPath
 */
void openFileInBrowser(const char *fullPath) {
    /*char command[256];
    snprintf(command, sizeof(command), "xdg-open %s", fullPath);
    system(command);*/
    size_t commandLength = strlen("firefox ") + strlen(fullPath) + 1;
    char *command = (char *)malloc(commandLength * sizeof(char));
    if (command == NULL) {
        error("malloc");
    }
    sprintf(command, "firefox %s", fullPath);
    system(command);
    free(command);
}

/**
 * Creates a local HTTP response
 * @param response
 * @param sizefile
 * @return
 */
int constructHTTPResponse(char *response, int sizefile) {
    // Check if response is NULL
    if (response == NULL) {
        fprintf(stderr, "Error: Response buffer is NULL\n");
        return -1; // Return an error code
    }

    // Construct the HTTP response string and get the size
    int responseSize = snprintf(response, 100, "HTTP/1.0 200 OK\r\nContent-Length: %d\r\n\r\n", sizefile);

    // Check if snprintf encountered an error
    if (responseSize < 0) {
        fprintf(stderr, "Error: Failed to construct HTTP response\n");
        return -1; // Return an error code
    }

    return responseSize; // Return the size of the response
}

/**
 * Fixes the path to the file by adding index.html to the file if necessary.
 * @param path
 * @param hostname
 * @param partialUrl
 */
void checkAndSetPath(char **path, const char *hostname, const char *filepath) {
    // Concatenate hostname with filepath to get 'domain/path'.
    size_t urlLength = strlen(hostname) + strlen(filepath) + 1;
    char *partialUrl = (char *)malloc(urlLength);
    if (partialUrl == NULL) {
        error("Memory allocation error");
    }
    snprintf(partialUrl, urlLength, "%s%s", hostname, filepath);

    size_t path_len = strlen(partialUrl);
    size_t hostname_len = strlen(hostname);

    // Calculate the length of the new path
    size_t new_len = path_len + hostname_len + strlen("/index.html") + 1; // Add 1 for null terminator

    // Allocate memory for the new path
    *path = (char *)malloc(new_len);
    if (*path == NULL) {
        perror("Memory allocation error");
        return;
    }

    // Copy the original path into the new buffer
    strcpy(*path, partialUrl);

    // Check if path is equal to hostname
    if (strcmp(*path, hostname) == 0) {
        // Append "/index.html" to the path
        strcat(*path, "/index.html");
    } else {
        // Check if path ends with a slash
        if ((*path)[path_len - 1] == '/') {
            // Append "index.html" to the path
            strcat(*path, "index.html");
        }
    }
    free(partialUrl);
}

/**
 * Check if the path lead to a file.
 * @param path
 * @return
 */
int isFile(const char *path) {
    struct stat path_stat;
    stat(path, &path_stat);
    return S_ISREG(path_stat.st_mode);  // Check if it's a regular file
}

int main(int argc, char *argv[]) {
    char prefix[] = "http://";
    if (argc < 2 || argc > 3 || (argc == 3 && strcmp(argv[2], "-s") != 0) || strncmp(argv[1], prefix, strlen(prefix)) != 0) {
        fprintf(stderr, "Usage: cproxy <URL> [-s]\n");
        exit(1);
    }

    int shouldOpenBrowser = (argc == 3 && strcmp(argv[2], "-s") == 0);
    ssize_t size = 0;
    char* hostname;
    int port;
    char* filepath;


    // Step 1: Parse the URL
    parseURL(argv[1], &hostname, &port, &filepath);

    // step 2: check hostname by 'gethostbyname' function.
    struct hostent *hp; /*ptr to host info for remote*/
    hp = gethostbyname(hostname);
    if (hp == NULL) {
        free(hostname);
        free(filepath);
        herror("gethostbyname");
        exit(1);
    }

    //for index.html --> url: hostname/path/../..[index.html]
    char* url;
    checkAndSetPath(&url, hostname, filepath);

    // Step 3: Check if the file appears in the local filesystem
    if (access(url, F_OK) != -1 && isFile(url)) {
        // File exists locally
        printf("File is given from local filesystem\n");
        // Get the file size and update size

        ssize_t fileSize = getFileSize(url);

        if (fileSize >= 0) {
            size += fileSize;
        } else {
            // Handle error from getFileSize
            size = 0;
        }

        char* response = (char*)malloc(size + 100);
        if (response == NULL) {
            error("malloc");
        }
        size += constructHTTPResponse(response,(int) size);
        printf("%s",response);
        printFile(url);
        free(response);

    } else {
        // File not found locally
        // Step 3b: Construct an HTTP request
        char request[BUFFER_SIZE];
        constructRequest(hostname, filepath, request);
        printf("HTTP request =\n%s\nLEN = %zu\n", request, strlen(request));

        // Implement logic to connect to the server, send the HTTP request, receive the response, and save the file locally
        int fd;		/* socket descriptor */

        if((fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
            error("socket");
        }

        struct sockaddr_in peeraddr;
        peeraddr.sin_family = AF_INET;
        peeraddr.sin_addr.s_addr = ((struct in_addr*)(hp->h_addr))->s_addr;

        struct sockaddr_in srv;		/* used by connect() */

        /* connect: use the Internet address family */
        srv.sin_family = AF_INET;
        /* connect: socket ‘fd’ to port */
        srv.sin_port = htons(port);
        /* connect: connect to IP Address */
        srv.sin_addr.s_addr = peeraddr.sin_addr.s_addr;

        if(connect(fd, (struct sockaddr*) &srv, sizeof(srv)) < 0) {
            error("connect");
        }

        // Send HTTP request to the server using write()
        int nbytes; // used by write()
        if ((nbytes = write(fd, request, strlen(request))) < 0) {
            close(fd);
            error("Error sending request to server");
        }

        // Read and print response, create directories, write the file to local system.
        size = getResponse(fd, url);

        // Close the server socket
        close(fd);
    }

    printf("\n Total response bytes: %d\n",(int)size);

    // Step 2e: If the flag -s is specified, use 'system(path);' to present the file using the browser
    if (shouldOpenBrowser && access(url, F_OK) != -1) {
        char* fullLocalPath = getFullPath(url);
        openFileInBrowser(fullLocalPath);
        free(fullLocalPath);
    }

    // end of the program.
    // Free dynamically allocated memory.
    free(hostname);
    free(filepath);
    free(url);
    return 0;
}
