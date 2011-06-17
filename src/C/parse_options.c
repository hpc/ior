/******************************************************************************\
*                                                                              *
*        Copyright (c) 2003, The Regents of the University of California       *
*      See the file COPYRIGHT for a complete copyright notice and license.     *
*                                                                              *
********************************************************************************
*
* CVS info:
*   $RCSfile: parse_options.c,v $
*   $Revision: 1.3 $
*   $Date: 2008/12/02 17:12:14 $
*   $Author: rklundt $
*
* Purpose:
*       Parse commandline functions.
*
\******************************************************************************/
/********************* Modifications to IOR-2.10.1 *****************************
* hodson - 8/18/2008: Added parsing for  the following variables               *
*    int TestNum;                    * test reference number                   *
*    int taskPerNodeOffset;          * task node offset for reading files      *
*    int reorderTasksRandom;         * reorder tasks for random file read back *
*    int reorderTasksRandomSeed;     * reorder tasks for random file read seed *
*    int fsyncPerWrite;              * fsync() after each write                *
*******************************************************************************/

#include "IOR.h"
#include "defaults.h"                                 /* IOR defaults */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

IOR_param_t initialTestParams;

/******************************************************************************/
/*
 * Check and correct all settings of each test in queue for correctness.
 */

void CheckRunSettings(IOR_queue_t *tests) {
    while (tests != NULL) {
        /* If no write/read/check action requested, set both write and read */
        if (tests->testParameters.writeFile == FALSE
            && tests->testParameters.readFile == FALSE
            && tests->testParameters.checkWrite == FALSE
            && tests->testParameters.checkRead == FALSE) {
            tests->testParameters.readFile = TRUE;
            tests->testParameters.writeFile = TRUE;
        }
        /* If numTasks set to 0, use all tasks */
        if (tests->testParameters.numTasks == 0) {
            MPI_CHECK(MPI_Comm_size(MPI_COMM_WORLD,
                                    &tests->testParameters.numTasks),
                      "MPI_Comm_size() error");
        }
        tests = tests->nextTest;
    }
} /* CheckRunSettings() */


/******************************************************************************/
/*
 * Set flags from commandline string/value pairs.
 */

