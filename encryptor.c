#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>

typedef struct arg_thread {
    char* map_in;
    char* map_out;
    int*  map_key;
    size_t len_map;     
    size_t key_len;
    size_t diff;        
    size_t safe_size;   
    sem_t* semafor;
} arg_thread;

void afiseaza_progres(off_t curent, off_t total) {
    int latime_bara = 50;
    double procent = (double)curent / total;
    if (procent > 1.0) procent = 1.0;
    int plin = (int)(latime_bara * procent);
    static int spin_index = 0;
    char spinner[] = "|/-\\";

    printf("\r\033[1;36m[PROGRES]\033[0m [");

    for (int i = 0; i < latime_bara; i++) {
        if (i < plin) 
            printf("\033[1;32m#\033[0m");
        else 
            printf("."); 
    }
    printf("] %3.0f%% %c", procent * 100, spinner[spin_index]);
    
    spin_index = (spin_index + 1) % 4;
    fflush(stdout);
}

void permutare_cuvant(char* cuv_in, size_t cuv_len, char* cuv_out, int* key_out, unsigned int* seed) {
    int *perm = (int*)malloc(cuv_len * sizeof(int));
    for (int i = 0; i < cuv_len; i++) 
        perm[i] = i;
    for (int i = cuv_len -1; i > 0; i--) {
        int x = rand_r(seed)%(i+1);
        int aux = perm[i];
        perm[i] = perm[x];
        perm[x] = aux; 
    }
    for (int i = 0; i < cuv_len; i++) {
        cuv_out[i] = cuv_in[perm[i]];
        key_out[i] = perm[i];
    }
    free(perm);
}

void aranjare_cuvant(char* cuv_in, size_t cuv_len, char* cuv_out, int* key_out) {
    for (int i = 0; i < cuv_len; i++) {
        cuv_out[key_out[i]] = cuv_in[i];
    }
}

size_t get_safe_size(char* data, size_t chunk_limit, size_t file_remaining) {
    if (chunk_limit >= file_remaining)
        return file_remaining;
    size_t safe_size = chunk_limit;
    while (safe_size > 0 && !isspace((unsigned char)data[safe_size - 1]) && !ispunct((unsigned char)data[safe_size - 1]))
        safe_size--;
    if (safe_size == 0)
        return chunk_limit;
    return safe_size;
}

void* encript_thread(void* arg) {
    arg_thread* arg_t = (arg_thread*)arg;
    char* map_in = arg_t->map_in;
    char* map_out = arg_t->map_out;
    int*  map_key = arg_t->map_key;
    size_t len_map = arg_t->len_map;     
    size_t key_len = arg_t->key_len;
    size_t diff = arg_t->diff;        
    size_t safe_size = arg_t->safe_size;   
    sem_t* semafor = arg_t->semafor;

    char* data_in = map_in + diff;
    char* data_out = map_out + diff;
    int* data_key = map_key + diff;
    unsigned int seed = (unsigned int)time(NULL)^(unsigned int)pthread_self();

    size_t i = 0;
    while (i < safe_size) {
        if (isspace((unsigned char)data_in[i]) || ispunct((unsigned char)data_in[i])) {
            data_out[i] = data_in[i];
            data_key[i] = -1;
            i++;
        }
        else {
            size_t start = i;
            while (i < safe_size && !isspace((unsigned char)data_in[i]) && !ispunct((unsigned char)data_in[i])) 
                i++;
            size_t len = i - start;
            permutare_cuvant(data_in+start, len, data_out+start, data_key+start, &seed);
        }
    }
    munmap(map_in, len_map);
    munmap(map_out, len_map);
    munmap(map_key, key_len);
    sem_post(semafor);
    free(arg_t);
    return NULL;
}

