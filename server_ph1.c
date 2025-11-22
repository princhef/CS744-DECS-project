#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mysql/mysql.h>
#include <unistd.h>
#include <strings.h>
#include <pthread.h>
#include "civetweb.h" 

#define POOL_SIZE 8
#define CACHE_CAPACITY 10



MYSQL *db_pool[POOL_SIZE];
int db_used[POOL_SIZE];
pthread_mutex_t db_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t db_cond  = PTHREAD_COND_INITIALIZER;



struct crs{
    char cid[10];
    char cname[100];
    char instr[100];
};




    //Linked List

    struct Node {
        struct crs data;
        struct Node* next;
    };


    struct Student{
        int sid;
        char cid[10];
        char cname[100];
        int marks;
    };
    

// Node in LRU cache
    typedef struct StdNode {
        struct Student data;
        struct StdNode* prev;
        struct StdNode* next;
    } StdNode;



    // LRU cache with dummy head and tail
    typedef struct {
        StdNode* head; // dummy head
        StdNode* tail; // dummy tail
        int size;
    } LRUCache;



    // Create a new node
StdNode* createNode(struct Student s) {
    StdNode* node = (StdNode*)malloc(sizeof(StdNode));

    node->data = s;
    node->prev = node->next = NULL;
    return node;
}


// Initialize LRU cache with dummy nodes
LRUCache* createCache() {
    LRUCache* cache = (LRUCache*)malloc(sizeof(LRUCache));
    cache->head = createNode((struct Student){0});
    cache->tail = createNode((struct Student){0});
    cache->head->next = cache->tail;
    cache->tail->prev = cache->head;
    cache->size = 0;
    return cache;
}



// Add node right after dummy head 
void addToHead(LRUCache* cache, StdNode* node) {
    node->next = cache->head->next;
    node->prev = cache->head;
    cache->head->next->prev = node;
    cache->head->next = node;
}


// Remove node from linked list
void removeNode(StdNode* node) {
    node->prev->next = node->next;
    node->next->prev = node->prev;
}


// Move a node to head 
void moveToHead(LRUCache* cache, StdNode* node) {
    removeNode(node);
    addToHead(cache, node);
}

// Remove least recently used node (before dummy tail)
StdNode* removeTail(LRUCache* cache) {
    StdNode* node = cache->tail->prev;
    if (node == cache->head) return NULL;
    removeNode(node);
    return node;
}


// Find node by student id and course id
StdNode* findNode(LRUCache* cache, int sid, const char* cid) {
    StdNode* curr = cache->head->next;
    while (curr != cache->tail) {
        if (curr->data.sid == sid && strcmp(curr->data.cid, cid) == 0)
            return curr;
        curr = curr->next;
    }
    return NULL;
}




// ---------------- Cache Operations ----------------

// Get a student record
struct Student* get(LRUCache* cache, int sid, const char* cid) {
    StdNode* node = findNode(cache, sid, cid);
    if (!node)  return NULL;
    
    moveToHead(cache, node);
    return &node->data;
}




// Insert or update a record
void put(LRUCache* cache, struct Student s) {
    StdNode* node = findNode(cache, s.sid, s.cid);

    if (node) {
        node->data = s; // update record
        moveToHead(cache, node);
        return;
    }

    StdNode* newNode = createNode(s);
    addToHead(cache, newNode);
    cache->size++;

    if (cache->size > CACHE_CAPACITY) {
        StdNode* lru = removeTail(cache);
        free(lru);
        cache->size--;
    }
}

void del(LRUCache* cache,int sid,const char* cid){
    StdNode* s=findNode(cache,sid,cid);
    if(s){
        cache->size--;
        removeNode(s);
        free(s);
    }
}



void freeCache(LRUCache* cache) {
    StdNode* curr = cache->head;
    while (curr) {
        StdNode* next = curr->next;
        free(curr);
        curr = next;
    }
    free(cache);
}

struct Node* head;
LRUCache* cache;
    
    //insertathead
void createcourseNode(struct Node* hd,char cid[],char cname[],char instr[]) {
        struct Node* newNode = (struct Node*)malloc(sizeof(struct Node));
        strcpy(newNode->data.cid,cid);
        strcpy(newNode->data.cname,cname);
        strcpy(newNode->data.instr,instr);
        newNode->next = NULL;

        newNode->next=hd->next;
        hd->next=newNode;
    }