void DecodeDirective(char *line, IOR_param_t *test)
{
    char option[MAX_STR];
    char value[MAX_STR];
    int rc;

    rc = sscanf(line, " %[^=# \t\r\n] = %[^# \t\r\n] ", option, value);
    if (rc != 2 && rank == 0) {
        fprintf(stdout, "Syntax error in configuration options: %s\n", line);
        MPI_CHECK(MPI_Abort(MPI_COMM_WORLD, -1), "MPI_Abort() error");
    }
    if (strcasecmp(option, "api") == 0) {
        strcpy(test->api, value);
    } else if (strcasecmp(option, "testnum") == 0) {
        test->TestNum = atoi(value);
    } else if (strcasecmp(option, "debug") == 0) {
        strcpy(test->debug, value);
    } else if (strcasecmp(option, "platform") == 0) {
        strcpy(test->platform, value);
    } else if (strcasecmp(option, "testfile") == 0) {
        strcpy(test->testFileName, value);
    } else if (strcasecmp(option, "hintsfilename") == 0) {
        strcpy(test->hintsFileName, value);
    } else if (strcasecmp(option, "deadlineforstonewalling") == 0) {
        test->deadlineForStonewalling = atoi(value);
    } else if (strcasecmp(option, "maxtimeduration") == 0) {
        test->maxTimeDuration = atoi(value);
    } else if (strcasecmp(option, "outlierthreshold") == 0) {
        test->outlierThreshold = atoi(value);
    } else if (strcasecmp(option, "nodes") == 0) {
        test->nodes = atoi(value);
    } else if (strcasecmp(option, "repetitions") == 0) {
        test->repetitions = atoi(value);
    } else if (strcasecmp(option, "intertestdelay") == 0) {
        test->interTestDelay = atoi(value);
    } else if (strcasecmp(option, "readfile") == 0) {
        test->readFile = atoi(value);
    } else if (strcasecmp(option, "writefile") == 0) {
        test->writeFile = atoi(value);
    } else if (strcasecmp(option, "fileperproc") == 0) {
        test->filePerProc = atoi(value);
    } else if (strcasecmp(option, "reordertasksconstant") == 0) {
        test->reorderTasks = atoi(value);
    } else if (strcasecmp(option, "taskpernodeoffset") == 0) {
        test->taskPerNodeOffset = atoi(value);
    } else if (strcasecmp(option, "reordertasksrandom") == 0) {
        test->reorderTasksRandom = atoi(value);
    } else if (strcasecmp(option, "reordertasksrandomSeed") == 0) {
        test->reorderTasksRandomSeed = atoi(value);
    } else if (strcasecmp(option, "checkwrite") == 0) {
        test->checkWrite = atoi(value);
    } else if (strcasecmp(option, "checkread") == 0) {
        test->checkRead = atoi(value);
    } else if (strcasecmp(option, "keepfile") == 0) {
        test->keepFile = atoi(value);
    } else if (strcasecmp(option, "keepfilewitherror") == 0) {
        test->keepFileWithError = atoi(value);
    } else if (strcasecmp(option, "multiFile") == 0) {
        test->multiFile = atoi(value);
    } else if (strcasecmp(option, "quitonerror") == 0) {
        test->quitOnError = atoi(value);
    } else if (strcasecmp(option, "segmentcount") == 0) {
        test->segmentCount = StringToBytes(value);
    } else if (strcasecmp(option, "blocksize") == 0) {
        test->blockSize = StringToBytes(value);
    } else if (strcasecmp(option, "transfersize") == 0) {
        test->transferSize = StringToBytes(value);
    } else if (strcasecmp(option, "setalignment") == 0) {
        test->setAlignment = StringToBytes(value);
    } else if (strcasecmp(option, "singlexferattempt") == 0) {
        test->singleXferAttempt = atoi(value);
    } else if (strcasecmp(option, "individualdatasets") == 0) {
        test->individualDataSets = atoi(value);
    } else if (strcasecmp(option, "intraTestBarriers") == 0) {
        test->intraTestBarriers = atoi(value);
    } else if (strcasecmp(option, "nofill") == 0) {
        test->noFill = atoi(value);
    } else if (strcasecmp(option, "verbose") == 0) {
        test->verbose = atoi(value);
    } else if (strcasecmp(option, "settimestampsignature") == 0) {
        test->setTimeStampSignature = atoi(value);
    } else if (strcasecmp(option, "collective") == 0) {
        test->collective = atoi(value);
    } else if (strcasecmp(option, "preallocate") == 0) {
        test->preallocate = atoi(value);
    } else if (strcasecmp(option, "storefileoffset") == 0) {
        test->storeFileOffset = atoi(value);
    } else if (strcasecmp(option, "usefileview") == 0) {
        test->useFileView = atoi(value);
    } else if (strcasecmp(option, "usesharedfilepointer") == 0) {
        test->useSharedFilePointer = atoi(value);
    } else if (strcasecmp(option, "useo_direct") == 0) {
        test->useO_DIRECT = atoi(value);
    } else if (strcasecmp(option, "usestrideddatatype") == 0) {
        test->useStridedDatatype = atoi(value);
    } else if (strcasecmp(option, "showhints") == 0) {
        test->showHints = atoi(value);
    } else if (strcasecmp(option, "showhelp") == 0) {
        test->showHelp = atoi(value);
    } else if (strcasecmp(option, "uniqueDir") == 0) {
        test->uniqueDir = atoi(value);
    } else if (strcasecmp(option, "useexistingtestfile") == 0) {
        test->useExistingTestFile = atoi(value);
    } else if (strcasecmp(option, "fsyncperwrite") == 0) {
        test->fsyncPerWrite = atoi(value);
    } else if (strcasecmp(option, "fsync") == 0) {
        test->fsync = atoi(value);
    } else if (strcasecmp(option, "randomoffset") == 0) {
        test->randomOffset = atoi(value);
    } else if (strcasecmp(option, "lustrestripecount") == 0) {
        test->lustre_stripe_count = atoi(value);
    } else if (strcasecmp(option, "lustrestripesize") == 0) {
        test->lustre_stripe_size = StringToBytes(value);
    } else if (strcasecmp(option, "lustrestartost") == 0) {
        test->lustre_start_ost = atoi(value);
    } else if (strcasecmp(option, "lustreignorelocks") == 0) {
        test->lustre_ignore_locks = atoi(value);
#if USE_UNDOC_OPT
    } else if (strcasecmp(option, "corruptFile") == 0) {
        test->corruptFile = atoi(value);
    } else if (strcasecmp(option, "fillTheFileSystem") == 0) {
        test->fillTheFileSystem = atoi(value);
    } else if (strcasecmp(option, "includeDeleteTime") == 0) {
        test->includeDeleteTime = atoi(value);
    } else if (strcasecmp(option, "multiReRead") == 0) {
        test->multiReRead = atoi(value);
    } else if (strcasecmp(option, "nfs_rootpath") == 0) {
        strcpy(test->NFS_rootPath, value);
    } else if (strcasecmp(option, "nfs_servername") == 0) {
        strcpy(test->NFS_serverName, value);
    } else if (strcasecmp(option, "nfs_servercount") == 0) {
        test->NFS_serverCount = atoi(value);
#endif /* USE_UNDOC_OPT */
    } else if (strcasecmp(option, "numtasks") == 0) {
        test->numTasks = atoi(value);
    } else {
        if (rank == 0)
            fprintf(stdout, "Unrecognized parameter \"%s\"\n", option);
        MPI_CHECK(MPI_Abort(MPI_COMM_WORLD, -1), "MPI_Abort() error");
    }
} /* DecodeDirective() */


