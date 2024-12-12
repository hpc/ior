#include <stdio.h>
#include <setjmp.h>
#include <cmocka.h>
#include <stdio.h>
#include <limits.h>

#include <fcntl.h>
#include <nfsc/libnfs.h>

#include "../../../src/aiori-LIBNFS.h"
#include "../../../src/aiori.h"


// data structure that contains all necessary information about the test environment
typedef struct
{
    char *local_folder_path; //path to the folder where the tests are run
    libnfs_options_t fake_options; //fake libnfs options -> url for the nfs server which is mapped to the local_folder_path
} test_infrastructure_data;

/******* helper functions *******/

int is_nfs_server_available(char *url) {
    int result = 0;
    struct nfs_context *context = nfs_init_context();
    struct nfs_url *parsed_url = nfs_parse_url_full(context, url);
    
    if (!nfs_mount(context, parsed_url->server, parsed_url->path)) {
        result = 1;
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

int create_file_local(const char* root, const char* relative_path, const char* content, char *full_path) {
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
    struct stat stat_buffer;
    assert_int_equal(stat(file_path, &stat_buffer), 0);
    assert_int_equal(stat_buffer.st_size, 0);
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
    char file_path[PATH_MAX];
    snprintf(file_path, sizeof(file_path), "%s/test.txt", data->local_folder_path);
    struct stat stat_buffer;
    assert_int_equal(stat(file_path, &stat_buffer), 0);
    assert_int_equal(written, 4);
    assert_int_equal(stat_buffer.st_size, 4);
}

static void open_and_read_file(void **state) {
    test_infrastructure_data *data = (test_infrastructure_data *)*state;
    aiori_mod_opt_t *module_options = (aiori_mod_opt_t *)&data->fake_options;

    //Arrange
    create_file_local(data->local_folder_path, "test.txt", "test", NULL);

    //Act
    char buffer[5] = {};
    aiori_fd_t *file_handle = libnfs_aiori.open("test.txt", IOR_RDONLY, module_options);
    IOR_offset_t readed = libnfs_aiori.xfer(READ, file_handle, (IOR_size_t *)buffer, 4, 0, module_options);
    libnfs_aiori.close(file_handle, module_options);

    //Assert
    assert_int_equal(readed, 4);
}

static void open_and_write_file(void **state) {
    test_infrastructure_data *data = (test_infrastructure_data *)*state;
    aiori_mod_opt_t *module_options = (aiori_mod_opt_t *)&data->fake_options;

    //Arrange
    char file_path[PATH_MAX + 1];
    create_file_local(data->local_folder_path, "test.txt", "test", file_path);

    //Act
    aiori_fd_t *file_handle = libnfs_aiori.open("test.txt", IOR_WRONLY, module_options);
    const char *buffer = "test";
    int written = libnfs_aiori.xfer(WRITE, file_handle, (IOR_size_t *)buffer, strlen(buffer), 0, module_options);
    libnfs_aiori.close(file_handle, module_options);

    //Assert
    struct stat stat_buffer;
    assert_int_equal(stat(file_path, &stat_buffer), 0);
    assert_int_equal(written, 4);
    assert_int_equal(stat_buffer.st_size, 4);
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

    //max. 5 attempts for connecting to the server
    int is_available = 0; 
    for (int i = 0; i < 5; i++) {
        if (is_nfs_server_available(argv[2]))
        {
            is_available = 1;
            break;
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
        cmocka_unit_test_prestate_setup_teardown(open_and_read_file, setup, teardown, state),
        cmocka_unit_test_prestate_setup_teardown(open_and_write_file, setup, teardown, state),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