void* decript_thread(void* arg) {
    arg_thread* arg_t = (arg_thread*)arg;
    char* map_in = arg_t->map_in;
    char* map_out = arg_t->map_out;
    int*  map_key = arg_t->map_key;
    size_t len_map = arg_t->len_map;     
    size_t key_len = arg_t->key_len;
    size_t diff = arg_t->diff;        
    size_t safe_size = arg_t->safe_size;   
    sem_t* semafor = arg_t->semafor;

    char* data_in = map_in + diff;
    char* data_out = map_out + diff;
    int* data_key = map_key + diff;

    size_t i = 0;
    while (i < safe_size) {
        if (data_key[i] == -1) {
            data_out[i] = data_in[i];
            i++;
        }
        else {
            size_t start = i;
            while (i < safe_size && data_key[i] != -1) 
                i++;
            size_t len = i - start;
            aranjare_cuvant(data_in+start, len, data_out+start, data_key+start);
        }
    }
    munmap(map_in, len_map);
    munmap(map_out, len_map);
    munmap(map_key, key_len);
    sem_post(semafor);
    free(arg_t);
    return NULL;
}

void nume_out_encript(const char* nume_in, char* nume_out, char* nume_key) {
    char* dot = strrchr(nume_in, '.');
    if (dot == NULL) {
        sprintf(nume_out, "%s_criptat", nume_in);
    }
    else {
        int len = dot - nume_in;
        strncpy(nume_out, nume_in, len);
        nume_out[len] = '\0'; 
        strcat(nume_out, "_criptat");
        strcat(nume_out, dot);
    }
    strcpy(nume_key, nume_in);
    dot = strrchr(nume_key, '.');
    if(dot) *dot = '\0';
    strcat(nume_key, "_key.bin");
}

void nume_out_decript(const char* nume_in, char* nume_out) {
    char* dot = strrchr(nume_in, '.');
    if (dot == NULL) {
        sprintf(nume_out, "%s_decriptat", nume_in);
    }
    else {
        int len = dot - nume_in;
        strncpy(nume_out, nume_in, len);
        nume_out[len] = '\0'; 
        strcat(nume_out, "_decriptat");
        strcat(nume_out, dot);
    }
}