/******************************************************************************/
/*
 * Parse a single line, which may contain multiple comma-seperated directives
 */

void
ParseLine(char *line, IOR_param_t *test)
{
    char *start, *end;

    start = line;
    do {
        end = strchr(start, ',');
        if (end != NULL)
            *end = '\0';
        DecodeDirective(start, test);
        start = end + 1;
    } while (end != NULL);

} /* ParseLine() */


/******************************************************************************/
/*
 * Determines if the string "haystack" contains only the string "needle", and
 * possibly whitespace before and after needle.  Function is case insensitive.
 */

int
contains_only(char *haystack, char *needle)
{
    char *ptr, *end;

    end = haystack + strlen(haystack);
    /* skip over leading shitspace */
    for (ptr = haystack; ptr < end; ptr++) {
        if (!isspace(*ptr))
            break;
    }
    /* check for "needle" */
    if (strncasecmp(ptr, needle, strlen(needle)) != 0)
        return 0;
    /* make sure the rest of the line is only whitspace as well */
    for (ptr += strlen(needle); ptr < end; ptr++) {
        if (!isspace(*ptr))
            return 0;
    }

    return 1;
} /* contains_only() */


/******************************************************************************/
/*
 * Read the configuration script, allocating and filling in the structure of
 * global parameters.
 */

IOR_queue_t *
ReadConfigScript(char * scriptName)
{
    int          test_num = 0;
    int          runflag = 0;
    char         linebuf[MAX_STR];
    char         empty[MAX_STR];
    FILE        *file;
    IOR_queue_t *head    = NULL;
    IOR_queue_t *tail    = NULL;
    IOR_queue_t *newTest = NULL;

    /* Initialize the first test */
    head = CreateNewTest(test_num++);
    tail = head;

    /* open the script */
    file = fopen(scriptName, "r");
    if (file == NULL)
        ERR("cannot open file");

    /* search for the "IOR START" line */
    while(fgets(linebuf, MAX_STR, file) != NULL) {
        if (contains_only(linebuf, "ior start")) {
            break;
        }
    }

    /* Iterate over a block of IOR commands */
    while(fgets(linebuf, MAX_STR, file) != NULL) {
        /* skip empty lines */
        if (sscanf(linebuf, "%s", empty) == -1)
            continue;
        /* skip lines containing only comments */
        if (sscanf(linebuf, " #%s", empty) == 1)
            continue;
        if (contains_only(linebuf, "ior stop")) {
            break;
        } else if (contains_only(linebuf, "run")) {
            runflag = 1;
        } else {
            /* If this directive was preceded by a "run" line, then
               create and initialize a new test structure */
            if (runflag) {
                newTest = (IOR_queue_t *)malloc(sizeof(IOR_queue_t));
                if (newTest == NULL)
                    ERR("malloc failed");
                newTest->testParameters = tail->testParameters;
                newTest->testParameters.id = test_num++;
                tail->nextTest = newTest;
                tail = newTest;
                tail->nextTest = NULL;
                runflag = 0;
            }
            ParseLine(linebuf, &tail->testParameters);
        }
    }

    /* close the script */
    if (fclose(file) != 0)
        ERR("cannot close script file");

    return(head);
} /* ReadConfigScript() */


/******************************************************************************/
/*
 * Parse Commandline.
 */

