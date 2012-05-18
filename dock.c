#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>

#define ANCHORAGES 3
#define WAITING_SPOTS 2
#define DOCKING_BAY 15

#define BOATS 16

#define ANCHORAGE_CARGO 145
#define BOAT_CARGO_CAPACITY 8

#define MAXIMUM_SAIL_TIME 6

void* thr_boat(void* ptr);
void* thr_anchorage(void* ptr);

typedef struct {
    int is_closed;
    int boats_at_bay;
    int anchorage_cargo[ANCHORAGES];
} dock_t;

typedef struct {
    int id;
    int cargo;
} boat_t;

sem_t sem_dock,  // controls spots to be occupied
      sem_enter_anchorage, // controls boat docked in anchorage
      sem_leave_anchorage,
      sem_anchorage,
      sem_crane,
      sem_start_cargo,
      sem_end_cargo;

pthread_mutex_t mutex_dock,
                mutex_boats;

dock_t dock;
boat_t boat[BOATS];

int main(int argc, char *argv[])
{
    int boat_i;
    long anchorage_cargo;
    long total_cargo;
    
    pthread_t boat_tid[BOATS];
    pthread_t anchorage_tid;

    pthread_mutex_init(&mutex_dock, NULL);
    pthread_mutex_init(&mutex_boats, NULL);
    
    srand(time(NULL));

    dock.is_closed = 0;
    dock.boats_at_bay = 0;
    
    sem_init(&sem_dock, 0, DOCKING_BAY);
    sem_init(&sem_enter_anchorage, 0, WAITING_SPOTS);
    sem_init(&sem_crane, 0, 1);
    //sem_init(&sem_leave_anchorage, 0, 0);
    sem_init(&sem_start_cargo, 0, 0);
    sem_init(&sem_end_cargo, 0, 0);
    
    for (boat_i = 0; boat_i < BOATS; boat_i++) {
        boat[boat_i].id = boat_i + 1;
        boat[boat_i].cargo = rand() % BOAT_CARGO_CAPACITY + 1;
        
        pthread_create(&boat_tid[boat_i], NULL, thr_boat, (void *) &boat[boat_i]);
    }
    
    pthread_create(&anchorage_tid, NULL, thr_anchorage, NULL);
    
    if (pthread_join(anchorage_tid, NULL) != 0) {
        perror("join anchorage thread");
    }

    pthread_mutex_lock(&mutex_dock);    
    dock.is_closed = 1;
    puts("~~~~~~~ The dock is closed! ~~~~~~~");
    pthread_mutex_unlock(&mutex_dock);
    
    for (boat_i = 0; boat_i < BOATS; boat_i++) {
        if (pthread_join(boat_tid[boat_i], NULL) != 0) {
            perror("join boat thread");
        }
        else {
            printf("Boat %2d is back at sea.\n", boat_i+1);
        }
    }
    
    puts("  >>> All ships are at sea, captain!");
    
    sem_destroy(&sem_dock);
    sem_destroy(&sem_crane);
    sem_destroy(&sem_start_cargo);
    sem_destroy(&sem_end_cargo);

    pthread_mutex_destroy(&mutex_dock);
    pthread_mutex_destroy(&mutex_boats);
    
    exit(EXIT_SUCCESS);
}

void enter_bay(int boat_id)
{
    pthread_mutex_lock(&mutex_dock);
    dock.boats_at_bay++;
    pthread_mutex_unlock(&mutex_dock);
    printf("Boat %2d entered bay.\n", boat_id);
}

void exit_bay(int boat_id)
{
    pthread_mutex_lock(&mutex_dock);
    dock.boats_at_bay--;
    pthread_mutex_unlock(&mutex_dock);
    printf("Boat %2d exited bay.\n", boat_id);
}

void sail(int boat_id)
{
    int sail_time = rand() % MAXIMUM_SAIL_TIME + 1;
    printf("Boat %2d will sail for %d secs.\n", boat_id, sail_time);
    sleep(sail_time);
}

void *thr_boat(void *ptr)
{
    boat_t boat = *(boat_t*) ptr;
    
    while (!dock.is_closed) {
        if (sem_trywait(&sem_dock) == 0) {
            enter_bay(boat.id);
            
            while (1) {
                //sleep(1);
                sleep(boat.id % WAITING_SPOTS);

                if (sem_trywait(&sem_enter_anchorage) == 0) {
                    printf("Boat %2d entered anchorage.\n", boat.id);

                    sem_wait(&sem_crane);
                    printf("Boat %2d will start loading/unloading.\n", boat.id);
                    
                    sem_post(&sem_start_cargo);
                    sem_post(&sem_enter_anchorage);
                    
                    sem_wait(&sem_end_cargo);
                    printf("Boat %2d finished loading/unloading.\n", boat.id);
                    
                    sem_post(&sem_crane);
                    printf("Boat %2d will leave now.\n", boat.id);
                    
                    sem_post(&sem_dock);
                    exit_bay(boat.id);

                    break;
                }
                else {
                    //sleep(1);
                }
            }
        }
        sail(boat.id);
    }
    
    printf(">>>> The dock is closed. Boat %2d is on its way.\n", boat.id);
    
    /*
    Algoritmo thread barco
      Enquanto (!fechouDoca) {
        entraNaBaia();
        entraAncoradouro();
        esperaFimDescargaCarga();
        saiDoAncoradouro();
        saiDaBaia();
        Navega(); // tempo variável
      Fim Enquanto
    Fim Algoritmo
    */
    
    return NULL;
}

int boats_at_bay()
{
    int available_spots;
    sem_getvalue(&sem_dock, &available_spots);
    
    return available_spots < DOCKING_BAY;
}

void *thr_anchorage(void *ptr)
{
    int cargo_to_load = ANCHORAGE_CARGO;
    int cargo_unloaded = 0;
    int cargo;
    
    while (1) {
        sem_wait(&sem_start_cargo);

        puts(">>> Loading/Unloading boat...");
        cargo_unloaded += rand() % BOAT_CARGO_CAPACITY + 1;

        if (cargo_to_load > 0) {
            cargo_to_load -= rand() % BOAT_CARGO_CAPACITY + 1;
            
            if (cargo_to_load < 0) {
                cargo_to_load = 0;
            }
        }
        
        sem_post(&sem_end_cargo);
        
        if (cargo_to_load == 0) {
            break;
        }
    }
    
    printf("Unloaded %d cargo units.\n", cargo_unloaded);

    /* 
    Algoritmo thread ancoradouro
      Enquanto (existemBarcosBaia)
        esperaAtracarBarco();
        descarregaCarregaBarco();
        desatracarBarco();
        // serve como parâmetro para fechar ancoradouro
        cargaACarregar -= y;
        cargaRecebida + = z;
      Fim Enquanto
    Fim Algoritmo
    */
    
    return NULL;
}

