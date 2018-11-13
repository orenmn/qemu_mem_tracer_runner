#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdbool.h>
#include <signal.h>
#include <assert.h>

#define PRINT_TO_TTYS0(str) {           \
    fprintf(serial_port_ttyS0, str);    \
    fflush(serial_port_ttyS0);          \
}

#define TTYS0_PATH              ("/dev/ttyS0")
#define SCRIPT_LOCAL_COPY_PATH  ("~/workload_script_received_from_serial")
#define REDIRECT_TO_TTYS0       (" > /dev/ttyS0 2>&1")

int main(int argc, char **argv) {
    int result = 0;

    // if (freopen(TTYS0_PATH, "w", stdout) == NULL) {
    //     printf("failed to redirect stdout to /dev/ttyS0. errno: %d\n", errno);
    //     return 1;
    // }

    // if (freopen(TTYS0_PATH, "w", stderr) == NULL) {
    //     printf("failed to redirect stderr to /dev/ttyS0. errno: %d\n", errno);
    //     result = 1;
    //     goto cleanup1;
    // }

    // This should work in case `sudo chmod 666 /dev/ttyS0` was executed.
    FILE *serial_port_ttyS0 = fopen(TTYS0_PATH, "rw");
    if (serial_port_ttyS0 == NULL) {
        printf("failed to open /dev/ttyS0. errno: %d\n", errno);
        return 1;
    }

    uint8_t dont_add_communications_with_host_to_workload = 0;
    while (1) {
        size_t num_of_bytes_read = fread(
            &dont_add_communications_with_host_to_workload, 1, 1, serial_port_ttyS0);

        if (num_of_bytes_read != 1) {
            printf("failed to read script size. ferror: %d, feof: %d, errno: %d\n",
                   ferror(serial_port_ttyS0), feof(serial_port_ttyS0), errno);
            result = 1;
            goto cleanup1;
        }
        // if (dont_add_communications_with_host_to_workload > 1) {
        //     printf("dont_add_communications_with_host_to_workload = %u, "
        //            "but it must be `0` or `1`.\n",
        //            dont_add_communications_with_host_to_workload);
        //     result = 1;
        //     goto cleanup1;
        // }

        printf("dont_add_communications_with_host_to_workload: %u\n",
               dont_add_communications_with_host_to_workload);
        fflush(stdout);
    }

    size_t script_size = 0;
    assert(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__);
    size_t num_of_dwords_read = fread(&script_size, 4, 1, serial_port_ttyS0);
    if (num_of_dwords_read != 1) {
        printf("failed to read script size. ferror: %d, feof: %d, errno: %d\n",
               ferror(serial_port_ttyS0), feof(serial_port_ttyS0), errno);
        result = 1;
        goto cleanup1;
    }

    printf("script_size: %zu\n", script_size);

    uint8_t *script_contents = malloc(script_size);
    if (script_contents == NULL) {
        printf("malloc error\n");
        result = 1;
        goto cleanup1;
    }

    size_t num_of_bytes_read2 = fread(script_contents, script_size, 1, 
                                      serial_port_ttyS0);
    if (num_of_bytes_read2 != 1) {
        printf("failed to read script contents. ferror: %d, feof: %d, errno: %d\n"
               "dont_add_communications_with_host_to_workload: %u\n"
               "script_size: %zu\n, num_of_bytes_read2: %zu\n",
               ferror(serial_port_ttyS0), feof(serial_port_ttyS0), errno,
               dont_add_communications_with_host_to_workload,
               script_size, num_of_bytes_read2);
        result = 1;
        goto cleanup1;
    }

    FILE *script_local_copy = fopen(SCRIPT_LOCAL_COPY_PATH, "wb");
    if (script_local_copy == NULL) {
        printf("failed to open a file for the script's local copy. errno: %d\n",
               errno);
        result = 1;
        goto cleanup1;
    }

    size_t num_of_bytes_written = fwrite(script_contents, script_size, 1, 
                                         script_local_copy);
    if (num_of_bytes_written != 1) {
        printf("failed to write script contents to the local copy. "
               "ferror: %d, feof: %d, errno: %d\n",
               ferror(script_local_copy), feof(script_local_copy), errno);
        result = 1;
        goto cleanup2;
    }

    char cmd_str[300];
    assert(strlen(SCRIPT_LOCAL_COPY_PATH) + strlen(REDIRECT_TO_TTYS0) <
           sizeof(cmd_str));
    if (cmd_str != strcpy(cmd_str, SCRIPT_LOCAL_COPY_PATH)) {
        printf("`strcpy()` failed.\n");
    }
    if (cmd_str != strcat(cmd_str, REDIRECT_TO_TTYS0)) {
        printf("`strcat()` failed.\n");
    }

    if (dont_add_communications_with_host_to_workload) {
        int system_result = system(cmd_str);
        if (system_result != 0) {
            printf("`system()` failed. result code: %d errno: %d\n",
                   system_result, errno);
            result = 1;
            goto cleanup2;
        }
    }
    else {
        PRINT_TO_TTYS0("-----begin workload info-----");
        PRINT_TO_TTYS0("-----end workload info-----");

        PRINT_TO_TTYS0("Ready to trace. Press enter to continue");
        getchar(); /* The host would use 'sendkey' when it is ready. */
        
        int system_result = system(cmd_str);
        if (system_result != 0) {
            printf("`system()` failed. result code: %d errno: %d\n",
                   system_result, errno);
            result = 1;
            goto cleanup2;
        }

        PRINT_TO_TTYS0("Stop tracing");
    }


cleanup2:
    if (fclose(script_local_copy) != 0) {
        printf("failed to close script local copy.\n");
    }
cleanup1:
    if (fclose(serial_port_ttyS0) != 0) {
        printf("failed to close /dev/ttyS0.\n");
    }
    return result;
}