void encriptare(const char* fisier_in) {
    printf("[ENCRIPT] Procesare fisier: %s\n", fisier_in);
    int fd_in = open(fisier_in, O_RDONLY);
    if (fd_in == -1) {
        perror("Eroare la deschiderea fisierului de intrare\n");
        exit(1);
    }

    struct stat sf;
    if (fstat(fd_in, &sf) == -1) {
        perror("Eroare la fstat\n");
        close(fd_in);
        exit(1);
    }
    if (sf.st_size == 0) {
        printf("Fisier gol\n");
        close(fd_in);
        exit(0);
    }

    printf("[ENCRIPT] Fisier deschis. Dimensiune: %ld bytes\n", sf.st_size);
    int len_name_out = strlen(fisier_in)+9;
    int len_name_key = strlen(fisier_in)+9;
    char fisier_out[len_name_out];
    char fisier_key[len_name_key];
    nume_out_encript(fisier_in, fisier_out, fisier_key);

    int fd_out = open(fisier_out, O_RDWR | O_CREAT | O_TRUNC, 0600);
    if (fd_out == -1) {
        perror("Eroare la crearea fisierului de iesire\n");
        close(fd_in);
        exit(1);
    }
    if (ftruncate(fd_out, sf.st_size) == -1) {
        perror("Eroare la ftruncate output");
        close(fd_in);
        close(fd_out);
        exit(1);
    }

    int fd_key = open(fisier_key, O_RDWR | O_CREAT | O_TRUNC, 0600);
    if (fd_key == -1) {
        perror("Eroare la crearea fisierului de chei\n");
        close(fd_in);
        close(fd_out);
        exit(1);
    }
    off_t size_key = sf.st_size * sizeof(int);
    if (ftruncate(fd_key, size_key) == -1) {
        perror("Eroare la ftruncate chei");
        close(fd_in);
        close(fd_out);
        exit(1);
    }
    printf("[ENCRIPT] Spatiu rezervat: Output (%ld bytes), Key (%ld bytes)\n", sf.st_size, size_key);

    // preprocesare incheiata
    long page_size = sysconf(_SC_PAGE_SIZE);
    off_t global_cursor = 0;
    size_t CHUNK_SIZE = 4096*1024;
    sem_t semafor;
    long num_cores = sysconf(_SC_NPROCESSORS_ONLN);
    if (num_cores < 1) 
        num_cores = 8; 
    sem_init(&semafor, 0, num_cores);
    while (global_cursor < sf.st_size) {
        sem_wait(&semafor);
        off_t mmap_offset = (global_cursor / page_size) * page_size;
        size_t diff = global_cursor - mmap_offset; // offset pentru bytes din pagina procesata anterior
        size_t len_map = diff + CHUNK_SIZE;
        if (mmap_offset + len_map > sf.st_size) {
            len_map = sf.st_size - mmap_offset;
        }
        char* map_in = mmap(NULL, len_map, PROT_READ, MAP_SHARED, fd_in, mmap_offset);
        if (map_in == MAP_FAILED) {
            perror("Eroare la maparea fisierului de intrare");
            exit(1);
        }
        char* map_out = mmap(NULL, len_map, PROT_WRITE, MAP_SHARED, fd_out, mmap_offset);
        if (map_out == MAP_FAILED) {
            perror("Eroare la maparea fisierului de iesire");
            exit(1);
        }
        off_t key_offset = mmap_offset * sizeof(int);
        size_t key_len = len_map * sizeof(int);
        int* map_key = mmap(NULL, key_len, PROT_WRITE, MAP_SHARED, fd_key, key_offset);
        if (map_key == MAP_FAILED) {
            perror("Eroare la maparea fisierului de chei");
            exit(1);
        }
        size_t safe_size = get_safe_size(map_in + diff, len_map - diff, sf.st_size - global_cursor);

        arg_thread* arg_t = (arg_thread*)malloc(sizeof(arg_thread));
        if (!arg_t) {
            perror("Eroare malloc arg_t");
            exit(1);
        }
        arg_t->map_in = map_in;
        arg_t->map_out = map_out;
        arg_t->map_key = map_key;
        arg_t->len_map = len_map;
        arg_t->key_len = key_len;
        arg_t->diff = diff;
        arg_t->safe_size = safe_size;
        arg_t->semafor = &semafor;
        
        pthread_t tid;
        if (pthread_create(&tid, NULL, encript_thread, arg_t) != 0) {
            perror("Eroare thread");
            free(arg_t);
            exit(1);
        }
        pthread_detach(tid);

        global_cursor += safe_size;
        afiseaza_progres(global_cursor, sf.st_size);
    }

    for (int i = 0; i < num_cores; i++) {
        sem_wait(&semafor);
    }
    sem_destroy(&semafor);
    printf("\n[ENCRIPT] Criptare incheiata\n");

    close(fd_in);
    close(fd_out);
    close(fd_key);
}