//struct Student coursetaken[CACHE_ENTRIES];


void freeList(struct Node** head) {
        struct Node* temp;
        while (*head != NULL) {
            temp = *head;
            *head = (*head)->next;
            free(temp);
        }
    }





/* --- Get a free MySQL connection --- */
MYSQL *get_db_conn() {
    pthread_mutex_lock(&db_lock);
    while (1) {
        for (int i = 0; i < POOL_SIZE; i++) {
            if (!db_used[i]) {
                db_used[i] = 1;
                pthread_mutex_unlock(&db_lock);
                return db_pool[i];
            }
        }
        pthread_cond_wait(&db_cond, &db_lock);
    }
}




/* --- Release a MySQL connection --- */
void release_db_conn(MYSQL *conn) {
    pthread_mutex_lock(&db_lock);
    for (int i = 0; i < POOL_SIZE; i++) {
        if (db_pool[i] == conn) {
            db_used[i] = 0;
            pthread_cond_signal(&db_cond);
            break;
        }
    }
    pthread_mutex_unlock(&db_lock);
}




static int api_handler(struct mg_connection *conn, void *cbdata) {
    (void)cbdata;
    const struct mg_request_info *req = mg_get_request_info(conn);
    if (!req || !req->request_method) {
        mg_send_http_error(conn, 500, "Internal server error");
        return 500;
    }

    MYSQL* conntn=get_db_conn();

    int ptype,id,reqtype,n,qs_len;
    char query[256];
    char buf[256];
    const char *qs = req->query_string;
    
    if (qs && qs[0] != '\0') {
        char buf[256];
        memset(buf,0,256);
        qs_len = (int)strlen(qs);

        n = mg_get_var(qs, qs_len, "id", buf, sizeof(buf));
         if(n>0) id=atoi(buf);
        else mg_printf(conn, "mg_get_var: id not present\n");

        memset(buf,0,256);
        n = mg_get_var(qs, qs_len, "ptype", buf, sizeof(buf));
        if(n>0) ptype=atoi(buf);
        else mg_printf(conn, "mg_get_var: person type not present\n");

        memset(buf,0,256);
        n = mg_get_var(qs, qs_len, "reqtype", buf, sizeof(buf));
        if(n>0) reqtype=atoi(buf);
        else mg_printf(conn, "mg_get_var: request type not present\n");
    }
        //send header
            mg_printf(conn,
                        "HTTP/1.1 200 OK\r\n"
                        "Content-Type: text/plain\r\n"
                        "Connection: close\r\n\r\n");


    if (strcmp(req->request_method, "GET") == 0) {

        //student
        if(ptype==0){

            //view all courses
            if(reqtype==0){
                //initially cache miss
                if(!head->next){
                    sprintf(query,"SELECT c_id,c_name,instr_name FROM courses,instructor where courses.instructor=instructor.instr_id;");
            
                    if (mysql_query(conntn, query)) {
                        mg_printf(conn,"Database error\n");
                        release_db_conn(conntn);
                        return 500;
                    }
                    
                    MYSQL_RES *res;
                    MYSQL_ROW row;
                    res = mysql_store_result(conntn);

                    while ((row = mysql_fetch_row(res))) {
                    mg_printf(conn,"Course no. %s | Course name is %s | Course Instructor %s\n",row[0],row[1],row[2]);
                    pthread_mutex_lock(&db_lock);
                    createcourseNode(head,row[0],row[1],row[2]);
                    pthread_mutex_unlock(&db_lock);
                    }
                }


                //cache hit
                else{
                    mg_printf(conn,"Loading from Cache....\n");
                    struct Node* traverse=head->next;
                    while(traverse){
                        mg_printf(conn,"Course no. %s | Course name is %s | Course Instructor %s\n",traverse->data.cid,traverse->data.cname,traverse->data.instr);
                        traverse=traverse->next;
                    }
                }
            }

            //view marks of a enrolled course
            else if(reqtype==1){
                     
                char cid[10];
                memset(cid,0,10);
                memset(buf,0,256);
                n = mg_get_var(qs, qs_len, "cid", buf, sizeof(buf));
                if(n>0) strcpy(cid,buf);
                else mg_printf(conn, "mg_get_var: id not present\n");

                pthread_mutex_lock(&db_lock);
                struct Student* s=get(cache,id,cid);
                pthread_mutex_unlock(&db_lock);
                
                if(s){
                    mg_printf(conn,"Loading from cache...\n");
                    mg_printf(conn,"Course no. %s | Course name is %s | Marks %d\n",cid,s->cname,s->marks);
                }
                
                else{
                    sprintf(query, "SELECT courses.c_id,c_name,marks FROM course_taken,courses where course_taken.c_id=courses.c_id AND s_id=%d AND courses.c_id='%s';",id,cid);
                    if (mysql_query(conntn, query)) {
                        mg_printf(conn,"Database error\n");
                        release_db_conn(conntn);
                        return 500;
                    }
                    MYSQL_RES *res;
                    MYSQL_ROW row;
                    res = mysql_store_result(conntn);

                    while ((row = mysql_fetch_row(res))) {
                        mg_printf(conn,"Course no. %s | Course name is %s | Marks %s\n",row[0],row[1],row[2]);
                        struct Student s;
                        s.sid=id;
                        strcpy(s.cid,row[0]);
                        strcpy(s.cname,row[1]);
                        s.marks=atoi(row[2]);
                        pthread_mutex_lock(&db_lock);
                        put(cache,s);
                        pthread_mutex_unlock(&db_lock);
                    }
                }
           }
            

        }

            release_db_conn(conntn);
        return 200;
    }

    if (strcmp(req->request_method, "DELETE") == 0) {
        

        //student drop a course
        if(ptype==0){

            
            memset(buf,0,256);
            char cid[10];
            n = mg_get_var(qs, qs_len, "cid", buf, sizeof(buf));
            if(n>0) strcpy(cid,buf);
            else mg_printf(conn, "mg_get_var: course id not present\n");


                //remove in cache
                pthread_mutex_lock(&db_lock);
                del(cache,id,cid);
                pthread_mutex_unlock(&db_lock);

                sprintf(query, "DELETE FROM course_taken WHERE s_id=%d AND c_id='%s' ", id,cid);
                  if (mysql_query(conntn, query)) {
                    mg_printf(conn,"Database error\n");
                }
                
                mg_printf(conn,"Deleted");
        }

        //instr remove a course
        else{

            memset(buf,0,256);
            char cid[10];
            n = mg_get_var(qs, qs_len, "cid", buf, sizeof(buf));
            if(n>0) strcpy(cid,buf);
            else mg_printf(conn, "mg_get_var: course id not present\n");

            sprintf(query, "DELETE FROM courses WHERE instructor=%d AND c_id='%s';", id,cid);
                  if (mysql_query(conntn, query)) {
                    mg_printf(conn,"Database error\n");
                }
            pthread_mutex_lock(&db_lock);
            struct Node* curr=head->next;
            struct Node* prev=head;
            while(curr && strcmp(curr->data.cid,cid)!=0){
                prev=curr;
                curr=curr->next;
            }
            if(curr){
                prev->next=curr->next;
                 free(curr);
            }
            pthread_mutex_unlock(&db_lock);
                mg_printf(conn,"Deleted");

        }

        release_db_conn(conntn);
        return 200;
    }



    if (strcmp(req->request_method, "POST") == 0) {
            

            long long content_len = req->content_length;
            if (content_len < 0) content_len = 0; 
            char cname[100];
            int to_read=content_len;
            int total = 0;
            

            while (total < to_read) {
                int r = mg_read(conn, cname + total, to_read - total);
                if (r <= 0) break;
                total += r;
            }
            cname[total] = '\0';


            //student enroll a course
            if(ptype==0){

                memset(buf,0,256);
                char cid[10];
                n = mg_get_var(qs, qs_len, "cid", buf, sizeof(buf));
                if(n>0) strcpy(cid,buf);
                else mg_printf(conn, "mg_get_var: course id not present\n");

                sprintf(query, "INSERT INTO course_taken VALUES(%d,'%s',0)", id,cid);
                    if (mysql_query(conntn, query)) {
                        mg_printf(conn,"Database error\n");
                    }

                    mg_printf(conn,"Course Enrolled");
            }

            //instr add a course
            else{

                memset(buf,0,256);
                char cid[10];
                n = mg_get_var(qs, qs_len, "cid", buf, sizeof(buf));
                if(n>0) strcpy(cid,buf);
                else mg_printf(conn, "mg_get_var: course id not present\n");

                sprintf(query, "INSERT INTO courses VALUES('%s','%s',%d) ",cid,cname,id);
                    if (mysql_query(conntn, query)) {
                        mg_printf(conn,"Database error\n");
                    }
                sprintf(query,"Select instr_name from instructor where instr_id=%d",id);
                
                if (mysql_query(conntn, query)) {
                        mg_printf(conn,"Database error\n");
                    }
                MYSQL_RES *res;
                        MYSQL_ROW row;
                        res = mysql_store_result(conntn);
                row = mysql_fetch_row(res);
                char tmpry[100];
                strcpy(tmpry,row[0]);
                pthread_mutex_lock(&db_lock);
                if(head->next) createcourseNode(head,cid,cname,tmpry);
                    mg_printf(conn,"Course Added");
                    pthread_mutex_unlock(&db_lock);
            }

                release_db_conn(conntn);
            return 200;
    }


    if (strcmp(req->request_method, "PUT") == 0) {
        


        //instructor update marks 
        memset(buf,0,256);
            char cid[10];
            n = mg_get_var(qs, qs_len, "cid", buf, sizeof(buf));
            if(n>0) strcpy(cid,buf);
            else mg_printf(conn, "mg_get_var: course id not present\n");

            int marks;
            memset(buf,0,256);
            n = mg_get_var(qs, qs_len, "marks", buf, sizeof(buf));
            if(n>0) marks=atoi(buf);
            else mg_printf(conn, "mg_get_var: marks not present\n");

            int sid;
            memset(buf,0,256);
            n = mg_get_var(qs, qs_len, "sid", buf, sizeof(buf));
            if(n>0) sid=atoi(buf);
            else mg_printf(conn, "mg_get_var: student id not present\n");


            sprintf(query, "UPDATE course_taken SET marks=%d WHERE c_id='%s' AND s_id=%d;",marks,cid,sid);
            
                  if (mysql_query(conntn, query)) {
                    mg_printf(conn,"Database error\n");
                    release_db_conn(conntn);
                    return 500;
                }

            
                //invalidate cache
                pthread_mutex_lock(&db_lock);
            del(cache,sid,cid);
            pthread_mutex_unlock(&db_lock);

            mg_printf(conn,"Marks Updated");
        
            release_db_conn(conntn);
        return 200;
    }

   

    /* Other methods: not allowed */
    mg_send_http_error(conn, 405, "Method not allowed");
    return 405;
}





