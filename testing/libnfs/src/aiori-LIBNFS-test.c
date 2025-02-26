#include <stdio.h>
#include <setjmp.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <fcntl.h>
#include <sys/statvfs.h>

#include <cmocka.h>
#include <nfsc/libnfs.h>

#include "../../../src/aiori-LIBNFS.h"
#include "../../../src/aiori.h"


// data structure that contains all necessary information about the test environment
typedef struct
{
    char *local_folder_path; //path to the folder where the tests are run
    libnfs_options_t fake_options; //fake libnfs options -> url for the nfs server which is mapped to the local_folder_path
} test_infrastructure_data;

typedef enum {
    OK,
    GRACE,
    ERROR
} server_state;

/******* helper functions *******/

server_state check_server_state(char *url) {
    server_state result = ERROR;
    struct nfs_context *context = nfs_init_context();
    struct nfs_url *parsed_url = nfs_parse_url_full(context, url);
    
    if (!nfs_mount(context, parsed_url->server, parsed_url->path)) {
        int mode = S_IRUSR;
        struct nfsfh *file;
        int create_result = nfs_open2(context, "test.txt", O_CREAT, mode, &file);
        if (!create_result && file != NULL) {
            nfs_close(context, file);
            result = OK;
        }
        else if (strstr(nfs_get_error(context), "GRACE")) {
            result = GRACE;
        } 
        else {
            printf("nfs error: %s\n", nfs_get_error(context));
        }
    }

    nfs_destroy_url(parsed_url);
    nfs_destroy_context(context);

    return result;
}

void clear_folder(char* folder_path) {    
    char command[PATH_MAX + 10];
    snprintf(command, sizeof(command), "rm -rf %s/*", folder_path);
    system(command);
}

int create_local_file(const char* root, const char* relative_path, const char* content, char *full_path) {
    if ((strlen(root) + strlen(relative_path)) > PATH_MAX) {
        return 1;
    }

    char file_path[PATH_MAX + 1];
    snprintf(file_path, sizeof(file_path), "%s/%s", root, relative_path);
    FILE *file = fopen(file_path, "w+");
    if (file == NULL) {
        return 1;
    }

    if (content) {
        int count = strlen(content);
        int written = fwrite(content, sizeof(char), count, file);
        if (written != count) {
            fclose(file);            
            return 1;
        }
    }

    fclose(file);
    if (full_path)
    {
        memset(full_path, 0, PATH_MAX + 1);
        strcpy(full_path, file_path);
    }    

    return 0;
}

int read_local_file(const char *file_path, char *buffer, size_t length) {
    FILE *file = fopen(file_path, "r");
    if (file == NULL) {
        return 1;
    }

    fread(buffer, sizeof(char), length, file);
    fclose(file);

    return 0;
}

void verify_local_file(const char *file_path, const char *expected_content) {
    struct stat stat_buffer;   
    int stat_result = stat(file_path, &stat_buffer);
    assert_int_equal(stat_result, 0);

    if (!expected_content) {
        assert_int_equal(stat_buffer.st_size, 0);
        return;
    }

    size_t content_length = strlen(expected_content);
    assert_int_equal(stat_buffer.st_size, content_length);
    char *buffer = malloc(content_length + 1);
    memset(buffer, 0, content_length + 1);
    if (read_local_file(file_path, buffer, content_length)) {
        free(buffer);
        fail_msg("read local file failed (file: %s)", file_path);
        return;
    }

    assert_string_equal(buffer, expected_content);
    free(buffer);
}

int local_directory_exists(const char *root, const char *relative_folder_path) {
    char folder_path[PATH_MAX + 1];
    snprintf(folder_path, sizeof(folder_path), "%s/%s", root, relative_folder_path);
    struct stat stat_buffer;       
    int stat_result = stat(folder_path, &stat_buffer);

    return (stat_result == 0 && S_ISDIR(stat_buffer.st_mode)) ? 1 : 0;
}

static int setup(void **state) {    
    test_infrastructure_data *data = (test_infrastructure_data *)*state;

    clear_folder(data->local_folder_path); //ensure all content in the folder is deleted for a fresh new test environment

    libnfs_aiori.initialize((aiori_mod_opt_t*) &data->fake_options);

    return 0;
}

