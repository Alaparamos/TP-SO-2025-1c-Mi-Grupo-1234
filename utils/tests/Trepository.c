#include "Trepository.h"

// FUNCIONES DE REPO

t_list* dummy_list = NULL;
sem_t repo_sem;

void repo_init() {
    dummy_list = list_create();
    sem_init(&repo_sem, 0, 1);
}

void repo_destroy() {
    list_destroy(dummy_list);
    sem_destroy(&repo_sem);
}

t_list* get_list() {
    sem_wait(&repo_sem);
    return dummy_list;
}

void repo_add(void* element) {
    t_list* list = get_list();
    list_add(list, element);
    sem_post(&repo_sem);
}

void* repo_find(bool (*predicate)(void*)) {
    t_list* list = get_list();
    void* result = list_find(list, predicate);
    sem_post(&repo_sem);
    return result;
}

// TESTS
context (repository_tests) {

    describe("Thread-safe Repository") {

        before {
            repo_init();
        } end

        after {
            repo_destroy();
        } end

        it("Adds and finds multiple elements") {
            repo_add("one");
            repo_add("two");
            repo_add("three");

            bool find_two(void* data) {
                return strcmp((char*)data, "two") == 0;
            }

            char* found = repo_find(find_two);
            should_string(found) be equal to("two");
        } end

        it("Handles concurrent additions from 100 threads") {
            #define THREAD_COUNT 100
            pthread_t threads[THREAD_COUNT];

            void* add_func(void* arg) {
                char* str = malloc(20);
                sprintf(str, "value_%d", *((int*)arg));
                repo_add(str); // Ownership of memory goes to list
                free(arg);
                return NULL;
            }

            for (int i = 0; i < THREAD_COUNT; i++) {
                int* id = malloc(sizeof(int));
                *id = i;
                pthread_create(&threads[i], NULL, add_func, id);
            }

            for (int i = 0; i < THREAD_COUNT; i++) {
                pthread_join(threads[i], NULL);
            }

            t_list* list = get_list();
            int size = list_size(list);
            sem_post(&repo_sem); // Manually release the lock from get_list

            should_int(size) be equal to(THREAD_COUNT);
        } end

        it("Maintains data consistency under concurrent access") {
            #define THREADS 50
            pthread_t adders[THREADS];
            pthread_t finders[THREADS];

            void* add_func(void* arg) {
                char* str = malloc(20);
                sprintf(str, "item_%d", *((int*)arg));
                repo_add(str);
                free(arg);
                return NULL;
            }

            void* find_func(void* _) {
                bool predicate(void* data) {
                    return strstr((char*)data, "item_") != NULL;
                }
                void* result = repo_find(predicate);
                
                return NULL;
            }

            for (int i = 0; i < THREADS; i++) {
                int* id = malloc(sizeof(int));
                *id = i;
                pthread_create(&adders[i], NULL, add_func, id);
                pthread_create(&finders[i], NULL, find_func, NULL);
            }

            for (int i = 0; i < THREADS; i++) {
                pthread_join(adders[i], NULL);
                pthread_join(finders[i], NULL);
            }

            t_list* list = get_list();
            int size = list_size(list);
            sem_post(&repo_sem);

            should_int(size) be equal to(THREADS);
        } end
    }end
}

