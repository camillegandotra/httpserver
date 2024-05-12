/****************************************************************************************
/ 	Assignment 4
/  
/	Author: Camille Gandotra
/  	CruzID: cgandotr
/
/  	httpserver.c
/  	~ HTTP Server for ASGN 4
/
*****************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <err.h>
#include <errno.h>
#include <signal.h>

#include "asgn2_helper_funcs.h"
#include "connection.h"
#include "response.h"
#include "request.h"
#include "queue.h"
#include "rwlock.h"

#define OPTIONS "t:"

/*****************************************************************************************/

// Helper Code (Given Skeleton)
void handle_connection(int);

void handle_get(conn_t *);
void handle_put(conn_t *);
void handle_unsupported(conn_t *);

/*********************************----NODE----****************************************/

typedef struct Node Node;

struct Node {
    char *key;
    rwlock_t *rwlock;
    Node *next;
    Node *prev;
};

Node *node_create(char *key, rwlock_t *rwlock) {
    Node *n = (Node *) malloc(sizeof(Node));
    if (n != NULL) {
        n->next = NULL;
        n->prev = NULL;
        if (key != NULL) {
            n->key = strdup(key);
        } else {
            n->key = NULL;
        }
        n->rwlock = rwlock;
    }
    return n;
}

void node_delete(Node **n) {
    (*n)->next = NULL;
    (*n)->prev = NULL;
    (*n)->rwlock = NULL;
    free((*n)->key);
    (*n)->key = NULL;
    free((*n));
    (*n) = NULL;
    return;
}

/*********************************----Linked List----****************************************/

typedef struct LinkedList LinkedList;

struct LinkedList {
    Node *head;
    Node *tail;
};

LinkedList *ll_create(void) {
    LinkedList *ll = (LinkedList *) malloc(sizeof(LinkedList));
    if (ll != NULL) {
        ll->head = node_create(NULL, NULL);
        ll->tail = node_create(NULL, NULL);
        ll->head->next = ll->tail;
        ll->tail->prev = ll->head;
    }
    return ll;
}

void ll_delete(LinkedList **ll) {
    Node *cur = (*ll)->head;
    // Delete all nodes in LL
    while (cur != NULL) {
        Node *next = cur->next;
        node_delete(&cur);
        cur = next;
    }
    // Once we delete all the nodes in our Linked List
    free(*ll); // Free our Linked List
    (*ll) = NULL;
    return;
}

Node *ll_lookup(LinkedList *ll, char *key) {
    Node *cur = ll->head->next; // Cur Node
    while (cur != ll->tail) { // Iterate through the LL
        if (!strcmp(cur->key, key)) {
            return cur;
        }
        cur = cur->next;
    }
    return NULL; // NULL if not found
}

void ll_insert(LinkedList *ll, char *key, rwlock_t *rwlock) {
    if (ll_lookup(ll, key) == NULL) { // Ensure not already there
        // Insert At Front
        Node *cur = node_create(key, rwlock);
        cur->next = ll->head->next;
        ll->head->next = cur;

        cur->next->prev = cur;
        cur->prev = ll->head;
    }
    return;
}

/*********************************----Hash Table----****************************************/

// polynomial rolling hash function (found help for implementation online)
unsigned long long get_hash(char *str, unsigned long long mod) {
    unsigned long long hash = 0;
    unsigned long long base = 256;
    for (int i = 0; str[i] != '\0'; i++) {
        hash = (hash * base + str[i]) % mod;
    }
    return hash;
}

typedef struct HashTable HashTable;

struct HashTable {
    uint32_t size;
    LinkedList **lists;
};

uint32_t ht_size(HashTable *ht) {
    return ht->size;
}

HashTable *ht_create(uint32_t size) {
    HashTable *ht = (HashTable *) malloc(sizeof(HashTable));
    if (ht != NULL) {
        ht->size = size;
        ht->lists = (LinkedList **) calloc(size, sizeof(LinkedList *));
        if (!ht->lists) {
            free(ht);
            ht = NULL;
        }
    }
    return ht;
}

void ht_delete(HashTable **ht) {
    for (uint32_t i = 0; i < ht_size(*ht); i += 1) { // Delete all the LL that are not NULL
        if ((*ht)->lists[i] != NULL) {
            ll_delete(&(*ht)->lists[i]);
        }
    }
    free((*ht)->lists); // Free the List of Linked Lists
    ((*ht)->lists) = NULL;
    free(*ht); // Free the Hash Table itself
    (*ht) = NULL;
    return;
}

Node *ht_lookup(HashTable *ht, char *key) {
    uint64_t idx = get_hash(key, ht->size);
    if (ht->lists[idx] == NULL) {
        return NULL;
    }
    Node *n = ll_lookup(ht->lists[idx], key);

    return n;
}