int main(void) {

    const char *options[] = {
        "document_root", ".",           
        "listening_ports", "8080",  
        "num_threads", "30",    
        NULL
    };
    
    /* --- Initialize the MySQL connection pool --- */
        
            for (int i = 0; i < POOL_SIZE; i++) {
                db_pool[i] = mysql_init(NULL);
                if (!mysql_real_connect(db_pool[i], "localhost", "pdy", "password123",
                                        "project", 0, NULL, 0)) {
                    fprintf(stderr, "MySQL connect failed: %s\n", mysql_error(db_pool[i]));
                    return -1;
                }
                db_used[i] = 0;
            }



            

//initialize head of linked list
head = (struct Node*)malloc(sizeof(struct Node));
head->next = NULL;




//initialize the cache

cache = createCache();

    struct mg_context *ctx = mg_start(NULL, NULL, options);
    if (!ctx) {
        fprintf(stderr, "Failed to start CivetWeb\n");
        return 1;
    }

    /* register our /api handler */
    mg_set_request_handler(ctx, "/api", api_handler, NULL);

    printf("CivetWeb minimal API running at http://localhost:8080/api\n");
    printf("Press Enter to stop server...\n");
    getchar();

    
    mg_stop(ctx);
     for (int i = 0; i < POOL_SIZE; i++)
        mysql_close(db_pool[i]);
    freeList(&head);
    freeCache(cache);
    return 0;
}