static int teardown(void **state) {
    test_infrastructure_data *data = (test_infrastructure_data *)*state;

    clear_folder(data->local_folder_path);

    libnfs_aiori.finalize((aiori_mod_opt_t*) &data->fake_options);

    return 0;
}

/******* tests *******/

static void create_file(void **state) {
    test_infrastructure_data *data = (test_infrastructure_data *)*state;
    aiori_mod_opt_t *module_options = (aiori_mod_opt_t *)&data->fake_options;

    //Act
    aiori_fd_t *file_handle = libnfs_aiori.create("test.txt", IOR_CREAT, module_options);
    libnfs_aiori.close(file_handle, module_options);

    // Assert
    char file_path[PATH_MAX];
    snprintf(file_path, sizeof(file_path), "%s/test.txt", data->local_folder_path);
    verify_local_file(file_path, NULL);
}

static void create_and_write_file(void **state) {
    test_infrastructure_data *data = (test_infrastructure_data *)*state;
    aiori_mod_opt_t *module_options = (aiori_mod_opt_t *)&data->fake_options;

    //Act
    aiori_fd_t *file_handle = libnfs_aiori.create("test.txt", IOR_CREAT | IOR_WRONLY, module_options);
    const char *buffer = "test";
    int written = libnfs_aiori.xfer(WRITE, file_handle, (IOR_size_t *)buffer, strlen(buffer), 0, module_options);
    libnfs_aiori.close(file_handle, module_options);

    // Assert
    assert_int_equal(written, 4);

    char file_path[PATH_MAX];
    snprintf(file_path, sizeof(file_path), "%s/test.txt", data->local_folder_path);
    verify_local_file(file_path, "test");
}

static void read_file(void **state) {
    test_infrastructure_data *data = (test_infrastructure_data *)*state;
    aiori_mod_opt_t *module_options = (aiori_mod_opt_t *)&data->fake_options;

    //Arrange
    create_local_file(data->local_folder_path, "test.txt", "test", NULL);

    //Act
    char buffer[5] = {};
    aiori_fd_t *file_handle = libnfs_aiori.open("test.txt", IOR_RDONLY, module_options);
    IOR_offset_t readed = libnfs_aiori.xfer(READ, file_handle, (IOR_size_t *)buffer, 4, 0, module_options);
    libnfs_aiori.close(file_handle, module_options);

    //Assert
    assert_int_equal(readed, 4);
    assert_string_equal(buffer, "test");
}

static void read_file_at_specific_offset(void **state) {
    test_infrastructure_data *data = (test_infrastructure_data *)*state;
    aiori_mod_opt_t *module_options = (aiori_mod_opt_t *)&data->fake_options;

    //Arrange
    create_local_file(data->local_folder_path, "test.txt", "test", NULL);

    //Act
    char buffer[3] = {};
    aiori_fd_t *file_handle = libnfs_aiori.open("test.txt", IOR_RDONLY, module_options);
    IOR_offset_t readed = libnfs_aiori.xfer(READ, file_handle, (IOR_size_t *)buffer, 3, 1, module_options);
    libnfs_aiori.close(file_handle, module_options);

    //Assert
    assert_int_equal(readed, 3);
    assert_string_equal(buffer, "est");
}

static void read_and_write_file(void **state) {
    test_infrastructure_data *data = (test_infrastructure_data *)*state;
    aiori_mod_opt_t *module_options = (aiori_mod_opt_t *)&data->fake_options;

    //Arrange
    char file_path[PATH_MAX + 1];
    create_local_file(data->local_folder_path, "test.txt", "test", file_path);

    //Act
    aiori_fd_t *file_handle = libnfs_aiori.open("test.txt", IOR_RDWR, module_options);

    const char *write_content = "abc";
    IOR_offset_t written = libnfs_aiori.xfer(WRITE, file_handle, (IOR_size_t *)write_content, strlen(write_content), 4, module_options);

    char buffer[8] = {};
    IOR_offset_t readed = libnfs_aiori.xfer(READ, file_handle, (IOR_size_t *)buffer, 7, 0, module_options);

    libnfs_aiori.close(file_handle, module_options);

    //Assert
    assert_int_equal(written, strlen(write_content));
    verify_local_file(file_path, "testabc");
    assert_int_equal(readed, 7);
    assert_string_equal(buffer, "testabc");
}

