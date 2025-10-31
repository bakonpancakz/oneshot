#include <stdio.h>

extern const unsigned char _binary_resources_document_help_txt_start[];
extern const unsigned char _binary_resources_document_help_txt_end[];
extern const unsigned char _binary_resources_document_help_txt_size[];

extern const unsigned char _binary_resources_document_yuri_txt_start[];
extern const unsigned char _binary_resources_document_yuri_txt_end[];
extern const unsigned char _binary_resources_document_yuri_txt_size[];

extern const unsigned char _binary_resources_document_errors_txt_start[];
extern const unsigned char _binary_resources_document_errors_txt_end[];
extern const unsigned char _binary_resources_document_errors_txt_size[];

int command_text(const char* res_name, FILE* out) {
    if (!strcmp(res_name, "help")) {
        fwrite(
            _binary_resources_document_help_txt_start,
            _binary_resources_document_help_txt_end - _binary_resources_document_help_txt_start,
            sizeof(unsigned char),
            out ? out : stdout
        );
    }
    else if (!strcmp(res_name, "yuri")) {
        fwrite(
            _binary_resources_document_yuri_txt_start,
            _binary_resources_document_yuri_txt_end - _binary_resources_document_yuri_txt_start,
            sizeof(unsigned char),
            out ? out : stdout
        );
    }
    else if (!strcmp(res_name, "errors")) {
        fwrite(
            _binary_resources_document_errors_txt_start,
            _binary_resources_document_errors_txt_end - _binary_resources_document_errors_txt_start,
            sizeof(unsigned char),
            out ? out : stdout
        );
    }
    else {
        printf("Unknown Document\n");
        return 1;
    }
    return 0;
}
