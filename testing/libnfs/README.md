# Integrations Tests for libnfs-backend

## General
This folder contains integration tests for the libnfs backend of the IOR/MDTEST benchmark. The backend utilizes libnfs, a user-space nfs library, as the nfs client. The tests are written in C with the CMocka test framework (https://cmocka.org/). The tests are designed to ensure that the libnfs library is used correctly and does what is expected of it. In addition, if changes are made to the backend implementation or to the libnfs version used, you can quickly check whether everything still works as expected.

## How to build the test

First you have to install the Cmocka dev library if it is not already installed. Then do the following steps:  

1. create a build folder
2. call ```cmake .. -DLibnfsIncludePath=/path/to/libnfs/include -DLibnfsLibPath=/path/to/libnfs/build/lib``` in the created build folder with the corresponding paths to your libnfs installation/build. Alternatively, the paths can also be adjusted in the CMakeLists.txt file.
3. call ```cmake --build .``` in the created build folder

## How to run the test
The tests are designed to call and test the backend implementations of src/aioiri-LIBNFS.h. This requires a running NFS server. For the tests it is necessary to have local access to the folder that is made available via NFS. There are several ways to do this:
1. The tests are run on the same machine that also provides the NFS server.
2. The NFS server is provided using Docker and local access to the folder is possible using bind mounts.

To provide the test program with the necessary information, it is passed via command line parameters. The first parameter is the absolute path to the shared folder. The second parameter is the NFS URL to this folder, just as you would specify it when running IOR or MDTEST.

Example:  
```
./libnfs_integration_tests "~/libnfs_integration_test" "nfs://127.0.0.1//?uid=0&gid=0&version=4"
```

The Path NFS-path / shows to the folder ~/libnfs_integration_test.  

**Warning:** The used test folder and the NFS-server should only be used for testing purposes and should not be mixed with production data! Data in the test folder will be deleted before every test case.

## How do the tests work
The tests call some functions from the libnfs backend implementation, e.g. create function from the aiori_fd_t structure. It is expected that a file has been created in the folder via nfs. Since the test program also has local access to this folder, it can check without NFS whether a file has been created by using the POSIX interface to verify.
