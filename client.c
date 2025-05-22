#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <time.h>

#include "cJSON.h"

#define PORT 9999
#define SERVER_ADDRESS "127.0.0.1"
#define SLEEP_TIME 5 // Increased for better visibility

double getCpuUsage() {
    return (double)(rand() % 100);
}

double getAvailableMemory() {
    return (double)(rand() % 100);
}

int main() {
    SOCKET clientSocket = INVALID_SOCKET;
    struct sockaddr_in serverAddress;
    WSADATA wsaData;

    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }

    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == INVALID_SOCKET) {
        fprintf(stderr, "Socket creation error with error: %d\n", WSAGetLastError());
        WSACleanup();
        return -1;
    }

    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = inet_addr(SERVER_ADDRESS);
    serverAddress.sin_port = htons(PORT);

    if (connect(clientSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR) {
        fprintf(stderr, "Connection failed with error: %d\n", WSAGetLastError());
        closesocket(clientSocket);
        WSACleanup();
        return -1;
    }

    srand(time(NULL));

    char dataBuffer[1024];

    while (1) {
        double cpuUsage = getCpuUsage();
        double availableMemory = getAvailableMemory();

        cJSON *jsonData = cJSON_CreateObject();
        cJSON_AddNumberToObject(jsonData, "cpuUsage", cpuUsage);
        cJSON_AddNumberToObject(jsonData, "availableMemory", availableMemory);

        char *jsonString = cJSON_PrintUnformatted(jsonData);
        if (jsonString == NULL) {
            fprintf(stderr, "Failed to print JSON.\n");
            cJSON_Delete(jsonData);
            closesocket(clientSocket);
            WSACleanup();
            return 1;
        }

        snprintf(dataBuffer, sizeof(dataBuffer), "%s", jsonString);
        send(clientSocket, dataBuffer, (int)strlen(dataBuffer), 0);

        printf("Sent: CPU Usage: %f, Available Memory: %f\n", cpuUsage, availableMemory);

        cJSON_Delete(jsonData);
        free(jsonString);

        Sleep(SLEEP_TIME * 1000);
    }

    closesocket(clientSocket);
    WSACleanup();
    return 0;
}