void ht_insert(HashTable *ht, char *key, rwlock_t *rwlock) {
    uint64_t idx = get_hash(key, ht->size);

    if (ht->lists[idx] == NULL) {
        ht->lists[idx] = ll_create();
    }
    if (!ll_lookup(ht->lists[idx], key)) {
        ll_insert(ht->lists[idx], key, rwlock);
    }
    return;
}

/*****************************************************************************************/

// HT Lock
pthread_mutex_t mutex;

// Bounded Buffer
queue_t *BB;

// Hash Table
HashTable *HT;

// Invalid Thread Function
void invalid_thread(void) {
    fprintf(stderr, "Invalid Number for Threads\n");
    exit(EXIT_FAILURE);
}

// Invalid Port Function
void invalid_port(void) {
    fprintf(stderr, "Invalid Port\n");
    exit(EXIT_FAILURE);
}

// Audit Log Function
void audit_log(char *op, char *uri, char *status_code, int request_id) {
    fprintf(stderr, "%s,/%s,%s,%d\n", op, uri, status_code, request_id);
}

// Handle Connection Function (given connection ID)
void handle_connection(int connfd) {
    // Create Connection from ID
    conn_t *conn = conn_new(connfd);

    // Static Parse
    const Response_t *res = conn_parse(conn);
    if (res != NULL) {
        conn_send_response(conn, res);
    } else {
        // Check Type of Request
        const Request_t *req = conn_get_request(conn);
        if (req == &REQUEST_GET) {
            handle_get(conn);
        } else if (req == &REQUEST_PUT) {
            handle_put(conn);
        } else {
            handle_unsupported(conn);
        }
    }
    // Delete Connection
    conn_delete(&conn);
}

// Handle GET Request Function
void handle_get(conn_t *conn) {
    // Get URI
    char *uri = conn_get_uri(conn);

    // Get Request ID (Optional -> 0)
    char *rID = conn_get_header(conn, "Request-Id");
    if (rID == NULL) {
        rID = "0";
    }
    int request_id = atoi(rID);

    // !!! START OF CS - Get RW LOCK !!!

    pthread_mutex_lock(&mutex); // Lock using Mutex

    Node *URI_NODE = ht_lookup(HT, uri);
    if (URI_NODE == NULL) {
        rwlock_t *RWL = rwlock_new(N_WAY, 1);
        ht_insert(HT, uri, RWL);
        URI_NODE = ht_lookup(HT, uri);
    }

    // Reader Lock
    reader_lock(URI_NODE->rwlock);

    pthread_mutex_unlock(&mutex); // Unlock using Mutex

    // 1. Open the file.
    int fd = open(uri, O_RDONLY);
    // If  open it returns < 0, then use the result appropriately
    if (fd < 0) {
        // a. Cannot access -- use RESPONSE_FORBIDDEN (403)
        if (errno == EACCES) {
            conn_send_response(conn, &RESPONSE_FORBIDDEN);
            audit_log("GET", uri, "403", request_id);
            close(fd);
            reader_unlock(URI_NODE->rwlock);
            return;
            // b. Cannot find the file -- use RESPONSE_NOT_FOUND (404)
        } else if (errno == ENOENT) {
            conn_send_response(conn, &RESPONSE_NOT_FOUND);
            audit_log("GET", uri, "404", request_id);
            close(fd);
            reader_unlock(URI_NODE->rwlock);
            return;
        }
        // c. other error? -- use RESPONSE_INTERNAL_SERVER_ERROR (500)
        else {
            conn_send_response(conn, &RESPONSE_INTERNAL_SERVER_ERROR);
            audit_log("GET", uri, "500", request_id);
            close(fd);
            reader_unlock(URI_NODE->rwlock);
            return;
        }
    }

    // 2. Get the size of the file.
    struct stat st;
    if (fstat(fd, &st) == -1) {
        exit(EXIT_FAILURE);
    }

    int contentLength = st.st_size;

    // 3. Check if the file is a directory, because directories *will*
    // open, but are not valid. (403)
    if (S_ISDIR(st.st_mode)) {
        conn_send_response(conn, &RESPONSE_FORBIDDEN);
        audit_log("GET", uri, "403", request_id);
        close(fd);
        reader_unlock(URI_NODE->rwlock);
        return;
    }

    // 4. Send the file
    conn_send_file(conn, fd, contentLength);
    audit_log("GET", uri, "200", request_id);
    close(fd);
    reader_unlock(URI_NODE->rwlock);
    return;
}

// Handle Unsupported
void handle_unsupported(conn_t *conn) {
    // Not Implemented (501)
    conn_send_response(conn, &RESPONSE_NOT_IMPLEMENTED);
    return;
}

