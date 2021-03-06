/*
  +----------------------------------------------------------------------+
  | Zan                                                                  |
  +----------------------------------------------------------------------+
  | Copyright (c) 2016-2017 Zan Group <https://github.com/youzan/zan>    |
  +----------------------------------------------------------------------+
  | This source file is subject to version 2.0 of the Apache license,    |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.apache.org/licenses/LICENSE-2.0.html                      |
  | If you did not receive a copy of the Apache2.0 license and are unable|
  | to obtain it through the world-wide-web, please send a note to       |
  | zan@zanphp.io so we can mail you a copy immediately.                 |
  +----------------------------------------------------------------------+
  | Author: Zan Group   <zan@zanphp.io>                                  |
  +----------------------------------------------------------------------+
*/
#ifndef _ZAN_WORKERS_H_
#define _ZAN_WORKERS_H_

#include "swBaseOperator.h"
#include "zanIpc.h"
#include "zanFactory.h"

#ifdef __cplusplus
extern "C" {
#endif

enum zanWorker_status
{
    ZAN_WORKER_BUSY = 1,
    ZAN_WORKER_IDLE = 2,
    ZAN_WORKER_DEL  = 3,
};

enum zanResponseType
{
    ZAN_RESPONSE_SMALL = 0,
    ZAN_RESPONSE_BIG   = 1,
};

enum zanTaskType
{
    ZAN_TASK_TMPFILE    = 1,
    ZAN_TASK_SERIALIZE  = 2,
    ZAN_TASK_NONBLOCK   = 4,
};

enum zanProcessType
{
    ZAN_PROCESS_UNKNOWN    = 0,
    ZAN_PROCESS_MASTER     = 1,
    ZAN_PROCESS_WORKER     = 2,
    ZAN_PROCESS_TASKWORKER = 3,
    ZAN_PROCESS_USERWORKER = 4,
    ZAN_PROCESS_NETWORKER  = 5,
};

#define is_unknown()    (ServerG.process_type == ZAN_PROCESS_UNKNOWN)
#define is_master()     (ServerG.process_type == ZAN_PROCESS_MASTER)
#define is_networker()  (ServerG.process_type == ZAN_PROCESS_NETWORKER)
#define is_worker()     (ServerG.process_type == ZAN_PROCESS_WORKER)
#define is_taskworker() (ServerG.process_type == ZAN_PROCESS_TASKWORKER)
#define is_userworker() (ServerG.process_type == ZAN_PROCESS_USERWORKER)

typedef struct _zanProcessPool zanProcessPool;

typedef struct _zanWorker
{
    uint8_t   process_type;
    zan_pid_t worker_pid;
    uint32_t  worker_id;

    uint8_t   redirect_stdout;       //redirect stdout to pipe_master
    uint8_t   redirect_stdin;        //redirect stdin to pipe_worker
    uint8_t   redirect_stderr;       //redirect stderr to pipe_worker

    //worker status, IDLE or BUSY
    uint8_t   status;
    uint8_t   deleted;
    uint8_t   deny_request;
    uint8_t   child_process;

    //worker
    uint32_t  request_num;

    zanLock lock;

    uint8_t ipc_mode;
    zanMsgQueue *queue;

    int pipe;
    int pipe_master;
    int pipe_worker;

    void *send_shm;

    zanPipe        *pipe_object;
    zanProcessPool *pool;

    void *ptr;
} zanWorker;

typedef struct _zanUserWorker_node
{
    struct _zanUserWorker_node *next, *prev;
    zanWorker *worker;
} zanUserWorker_node;

int zanWorker_init(zanWorker *worker);
void zanWorker_free(zanWorker *worker);
zan_pid_t zanMaster_spawnworker(zanProcessPool *pool, zanWorker *worker);
void zan_worker_shutdown(zanProcessPool *pool);
void zanWorker_clean(void);

//networker<--->worker<--->task_worker
int zanWorker_send2worker(zanWorker *dst_worker, void *buf, int n, int flag);
int zanWorker_send2networker(swEventData *ev_data, size_t sendn, int fd);

int zanNetworker_send2worker(void *data, int len, uint16_t target_worker_id);
int zanNetworker_close_connection(swReactor *reactor, int fd);
zan_pid_t zanNetWorker_spawn(zanWorker *worker);
void zan_networker_shutdown(zanProcessPool *pool);

int zanNetworker_onClose(swReactor *reactor, swEvent *event);

int zanTaskworker_finish(char *data, int data_len, int flags);

zan_pid_t zanTaskWorker_spawn(zanWorker *worker);
int zanTaskWorker_largepack(swEventData *task, void *data, int data_len);

int zanReactor_onAccept(swReactor *reactor, swEvent *event);
void zanReactor_enableAccept(swReactor *reactor);

////////////////////////////////////////////////////////////////////////////////
//worker pool
struct _zanProcessPool
{
    uint16_t     start_id;         //worker->id = start_id + index
    sw_atomic_t round_id;

    //taskworker
    zanMsgQueue *queue;
    swHashMap   *map;

    zanWorker   *workers;
    zanPipe     *pipes;

    void (*onWorkerStart)(struct _zanProcessPool *pool, zanWorker *worker);
    void (*onWorkerStop)(struct _zanProcessPool *pool, zanWorker *worker);

    int (*onTask)(struct _zanProcessPool *pool, swEventData *task);
    int (*main_loop)(struct _zanProcessPool *pool, zanWorker *worker);
};

//create and start child workers
int zan_start_worker_processes(void);

/*----------------------------Process Pool-------------------------------*/
int zan_processpool_create(zanProcessPool *pool, int process_type);
void zan_processpool_shutdown(zanProcessPool *pool);

//
void zan_stats_set_worker_status(zanWorker *worker, int status);
void zanWorker_clean_pipe(void);

int zanPool_dispatch_to_taskworker(zanProcessPool *pool, swEventData *data, int *dst_worker_id);

static inline zanWorker* zan_pool_get_worker(zanProcessPool *pool, int worker_id)
{
    return &(pool->workers[worker_id - pool->start_id]);
}

swString *zanWorker_get_buffer(int from_id);

/************************************************************************/

typedef struct _swPackage
{
    void *data;
    uint32_t length;
    uint32_t id;
} swPackage;

typedef struct _swDgramPacket
{
    union
    {
        struct in6_addr v6;
        struct in_addr v4;
        struct
        {
            uint16_t path_length;
        } un;
    } addr;
    uint16_t port;
    uint32_t length;
    char data[0];
} swDgramPacket;

typedef struct
{
    int length;
    char tmpfile[SW_TASK_TMPDIR_SIZE + sizeof(SW_TASK_TMP_FILE)];
} swPackage_task;

typedef struct
{
    int length;
    int worker_id;
} swPackage_response;


int swTaskWorker_large_pack(swEventData *task, void *data, int data_len);

#define swTask_type(task)                  ((task)->info.from_fd)

#define swTaskWorker_large_unpack(task, __malloc, _buf, _length)   swPackage_task _pkg;\
memcpy(&_pkg, task->data, sizeof(_pkg));\
_length = _pkg.length;\
    if (_length > ServerG.serv->listen_list->protocol.package_max_length) {\
    zanWarn("task package[length=%d] is too big.", _length);\
}\
_buf = __malloc(_length + 1);\
_buf[_length] = 0;\
int tmp_file_fd = open(_pkg.tmpfile, O_RDONLY);\
    if (tmp_file_fd < 0){\
    zanError("open(%s) failed.", task->data);\
    _length = -1;\
    } else if (swoole_sync_readfile(tmp_file_fd, _buf, _length) > 0) {\
    close(tmp_file_fd);\
    unlink(_pkg.tmpfile);\
    } else {\
    _length = -1;\
    close(tmp_file_fd); \
    unlink(_pkg.tmpfile); \
}


#ifdef __cplusplus
}
#endif

#endif  //_ZAN_WORKERS_H_

