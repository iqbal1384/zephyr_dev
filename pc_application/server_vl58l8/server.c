#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>

#define ZONES_COUNT 64
#define ZONES_PER_LINE 8
#define RX_BUFFER_SIZE 4096
#define MAX_ZONES_PER_FRAME 64

struct frame {
    long timestamp_ms;
    int distance_mm[ZONES_COUNT];
    int target_detected[ZONES_COUNT];
};

static volatile int running = 1;

void signal_handler(int sig)
{
    (void)sig;
    running = 0;
}

int parse_frame(const char *line, struct frame *frame)
{
    if (!line || !frame) {
        return -1;
    }

    memset(frame, 0, sizeof(*frame));
    frame->timestamp_ms = -1;

    char *line_copy = strdup(line);
    if (!line_copy) {
        return -1;
    }

    char *saveptr = NULL;
    char *token = strtok_r(line_copy, ",", &saveptr);

    int parsed_count = 0;

    while (token && parsed_count < MAX_ZONES_PER_FRAME + 1) {
        char *eq = strchr(token, '=');
        if (!eq) {
            token = strtok_r(NULL, ",", &saveptr);
            continue;
        }

        *eq = '\0';
        const char *key = token;
        const char *value_str = eq + 1;
        long value = strtol(value_str, NULL, 10);

        if (strcmp(key, "ts") == 0) {
            frame->timestamp_ms = value;
        } else if (key[0] == 'z' && key[1] >= '0' && key[1] <= '9') {
            int zone_idx = (int)strtol(&key[1], NULL, 10);
            if (zone_idx >= 0 && zone_idx < ZONES_COUNT) {
                frame->distance_mm[zone_idx] = (int)value;
                frame->target_detected[zone_idx] = (value >= 0) ? 1 : 0;
            }
        }

        parsed_count++;
        token = strtok_r(NULL, ",", &saveptr);
    }

    free(line_copy);

    if (frame->timestamp_ms < 0) {
        return -1;
    }

    return 0;
}

void render_frame(const struct frame *frame)
{
    printf("\033[2J\033[H");
    printf("VL53L8A1 - distance results (%dx%d)\n", ZONES_PER_LINE, ZONES_PER_LINE);
    printf("ts=%ld ms\n\n", frame->timestamp_ms);

    for (int row = 0; row < ZONES_PER_LINE; row++) {
        for (int col = ZONES_PER_LINE - 1; col >= 0; col--) {
            int zone = row * ZONES_PER_LINE + col;
            if (frame->target_detected[zone]) {
                printf("| %4d mm ", frame->distance_mm[zone]);
            } else {
                printf("|    ---- ");
            }
        }
        printf("|\n");
    }
    printf("\n");
    fflush(stdout);
}

int main(int argc, char *argv[])
{
    int port = 5000;

    if (argc > 1) {
        port = atoi(argv[1]);
    }

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(server_fd);
        return 1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((unsigned short)port);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 1) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    printf("VL53L8A1 TCP Server listening on port %d\n", port);
    printf("Waiting for device connection...\n");
    fflush(stdout);

    while (running) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);

        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_fd < 0) {
            if (errno != EINTR) {
                perror("accept");
            }
            continue;
        }

        printf("Client connected: %s:%d\n",
               inet_ntoa(client_addr.sin_addr),
               ntohs(client_addr.sin_port));
        fflush(stdout);

        char buffer[RX_BUFFER_SIZE];
        struct frame frame;
        int offset = 0;

        while (running) {
            int n = recv(client_fd, &buffer[offset], RX_BUFFER_SIZE - offset - 1, 0);
            if (n <= 0) {
                if (n < 0) {
                    perror("recv");
                } else {
                    printf("Client disconnected.\n");
                }
                break;
            }

            offset += n;
            buffer[offset] = '\0';

            /* Process lines */
            char *line_start = buffer;
            char *newline;

            while ((newline = strchr(line_start, '\n')) != NULL) {
                *newline = '\0';

                if (parse_frame(line_start, &frame) == 0) {
                    render_frame(&frame);
                } else if (strlen(line_start) > 0) {
                    printf("Malformed frame: %s\n", line_start);
                    fflush(stdout);
                }

                line_start = newline + 1;
            }

            /* Shift leftover bytes to buffer start */
            int leftover = offset - (int)(line_start - buffer);
            if (leftover > 0) {
                memmove(buffer, line_start, leftover);
            }
            offset = leftover;
        }

        close(client_fd);
        printf("Waiting for next connection...\n");
        fflush(stdout);
    }

    close(server_fd);
    printf("Server shutdown.\n");
    return 0;
}
