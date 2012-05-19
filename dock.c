#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>

#define DOCKING_BAY 15

#define ANCHORAGES 3
#define ANCHORAGE_WAITING_SPOTS 2
#define ANCHORAGE_CARGO 145

#define BOATS 16
#define BOAT_CARGO_CAPACITY 8
#define MAXIMUM_SAIL_TIME 6

#define ANCHORAGE_UNAVAILABLE -1

// define new types for boats and anchorages
typedef struct {
    int id;
    int cargo;
} boat_t;

typedef struct {
    int id;
    int cargo_to_load;
    int cargo_unloaded;
    int docked_boat;
} anchorage_t;

// two threads for boats and anchorages
void *thr_boat(void*);
void *thr_anchorage(void*);


// declare semaphores
sem_t sem_dock,  // controls spots to be occupied
      sem_enter_anchorage[ANCHORAGES],
      sem_crane[ANCHORAGES],
      sem_start_cargo[ANCHORAGES],
      sem_end_cargo[ANCHORAGES];

pthread_mutex_t mutex;

boat_t boats[BOATS];
anchorage_t anchorages[ANCHORAGES];

int main(int argc, char **argv)
{
    int i, total_cargo = 0;
    
    pthread_t boat_tid[BOATS];
    pthread_t anchorage_tid[ANCHORAGES];

    srand(time(NULL));

    pthread_mutex_init(&mutex, NULL);
    
    sem_init(&sem_dock, 0, DOCKING_BAY);

    for (i = 0; i < ANCHORAGES; i++) {
        sem_init(&sem_enter_anchorage[i], 0, ANCHORAGE_WAITING_SPOTS);
        sem_init(&sem_crane[i], 0, 1);
        sem_init(&sem_start_cargo[i], 0, 0);
        sem_init(&sem_end_cargo[i], 0, 0);

        anchorages[i].id = i;
        anchorages[i].cargo_unloaded = 0;
        anchorages[i].cargo_to_load = ANCHORAGE_CARGO;
        
        pthread_create(&anchorage_tid[i], NULL, thr_anchorage, (void *) &anchorages[i]);
    }
    
    for (i = 0; i < BOATS; i++) {
        boats[i].id = i;
        boats[i].cargo = rand() % BOAT_CARGO_CAPACITY + 1;
        
        pthread_create(&boat_tid[i], NULL, thr_boat, (void *) &boats[i]);
    }
    
    for (i = 0; i < BOATS; i++) {
        if (pthread_join(boat_tid[i], NULL) != 0) {
            perror("join boat thread");
        }
        else {
            printf("~~~~~ [B%02d] is back at sea. ~~~~~\n", i);
        }
    }
    
    puts("");
    
    for (i = 0; i < ANCHORAGES; i++) {
        // attempt to unblock any anchorage in a waiting state
        sem_post(&sem_start_cargo[i]);
        
        if (pthread_join(anchorage_tid[i], NULL) != 0) {
            perror("join anchorage thread");
        }
        else {
            printf("[A%02d] closed with %d units of unloaded cargo.\n", i, anchorages[i].cargo_unloaded);
            total_cargo += anchorages[i].cargo_unloaded;
            
            sem_destroy(&sem_enter_anchorage[i]);
            sem_destroy(&sem_crane[i]);
            sem_destroy(&sem_start_cargo[i]);
            sem_destroy(&sem_end_cargo[i]);
        }
    }

    printf("\n  >>> All ships are at sea, captain!\n");
    printf("\nTotal unloaded cargo: %d units\n", total_cargo);
    
    pthread_mutex_destroy(&mutex);
    
    exit(EXIT_SUCCESS);
}

void sail(int boat_id)
{
    int sail_time = rand() % MAXIMUM_SAIL_TIME + 1;
    printf("[B%02d] will sail for %d secs.\n", sail_time);
    sleep(sail_time);
}