// Handle PUT Request Function
void handle_put(conn_t *conn) {
    // Get URI
    char *uri = conn_get_uri(conn);
    const Response_t *res = NULL;

    // Get Request ID (Optional -> 0)
    char *rID = conn_get_header(conn, "Request-Id");
    if (rID == NULL) {
        rID = "0";
    }
    int request_id = atoi(rID);

    // !!! START OF CS - Get RW LOCK !!!

    pthread_mutex_lock(&mutex); // Lock using Mutex

    Node *URI_NODE = ht_lookup(HT, uri);
    if (URI_NODE == NULL) {
        rwlock_t *RWL = rwlock_new(N_WAY, 1);
        ht_insert(HT, uri, RWL);
        URI_NODE = ht_lookup(HT, uri);
    }

    writer_lock(URI_NODE->rwlock);

    pthread_mutex_unlock(&mutex); // Unlock using Mutex

    // Check if file already exists before opening it.
    bool existed = access(uri, F_OK) == 0;

    // Open the file..
    int fd = open(uri, O_CREAT | O_TRUNC | O_WRONLY, 0600);
    if (fd < 0) {
        if (errno == EACCES || errno == EISDIR || errno == ENOENT) {
            // 403
            conn_send_response(conn, &RESPONSE_FORBIDDEN);
            audit_log("PUT", uri, "403", request_id);
            close(fd);
            writer_unlock(URI_NODE->rwlock);
            return;
        } else {
            // 400
            conn_send_response(conn, &RESPONSE_INTERNAL_SERVER_ERROR);
            audit_log("PUT", uri, "500", request_id);
            close(fd);
            writer_unlock(URI_NODE->rwlock);
            return;
        }
    }

    // Get File
    res = conn_recv_file(conn, fd);

    if (res == NULL && existed) {
        // 200
        conn_send_response(conn, &RESPONSE_OK);
        audit_log("PUT", uri, "200", request_id);
    } else if (res == NULL && !existed) {
        // 201
        conn_send_response(conn, &RESPONSE_CREATED);
        audit_log("PUT", uri, "201", request_id);
    } else {
        // 500
        conn_send_response(conn, &RESPONSE_INTERNAL_SERVER_ERROR);
        audit_log("PUT", uri, "500", request_id);
    }

    close(fd);
    writer_unlock(URI_NODE->rwlock);
    return;
}

// Worker Thread (Function)
void *worker_thread_fnc(void *arg) {
    (void) arg;
    // Get Connection, Handle Connection, Close Connection
    while (1) {
        uintptr_t cID;
        if (!queue_pop(BB, (void **) &cID)) {
            // Should Never Happen
            exit(EXIT_FAILURE);
        }
        handle_connection(cID);
        close(cID);
    }
}

// Main Function
int main(int argc, char *argv[]) {
    int opt = 0; // Used to store the current user input
    int worker_threads = 4; // Number of Worker Threads (Default 4) - Optional
    unsigned long port = 0; // Port Number - Required

    // Get the inputs - ./httpserver [-t threads] <port>

    // Get # Threads (Optional)
    while ((opt = getopt(argc, argv, OPTIONS)) != -1) {
        switch (opt) {
        case 't':
            worker_threads = strtoul(optarg, NULL, 10);
            if (errno == EINVAL || worker_threads <= 0) { // 0?
                invalid_thread();
            }
            break;
        default: exit(EXIT_FAILURE); break;
        }
    }
    // Get Port from ARGS (Required)
    if (argv[optind] == (void *) 0) {
        invalid_port();
    }

    port = strtoul(argv[optind], NULL, 10);

    // Check if Valid Port Number + Reserve Port Numbers
    if (errno == EINVAL || port < 1 || port > 65535 || (0 < port && port <= 1023)) { // Reserved?
        invalid_port();
    }

    // Socket
    Listener_Socket socket;

    // Initialize Socket
    int init_status = listener_init(&socket, port);

    // Check if Socket was Initialized
    if (init_status == -1) {
        invalid_port();
    }

    // Create BB (size of Worker Threads)
    BB = queue_new(worker_threads);

    // Create HT size of Worker Threads)
    HT = ht_create(worker_threads);

    // Create HT Lock
    pthread_mutex_init(&mutex, NULL);

    // Creating Worker Threads
    pthread_t threads[worker_threads];
    for (int i = 0; i < worker_threads; i += 1) {
        pthread_create(&(threads[i]), NULL, worker_thread_fnc, NULL);
    }

    // Dispatcher Thread Work
    while (1) {
        uintptr_t connectionID = listener_accept(&socket);
        if (!queue_push(BB, (void *) connectionID)) {
            // Should Never Happen
            exit(EXIT_FAILURE);
        }
    }

    // Join Threads
    for (int i = 0; i < worker_threads; i += 1) {
        pthread_join(threads[i], NULL);
    }
    // Delete Queue & HT
    queue_delete(&BB);
    ht_delete(&HT);
    exit(EXIT_SUCCESS);
}