void decriptare(const char* fisier_in, const char* fisier_key) {
    printf("[DECRIPT] Procesare fisier: %s\n", fisier_in);
    int fd_in = open(fisier_in, O_RDONLY);
    if (fd_in == -1) {
        perror("Eroare la deschiderea fisierului de intrare\n");
        exit(1);
    }

    struct stat sf;
    if (fstat(fd_in, &sf) == -1) {
        perror("Eroare la fstat\n");
        close(fd_in);
        exit(1);
    }
    if (sf.st_size == 0) {
        printf("Fisier gol\n");
        close(fd_in);
        exit(0);
    }

    printf("[DECRIPT] Fisier deschis. Dimensiune: %ld bytes\n", sf.st_size);
    int len_name_out = strlen(fisier_in) + 11;
    char fisier_out[len_name_out]; 
    nume_out_decript(fisier_in, fisier_out);
    int fd_out = open(fisier_out, O_RDWR | O_CREAT | O_TRUNC, 0600);
    if (fd_out == -1) {
        perror("Eroare la deschiderea fisierului de iesire\n");
        exit(1);
    }
    if (ftruncate(fd_out, sf.st_size) == -1) {
        perror("Eroare la ftruncate output");
        close(fd_in);
        close(fd_out);
        exit(1);
    }
    int fd_key = open(fisier_key, O_RDONLY);
    if (fd_key == -1) {
        perror("Eroare la deschiderea fisierului de chei\n");
        close(fd_in);
        close(fd_out);
        exit(1);
    }
    printf("[DECRIPT] Spatiu rezervat: Output (%ld bytes)\n", sf.st_size);

    // preprocesare incheiata
    long page_size = sysconf(_SC_PAGE_SIZE);
    off_t global_cursor = 0;
    size_t CHUNK_SIZE = 4096*1024;
    sem_t semafor;
    long num_cores = sysconf(_SC_NPROCESSORS_ONLN);
    if (num_cores < 1) 
        num_cores = 8;
    sem_init(&semafor, 0, num_cores);
    while (global_cursor < sf.st_size) {
        sem_wait(&semafor);
        off_t mmap_offset = (global_cursor / page_size) * page_size;
        size_t diff = global_cursor - mmap_offset; // offset pentru bytes din pagina procesata anterior
        size_t len_map = diff + CHUNK_SIZE;
        if (mmap_offset + len_map > sf.st_size) {
            len_map = sf.st_size - mmap_offset;
        }
        char* map_in = mmap(NULL, len_map, PROT_READ, MAP_SHARED, fd_in, mmap_offset);
        if (map_in == MAP_FAILED) {
            perror("Eroare la maparea fisierului de intrare");
            exit(1);
        }
        char* map_out = mmap(NULL, len_map, PROT_WRITE, MAP_SHARED, fd_out, mmap_offset);
        if (map_out == MAP_FAILED) {
            perror("Eroare la maparea fisierului de iesire");
            exit(1);
        }
        off_t key_offset = mmap_offset * sizeof(int);
        size_t key_len = len_map * sizeof(int);
        int* map_key = mmap(NULL, key_len, PROT_READ, MAP_SHARED, fd_key, key_offset);
        if (map_key == MAP_FAILED) {
            perror("Eroare la maparea fisierului de chei");
            exit(1);
        }
        size_t safe_size = get_safe_size(map_in + diff, len_map - diff, sf.st_size - global_cursor);

        arg_thread* arg_t = (arg_thread*)malloc(sizeof(arg_thread));;
        if (!arg_t) {
            perror("Eroare malloc arg_t");
            exit(1);
        }
        arg_t->map_in = map_in;
        arg_t->map_out = map_out;
        arg_t->map_key = map_key;
        arg_t->len_map = len_map;
        arg_t->key_len = key_len;
        arg_t->diff = diff;
        arg_t->safe_size = safe_size;
        arg_t->semafor = &semafor;
        
        pthread_t tid;
        if (pthread_create(&tid, NULL, decript_thread, arg_t) != 0) {
            perror("Eroare thread");
            free(arg_t);
            exit(1);
        }
        pthread_detach(tid);

        global_cursor += safe_size;
        afiseaza_progres(global_cursor, sf.st_size);
    }
    for (int i = 0; i < num_cores; i++) {
        sem_wait(&semafor);
    }
    sem_destroy(&semafor);
    printf("\n[DECRIPT] Decriptare incheiata\n");

    close(fd_in);
    close(fd_out);
    close(fd_key);
}

int main(int argc, char *argv[]) {
    if (argc == 2) { // encriptare
        printf("[INFO] Mod Encriptare\n");
        encriptare(argv[1]);
    }
    else if (argc == 3) { // decriptare
        printf("[INFO] Mod Decriptare\n");
        decriptare(argv[1], argv[2]);
    }
    else {
        printf("[INFO] Numar invalid de argumente\n");
    }
}