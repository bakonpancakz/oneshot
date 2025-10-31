#include <stdlib.h>
#include <command_package.h>
#include <command_text.h>
#include <command_list.h>

int main(int argc, char** argv) {

    // Print the contents of a YURI Archive
    if (argc >= 3 && !strcmp(argv[1], "list")) {
        return command_list(argv[2]);
    }

    // Print Document into the Console or write to a File
    if (argc >= 3 && !strcmp(argv[1], "docs")) {
        FILE* w = NULL;
        if (argc >= 4) {
            if ((w = fopen(argv[3], "wb")) == NULL) {
                printf("Cannot Open File: %s", strerror(errno));
                return 1;
            }
        }
        return command_text(argv[2], w);
    }

    // Convert a directory into a YURI Archive
    if (argc >= 4 && !strcmp(argv[1], "package")) {
        return command_package(argv[2], argv[3]);
    }

    // Convert a YURI Archive into a directory
    if (argc >= 4 && !strcmp(argv[1], "extract")) {

        printf("This function has not yet been implemented! :3\n");
        printf("* Argument #1 : '%s'\n", argv[2]);
        printf("* Argument #2 : '%s'\n", argv[3]);

        return 2;
    }

    // Print Standard Help Menu
    return command_text("help", NULL);
}