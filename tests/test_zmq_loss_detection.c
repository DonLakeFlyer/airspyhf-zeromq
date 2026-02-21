#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <zmq.h>

#define AIRSPYHF_ZMQ_MAGIC 0x5a514941u
#define AIRSPYHF_ZMQ_VERSION 1u
#define TEST_ENDPOINT "tcp://127.0.0.1:5555"
#define REQUIRED_PACKETS 8

typedef struct
{
    uint32_t magic;
    uint16_t version;
    uint16_t header_size;
    uint64_t sequence;
    uint64_t timestamp_us;
    uint32_t sample_rate;
    uint32_t sample_count;
    uint32_t payload_bytes;
    uint32_t flags;
} airspyhf_zmq_packet_hdr_t;

static int child_running(pid_t pid)
{
    int status = 0;
    pid_t result = waitpid(pid, &status, WNOHANG);
    if (result == 0) {
        return 1;
    }
    return 0;
}

static void stop_child(pid_t pid)
{
    if (pid <= 0) {
        return;
    }

    if (child_running(pid)) {
        kill(pid, SIGINT);
        usleep(200000);
    }

    if (child_running(pid)) {
        kill(pid, SIGTERM);
        usleep(200000);
    }

    if (child_running(pid)) {
        kill(pid, SIGKILL);
    }

    waitpid(pid, NULL, 0);
}

int main(int argc, char** argv)
{
    void* zmq_ctx = NULL;
    void* sub = NULL;
    pid_t rx_pid = -1;
    int rc;
    int received_packets = 0;
    uint64_t prev_sequence = 0;
    uint64_t total_missing = 0;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <path-to-airspyhf_zeromq_rx>\n", argv[0]);
        return EXIT_FAILURE;
    }

    rx_pid = fork();
    if (rx_pid < 0) {
        perror("fork");
        return EXIT_FAILURE;
    }

    if (rx_pid == 0) {
        execl(argv[1], argv[1], "-f", "146", "-g", "off", "-t", "0", "-m", "on", (char*)NULL);
        perror("execl");
        _exit(127);
    }

    zmq_ctx = zmq_ctx_new();
    if (zmq_ctx == NULL) {
        fprintf(stderr, "zmq_ctx_new failed: %s\n", zmq_strerror(errno));
        stop_child(rx_pid);
        return EXIT_FAILURE;
    }

    sub = zmq_socket(zmq_ctx, ZMQ_SUB);
    if (sub == NULL) {
        fprintf(stderr, "zmq_socket(ZMQ_SUB) failed: %s\n", zmq_strerror(errno));
        stop_child(rx_pid);
        zmq_ctx_term(zmq_ctx);
        return EXIT_FAILURE;
    }

    {
        int rcv_hwm = 1;
        int conflate = 1;
        int timeout_ms = 5000;
        zmq_setsockopt(sub, ZMQ_RCVHWM, &rcv_hwm, sizeof(rcv_hwm));
        zmq_setsockopt(sub, ZMQ_CONFLATE, &conflate, sizeof(conflate));
        zmq_setsockopt(sub, ZMQ_RCVTIMEO, &timeout_ms, sizeof(timeout_ms));
    }

    rc = zmq_setsockopt(sub, ZMQ_SUBSCRIBE, "", 0);
    if (rc != 0) {
        fprintf(stderr, "zmq_setsockopt(ZMQ_SUBSCRIBE) failed: %s\n", zmq_strerror(errno));
        stop_child(rx_pid);
        zmq_close(sub);
        zmq_ctx_term(zmq_ctx);
        return EXIT_FAILURE;
    }

    rc = zmq_connect(sub, TEST_ENDPOINT);
    if (rc != 0) {
        fprintf(stderr, "zmq_connect(%s) failed: %s\n", TEST_ENDPOINT, zmq_strerror(errno));
        stop_child(rx_pid);
        zmq_close(sub);
        zmq_ctx_term(zmq_ctx);
        return EXIT_FAILURE;
    }

    usleep(300000);

    while (received_packets < REQUIRED_PACKETS) {
        zmq_msg_t msg;
        airspyhf_zmq_packet_hdr_t hdr;
        const void* msg_data;
        size_t msg_size;

        if (!child_running(rx_pid)) {
            fprintf(stderr, "airspyhf_zeromq_rx exited before enough packets were received (likely no device available).\n");
            zmq_close(sub);
            zmq_ctx_term(zmq_ctx);
            return 77;
        }

        zmq_msg_init(&msg);
        rc = zmq_msg_recv(&msg, sub, 0);
        if (rc < 0) {
            zmq_msg_close(&msg);
            if (errno == EAGAIN) {
                continue;
            }
            fprintf(stderr, "zmq_msg_recv failed: %s\n", zmq_strerror(errno));
            stop_child(rx_pid);
            zmq_close(sub);
            zmq_ctx_term(zmq_ctx);
            return EXIT_FAILURE;
        }

        msg_data = zmq_msg_data(&msg);
        msg_size = zmq_msg_size(&msg);

        if (msg_size < sizeof(airspyhf_zmq_packet_hdr_t)) {
            fprintf(stderr, "Message too short: %zu\n", msg_size);
            zmq_msg_close(&msg);
            stop_child(rx_pid);
            zmq_close(sub);
            zmq_ctx_term(zmq_ctx);
            return EXIT_FAILURE;
        }

        memcpy(&hdr, msg_data, sizeof(hdr));

        if (hdr.magic != AIRSPYHF_ZMQ_MAGIC || hdr.version != AIRSPYHF_ZMQ_VERSION) {
            fprintf(stderr, "Invalid packet header.\n");
            zmq_msg_close(&msg);
            stop_child(rx_pid);
            zmq_close(sub);
            zmq_ctx_term(zmq_ctx);
            return EXIT_FAILURE;
        }

        if (received_packets > 0 && hdr.sequence > prev_sequence + 1) {
            total_missing += (hdr.sequence - (prev_sequence + 1));
        }

        prev_sequence = hdr.sequence;
        received_packets++;

        zmq_msg_close(&msg);

        usleep(1200000);
    }

    stop_child(rx_pid);
    zmq_close(sub);
    zmq_ctx_term(zmq_ctx);

    if (total_missing == 0) {
        fprintf(stderr, "No packet loss detected with intentionally slow consumer.\n");
        return EXIT_FAILURE;
    }

    printf("Detected packet loss as expected; missing packets: %llu\n", (unsigned long long)total_missing);
    return EXIT_SUCCESS;
}
