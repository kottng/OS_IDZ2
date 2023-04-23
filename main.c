#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <fcntl.h>
#include <unistd.h>

typedef struct {
    int found_treasure; // флаг, указывающий на наличие клада на участке 0 - нет, 1 - да
    int num; // номер учатска
} area_t;

int main(void) {
    int fd; // объяснение ниже
    int NUM_AREAS; // общее количество участков острова / команд для поиска клада
    int TREASURE_AREA; // номер участка острова, на котором находится клад (спасибо карте флинта за такую информацию)
    area_t *areas; // массив всех участков
    sem_t *sem_treasure = NULL;
    int i, pid;
    scanf("%d", &NUM_AREAS);
    scanf("%d", &TREASURE_AREA);
    // создаем общую разделяемую память для участков острова
    fd = shm_open("/treasure_hunt", O_CREAT | O_RDWR, 0666);
    if (fd < 0) {
        perror("shm_open");
        exit(EXIT_FAILURE);
    }
    // меняем размер файла до заданной длины
    if (ftruncate(fd, NUM_AREAS * sizeof(area_t)) == -1) {
        perror("ftruncate");
        exit(EXIT_FAILURE);
    }
    areas = (area_t *) mmap(NULL, NUM_AREAS * sizeof(area_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (areas == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    // создаем именованный семафор для обнаружения клада
    sem_treasure = sem_open("/treasure", O_CREAT | O_EXCL, 0666, 0);
    // заполняем данные об участках острова
    if (sem_treasure == SEM_FAILED) {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }

    for (i = 0; i < NUM_AREAS; i++) {
        areas[i].found_treasure = 0;
        areas[i].num = i;
    }
    for (i = 0; i < NUM_AREAS; i++) {
        // создаем процессы для каждой команды / участка острова
        pid = fork();
        if (pid == -1) {
            perror("fork");
            exit(EXIT_FAILURE);
        }
        if (pid == 0) {
            // дочерний процесс
            printf("Exploring area with num (%d)...\n", areas[i].num);
            if (i == TREASURE_AREA) {
                printf("Treasure found in area with num (%d)!\n", areas[i].num);
                areas[i].found_treasure = 1;
                sem_post(sem_treasure); // сигнализируем о найденном кладе в родительский процесс
                exit(EXIT_SUCCESS);
            }
            exit(EXIT_FAILURE);
        }
    }
    // родительский процесс будет находится в ожидании, пока не найдется класс
    sem_wait(sem_treasure);
    // завершаем работу всех процессов
    for (i = 0; i < NUM_AREAS; i++) {
        wait(NULL);
    }
    // выводим результаты поисков с каждого участка острова (нашел/не нашел)
    printf("Results:\n");
    for (i = 0; i < NUM_AREAS; i++) {
        if (areas[i].found_treasure) {
            printf("Treasure found in area with num(%d)\n", areas[i].num);
        } else {
            printf("No treasure found in area with num(%d)\n", areas[i].num);
        }
    }
    // освобождение зарезрвированной виртуальной памяти
    munmap(areas, NUM_AREAS * sizeof(area_t));
    // удаляем именованные семафоры и разделяемую память
    shm_unlink("/treasure_hunt");
    sem_close(sem_treasure);
    sem_unlink("/treasure");
    return 0;
}