int enter_anchorage(int anchorage_id, boat_t *boat)
{
    if (boat->cargo == 0 && anchorages[anchorage_id].cargo_to_load == 0)
        return 0;

    if (sem_trywait(&sem_enter_anchorage[anchorage_id]) == 0) {
        pthread_mutex_lock(&mutex);
        anchorages[anchorage_id].docked_boat = boat->id;
        printf("[B%02d] entered [A%d].\n", boat->id, anchorage_id);
        pthread_mutex_unlock(&mutex);

        return 1;
    }
    return 0;
}

void leave_anchorage(int anchorage_id, int boat_id)
{
    pthread_mutex_lock(&mutex);
    anchorages[anchorage_id].docked_boat = -1;
    printf("[B%02d] left [A%d].\n", boat_id, anchorage_id);
    pthread_mutex_unlock(&mutex);
}

boat_t *boat_in_anchorage(int anchorage_id)
{
    //pthread_mutex_lock(&mutex);
    int boat_id = anchorages[anchorage_id].docked_boat;
    //pthread_mutex_unlock(&mutex);
    
    if (boat_id == -1)
        return NULL;

    return &boats[boat_id];
}

int cargo_in_boats()
{
    int i;
    for (i = 0; i < BOATS; i++)
        if (boats[i].cargo > 0)
            return 1;
    return 0;
}

int cargo_in_anchorages()
{
    int i;
    for (i = 0; i < ANCHORAGES; i++)
        if (anchorages[i].cargo_to_load > 0)
            return 1;
    return 0;
}

int dock_is_closed()
{
    return !cargo_in_boats() && !cargo_in_anchorages();
}

int anchorage_is_closed(int id)
{
    return anchorages[id].cargo_to_load == 0 && !cargo_in_boats();
}

void *thr_boat(void *ptr)
{
    boat_t *boat = (boat_t*) ptr;
    int i;
    
    while (!dock_is_closed()) {
        if (sem_trywait(&sem_dock) == 0) {
            printf("[B%02d] entered bay.\n", boat->id);

            for (i = 0; !dock_is_closed(); i = (i+1) % ANCHORAGES) {
                if (enter_anchorage(i, boat)) {
                        
                    sem_wait(&sem_crane[i]);

                    printf("[B%02d] will start unloading (%d)/loading in [A%d].\n", boat->id, boat->cargo, i);
                    sem_post(&sem_start_cargo[i]);
                    
                    // at this point, the crane is being occupied,
                    // leaving the waiting spot vacant
                    sem_post(&sem_enter_anchorage[i]);

                    sem_wait(&sem_end_cargo[i]);
                    leave_anchorage(i, boat->id);
                    sem_post(&sem_crane[i]);

                    break;
                }
                sleep(1);
            }
            // boat leaving dock, so the spot is vacant
            sem_post(&sem_dock);
            printf("[B%02d] is done and will leave now.\n", boat->id);
        }
        sail(boat->id);
    }
    
    printf(">>>>> The dock is closed. [B%02d] is on its way.\n", boat->id);
    
    return NULL;
}

void *thr_anchorage(void *ptr)
{
    boat_t *boat;
    anchorage_t *anchorage = (anchorage_t*) ptr;

    int id = anchorage->id;

    sem_wait(&sem_start_cargo[id]);
    
    while (!anchorage_is_closed(id)) {
        
        boat = boat_in_anchorage(id);
        
        if (boat == NULL) {
            printf(" ???  [A%d] which boat? Nothing to do.\n", id);
            sem_post(&sem_end_cargo[id]);
            sem_wait(&sem_start_cargo[id]);
            continue;
        }
        
        pthread_mutex_lock(&mutex);
        
        // unload the boat's cargo
        anchorage->cargo_unloaded += boat->cargo;

        // load boat with random cargo, up to full capacity
        boat->cargo = rand() % BOAT_CARGO_CAPACITY + 1;
        if (boat->cargo > anchorage->cargo_to_load)
            boat->cargo = anchorage->cargo_to_load;
        
        anchorage->cargo_to_load -= boat->cargo;
        
        printf(" [A%d] just unloaded [B%02d] and loaded %d more units.\n", id, boat->id, boat->cargo);
        pthread_mutex_unlock(&mutex);

        sem_post(&sem_end_cargo[id]);
        sem_wait(&sem_start_cargo[id]);
    }
    
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