IOR_queue_t *
ParseCommandLine(int argc, char ** argv)
{
    static const char * opts
        = "A:a:b:BcCQ:ZX:d:D:YeEf:FgG:hHi:j:J:IkKlmnN:o:O:pPqrRs:St:T:uU:vVwWxz";
    int                 c, i;
    static IOR_queue_t *tests = NULL;

    /* suppress getopt() error message when a character is unrecognized */
    opterr = 0;

    initialTestParams = defaultParameters;
    GetPlatformName(initialTestParams.platform);
    initialTestParams.writeFile  = initialTestParams.readFile  = FALSE;
    initialTestParams.checkWrite = initialTestParams.checkRead = FALSE;

    while ((c = getopt(argc, argv, opts)) != -1) {
        switch (c) {
        case 'A': initialTestParams.TestNum = atoi(optarg);        break;
        case 'a': strcpy(initialTestParams.api, optarg);           break;
        case 'b': initialTestParams.blockSize =
                      StringToBytes(optarg);                       break;
        case 'B': initialTestParams.useO_DIRECT = TRUE;            break;
        case 'c': initialTestParams.collective = TRUE;             break;
        case 'C': initialTestParams.reorderTasks = TRUE;           break;
        case 'Q': initialTestParams.taskPerNodeOffset = 
                                                 atoi(optarg);     break;
        case 'Z': initialTestParams.reorderTasksRandom = TRUE;     break;
        case 'X': initialTestParams.reorderTasksRandomSeed = 
                                                 atoi(optarg);     break;
        case 'd': initialTestParams.interTestDelay = atoi(optarg); break;
        case 'D': initialTestParams.deadlineForStonewalling =
                      atoi(optarg);                                break;
        case 'Y': initialTestParams.fsyncPerWrite = TRUE;          break;
        case 'e': initialTestParams.fsync = TRUE;                  break;
        case 'E': initialTestParams.useExistingTestFile = TRUE;    break;
        case 'f': tests = ReadConfigScript(optarg);                break;
        case 'F': initialTestParams.filePerProc = TRUE;            break;
        case 'g': initialTestParams.intraTestBarriers = TRUE;      break;
        case 'G': initialTestParams.setTimeStampSignature =
                      atoi(optarg);                                break;
        case 'h': initialTestParams.showHelp = TRUE;               break;
        case 'H': initialTestParams.showHints = TRUE;              break;
        case 'i': initialTestParams.repetitions = atoi(optarg);    break;
        case 'I': initialTestParams.individualDataSets = TRUE;     break;
        case 'j': initialTestParams.outlierThreshold =
                      atoi(optarg);                                break;
        case 'J': initialTestParams.setAlignment =
                      StringToBytes(optarg);                       break;
        case 'k': initialTestParams.keepFile = TRUE;               break;
        case 'K': initialTestParams.keepFileWithError = TRUE;      break;
        case 'l': initialTestParams.storeFileOffset = TRUE;        break;
        case 'm': initialTestParams.multiFile = TRUE;              break;
        case 'n': initialTestParams.noFill = TRUE;                 break;
        case 'N': initialTestParams.numTasks = atoi(optarg);       break;
        case 'o': strcpy(initialTestParams.testFileName, optarg);  break;
        case 'O': ParseLine(optarg, &initialTestParams);           break;
        case 'p': initialTestParams.preallocate = TRUE;            break;
        case 'P': initialTestParams.useSharedFilePointer = TRUE;   break;
        case 'q': initialTestParams.quitOnError = TRUE;            break;
        case 'r': initialTestParams.readFile = TRUE;               break;
        case 'R': initialTestParams.checkRead = TRUE;              break;
        case 's': initialTestParams.segmentCount = atoi(optarg);   break;
        case 'S': initialTestParams.useStridedDatatype = TRUE;     break;
        case 't': initialTestParams.transferSize =
                      StringToBytes(optarg);                       break;
        case 'T': initialTestParams.maxTimeDuration = atoi(optarg);break;
        case 'u': initialTestParams.uniqueDir = TRUE;              break;
        case 'U': strcpy(initialTestParams.hintsFileName, optarg); break;
        case 'v': initialTestParams.verbose++;                     break;
        case 'V': initialTestParams.useFileView = TRUE;            break;
        case 'w': initialTestParams.writeFile = TRUE;              break;
        case 'W': initialTestParams.checkWrite = TRUE;             break;
        case 'x': initialTestParams.singleXferAttempt = TRUE;      break;
        case 'z': initialTestParams.randomOffset = TRUE;           break;
        default:  fprintf (stdout, "ParseCommandLine: unknown option `-%c'.\n", optopt);
        }
    }

    for (i = optind; i < argc; i++)
        fprintf (stdout, "non-option argument: %s\n", argv[i]);

    /* If an IOR script was not used, initialize test queue to the defaults */
    if (tests == NULL) {
        tests = (IOR_queue_t *) malloc (sizeof(IOR_queue_t));
        if (!tests)
            ERR("malloc failed");
        tests->testParameters = initialTestParams;
        tests->nextTest = NULL;
    }

    CheckRunSettings(tests);

    return(tests);
} /* ParseCommandLine() */
