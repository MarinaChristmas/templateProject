#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h> 
#include <ws2tcpip.h> 
#include <pthread.h>
#include <time.h>

#include "cJSON.h"

#define PORT 9999
#define BUFFER_SIZE 1024
#define HISTORY_SIZE 100

typedef struct {
    double cpuUsage[HISTORY_SIZE];
    double availableMemory[HISTORY_SIZE];
    int currentIndex;
    pthread_mutex_t dataMutex;
} SystemData;

SystemData system_data;

void initializeSystemData() {
    for (int i = 0; i < HISTORY_SIZE; i++) {
        system_data.cpuUsage[i] = 0.0;
        system_data.availableMemory[i] = 0.0;
    }
    system_data.currentIndex = 0;
    pthread_mutex_init(&system_data.dataMutex, NULL);
}
void addSystemData(double cpuLoad, double memoryAvailable) {
    pthread_mutex_lock(&system_data.dataMutex);
    system_data.cpuUsage[system_data.currentIndex] = cpuLoad;
    system_data.availableMemory[system_data.currentIndex] = memoryAvailable;
    system_data.currentIndex = (system_data.currentIndex + 1) % HISTORY_SIZE;
    pthread_mutex_unlock(&system_data.dataMutex);
}

void generateDataFile() {
    FILE *dataFile = fopen("system_data.json", "w");

    pthread_mutex_lock(&system_data.dataMutex);
    for (int i = 0; i < HISTORY_SIZE; i++) {
        int index = (system_data.currentIndex + i) % HISTORY_SIZE; // Corrected Indexing
        fprintf(dataFile, "%d %f %f\n", i, system_data.cpuUsage[index], system_data.availableMemory[index]);
    }
    pthread_mutex_unlock(&system_data.dataMutex);

    fclose(dataFile);
}

void generateGnuplotScript() {
    FILE *plotScript = fopen("plot_script.gp", "w");

    fprintf(plotScript, "set terminal png size 800,600\n");
    fprintf(plotScript, "set output 'system_load.png'\n");
    fprintf(plotScript, "set title 'System Performance'\n");
    fprintf(plotScript, "set xlabel 'Time'\n");
    fprintf(plotScript, "set ylabel 'Percentage'\n");
    fprintf(plotScript, "plot 'system_data.json' using 1:2 with lines title 'CPU Usage', \\\n");
    fprintf(plotScript, "     'system_data.json' using 1:3 with lines title 'Available Memory'\n");

    fclose(plotScript);
}
void generatePlot() {
    generateDataFile();
    generateGnuplotScript();
    system("gnuplot plot_script.gp");
}

// Client handling thread
void *handleClient(void *socketDescriptor) {
    SOCKET clientSocket = *(SOCKET*)socketDescriptor; // Changed to SOCKET
    char dataBuffer[BUFFER_SIZE];
    int bytesRead;

    while ((bytesRead = recv(clientSocket, dataBuffer, BUFFER_SIZE - 1, 0)) > 0) {
        dataBuffer[bytesRead] = '\0';

        cJSON *jsonData = cJSON_Parse(dataBuffer);
        if (jsonData == NULL) {
            const char *errorPointer = cJSON_GetErrorPtr();
            break;
        }

        cJSON *cpuUsageJson = cJSON_GetObjectItemCaseSensitive(jsonData, "cpuUsage");
        cJSON *availableMemoryJson = cJSON_GetObjectItemCaseSensitive(jsonData, "availableMemory");

        if (cJSON_IsNumber(cpuUsageJson) && cJSON_IsNumber(availableMemoryJson)) {
            double cpuLoad = cpuUsageJson->valuedouble;
            double memoryAvailable = availableMemoryJson->valuedouble;

            printf("Received: CPU Usage: %f, Available Memory: %f\n", cpuLoad, memoryAvailable);
            addSystemData(cpuLoad, memoryAvailable);
            generatePlot();
        }

        cJSON_Delete(jsonData);
    }

    closesocket(clientSocket);  // Use closesocket on Windows
    free(socketDescriptor);
    return NULL;
}


int main() {
    SOCKET serverSocket, clientSocket, *newSocketPtr;  // Changed to SOCKET
    struct sockaddr_in serverAddress;
    int addressLength = sizeof(serverAddress);
    pthread_t threadId;
    WSADATA wsaData; //needed for winsock

     // Initialize Winsock
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);  // Request version 2.2
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }


    initializeSystemData();

    // Create socket
    if ((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) { // Changed to INVALID_SOCKET
        fprintf(stderr, "socket failed with error: %d\n", WSAGetLastError());  // Windows error reporting
        WSACleanup();
        exit(EXIT_FAILURE);
    }

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(PORT);

    // Bind socket
    if (bind(serverSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR) { // Changed to SOCKET_ERROR
        fprintf(stderr, "bind failed with error: %d\n", WSAGetLastError());
        closesocket(serverSocket);
        WSACleanup();
        exit(EXIT_FAILURE);
    }

    // Listen for connections
    if (listen(serverSocket, 3) == SOCKET_ERROR) {  // Changed to SOCKET_ERROR
        fprintf(stderr, "listen failed with error: %d\n", WSAGetLastError());
        closesocket(serverSocket);
        WSACleanup();
        exit(EXIT_FAILURE);
    }

    puts("Server listening...");

    while (1) {
        // Accept connection
        if ((clientSocket = accept(serverSocket, (struct sockaddr *)&serverAddress, (int*)&addressLength)) == INVALID_SOCKET) { // Changed to SOCKET and cast addrlen
            fprintf(stderr, "accept failed with error: %d\n", WSAGetLastError());
            continue;
        }
        puts("Connection accepted");

        newSocketPtr = malloc(sizeof(SOCKET)); // Allocate enough space for a SOCKET
        *newSocketPtr = clientSocket;

        // Create thread to handle the client
        if (pthread_create(&threadId, NULL, handleClient, (void*) newSocketPtr) != 0) { //Condition changed since pthread_create on windows returns 0 on success
            perror("could not create thread");
            free(newSocketPtr);
            closesocket(clientSocket);
            continue;
        }

        pthread_detach(threadId);
    }

    closesocket(serverSocket); // Use closesocket on Windows
    WSACleanup();
    pthread_mutex_destroy(&system_data.dataMutex);
    return 0;
}