#include "tecmo_gameplay_cpu_opcode_workspaces.h"

#include <stdio.h>

int main(void)
{
    char message[256];
    if (!tecmo_gameplay_cpu_opcode_workspace_self_test(
            message, sizeof(message))) {
        printf("CPU opcode workspace harness failed: %s\n", message);
        return 1;
    }
    printf("%s\n", message);
    return 0;
}