static void write_to_empty_file(void **state) {
    test_infrastructure_data *data = (test_infrastructure_data *)*state;
    aiori_mod_opt_t *module_options = (aiori_mod_opt_t *)&data->fake_options;

    //Arrange
    char file_path[PATH_MAX + 1];
    create_local_file(data->local_folder_path, "test.txt", NULL, file_path);

    //Act
    aiori_fd_t *file_handle = libnfs_aiori.open("test.txt", IOR_WRONLY, module_options);
    const char *buffer = "test";
    int written = libnfs_aiori.xfer(WRITE, file_handle, (IOR_size_t *)buffer, strlen(buffer), 0, module_options);
    libnfs_aiori.close(file_handle, module_options);

    //Assert
    assert_int_equal(written, 4);
    verify_local_file(file_path, "test");
}

static void write_to_file_at_specific_offset(void **state) {
    test_infrastructure_data *data = (test_infrastructure_data *)*state;
    aiori_mod_opt_t *module_options = (aiori_mod_opt_t *)&data->fake_options;

    //Arrange
    char file_path[PATH_MAX + 1];
    create_local_file(data->local_folder_path, "test.txt", "test", file_path);

    //Act
    aiori_fd_t *file_handle = libnfs_aiori.open("test.txt", IOR_WRONLY, module_options);
    const char *buffer = "iltest";
    int written = libnfs_aiori.xfer(WRITE, file_handle, (IOR_size_t *)buffer, strlen(buffer), 2, module_options);
    libnfs_aiori.close(file_handle, module_options);

    //Assert
    assert_int_equal(written, 6);
    verify_local_file(file_path, "teiltest");
}

static void append_file(void **state) {
    test_infrastructure_data *data = (test_infrastructure_data *)*state;
    aiori_mod_opt_t *module_options = (aiori_mod_opt_t *)&data->fake_options;

    //Arrange
    char file_path[PATH_MAX + 1];
    create_local_file(data->local_folder_path, "test.txt", "test", file_path);

    //Act
    aiori_fd_t *file_handle = libnfs_aiori.open("test.txt", IOR_WRONLY | IOR_APPEND, module_options);
    const char *buffer = "test";
    int written = libnfs_aiori.xfer(WRITE, file_handle, (IOR_size_t *)buffer, strlen(buffer), 1, module_options);
    libnfs_aiori.close(file_handle, module_options);

    //Assert
    assert_int_equal(written, 4);
    verify_local_file(file_path, "testtest");
}

static void truncate_file(void **state) {
    test_infrastructure_data *data = (test_infrastructure_data *)*state;
    aiori_mod_opt_t *module_options = (aiori_mod_opt_t *)&data->fake_options;

    //Arrange
    char file_path[PATH_MAX + 1];
    create_local_file(data->local_folder_path, "test.txt", "test", file_path);

    //Act
    aiori_fd_t *file_handle = libnfs_aiori.open("test.txt", IOR_WRONLY | IOR_TRUNC, module_options);
    const char *buffer = "a";
    int written = libnfs_aiori.xfer(WRITE, file_handle, (IOR_size_t *)buffer, strlen(buffer), 0, module_options);
    libnfs_aiori.close(file_handle, module_options);

    //Assert
    assert_int_equal(written, 1);
    verify_local_file(file_path, "a");
}

static void remove_file(void **state) {
    test_infrastructure_data *data = (test_infrastructure_data *)*state;
    aiori_mod_opt_t *module_options = (aiori_mod_opt_t *)&data->fake_options;

    //Arrange
    char file_path[PATH_MAX + 1];
    create_local_file(data->local_folder_path, "test.txt", "test", file_path);

    //Act
    libnfs_aiori.remove("test.txt", module_options);

    //Assert
    struct stat stat_buffer;   
    int stat_result = stat(file_path, &stat_buffer);
    assert_int_not_equal(stat_result, 0);
}

static void make_directory(void **state) {
    test_infrastructure_data *data = (test_infrastructure_data *)*state;
    aiori_mod_opt_t *module_options = (aiori_mod_opt_t *)&data->fake_options;

    //Act
    int result = libnfs_aiori.mkdir("test_folder", 0777, module_options);

    //Assert
    assert_int_equal(result, 0);
    assert_true(local_directory_exists(data->local_folder_path, "test_folder"));
}

static void remove_directory(void **state) {
    test_infrastructure_data *data = (test_infrastructure_data *)*state;
    aiori_mod_opt_t *module_options = (aiori_mod_opt_t *)&data->fake_options;

    //Arrange
    char folder_path[PATH_MAX + 1];
    snprintf(folder_path, sizeof(folder_path), "%s/%s", data->local_folder_path, "test_folder");
    int mkdir_result = mkdir(folder_path, 0777);
    if (mkdir_result) {
        fail_msg("failed to create a local folder at path %s", folder_path);
        return;
    }

    //Act
    int result = libnfs_aiori.rmdir("test_folder", module_options);

    //Assert
    assert_int_equal(result, 0);
    assert_false(local_directory_exists(data->local_folder_path, "test_folder"));
}

static void get_file_size(void **state) {
    test_infrastructure_data *data = (test_infrastructure_data *)*state;
    aiori_mod_opt_t *module_options = (aiori_mod_opt_t *)&data->fake_options;

    //Arrange
    create_local_file(data->local_folder_path, "test.txt", "test", NULL);

    //Act
    IOR_offset_t file_size = libnfs_aiori.get_file_size(module_options, "test.txt");

    //Assert
    assert_int_equal(file_size, 4);
}

/******* main *******/

int main(int argc, char** argv) {
    if (argc != 3)
    {
        printf("invalid parameter: the first parameter must be a path to a local folder and the second must be a valid nfs url that points to the local folder through the nfs_server\n");
        return 1;
    }

    libnfs_options_t fake_options;
    fake_options.url = argv[2];

    test_infrastructure_data data;
    data.local_folder_path = argv[1];
    data.fake_options = fake_options;
    
    void *state = (void *) &data;

    clear_folder(data.local_folder_path);
    //max. 5 attempts for connecting to the server
    int is_available = 0; 
    for (int i = 0; i < 5; i++) {
        server_state state = check_server_state(data.fake_options.url);
        if (state == OK) {
            is_available = 1;
            break;
        }

        if (state == GRACE) {
            printf("nfs server is in grace mode... waiting until the server is available...\n");
            i--;
            sleep(20);
            continue;
        }

        printf("the nfs server is not available (connect attempt %d/5)... retrying in 2 seconds...\n", i + 1);
        sleep(2);
    }

    if (!is_available)
    {
        printf("the nfs server is not available under %s\n", argv[2]);
        return 1;
    }    

    printf("nfs server is available and url is correct... starting the integration tests...\n");
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_prestate_setup_teardown(create_file, setup, teardown, state),
        cmocka_unit_test_prestate_setup_teardown(create_and_write_file, setup, teardown, state),
        cmocka_unit_test_prestate_setup_teardown(read_file, setup, teardown, state),
        cmocka_unit_test_prestate_setup_teardown(read_file_at_specific_offset, setup, teardown, state),
        cmocka_unit_test_prestate_setup_teardown(read_and_write_file, setup, teardown, state),
        cmocka_unit_test_prestate_setup_teardown(write_to_empty_file, setup, teardown, state),
        cmocka_unit_test_prestate_setup_teardown(write_to_file_at_specific_offset, setup, teardown, state),        
        cmocka_unit_test_prestate_setup_teardown(append_file, setup, teardown, state),                
        cmocka_unit_test_prestate_setup_teardown(truncate_file, setup, teardown, state),                
        cmocka_unit_test_prestate_setup_teardown(remove_file, setup, teardown, state),     
        cmocka_unit_test_prestate_setup_teardown(make_directory, setup, teardown, state),
        cmocka_unit_test_prestate_setup_teardown(remove_directory, setup, teardown, state),
        cmocka_unit_test_prestate_setup_teardown(get_file_size, setup, teardown, state),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
