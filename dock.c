/**
 * Dock - Loading/Unloading Cargo
 *
 * This program manages a dock with some anchorages to unload and load
 * cargo from and to many boats. It was made for an Operating Systems
 * class, to practice the use of threads and semaphores.
 *
 * (C) 2012 Helder Correia
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE. THE AUTHORS AND
 * CONTRIBUTORS ACCEPT NO RESPONSIBILITY IN ANY CONCEIVABLE MANNER.
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>

#define DOCKING_BAY 15            // maximum docking spots at bay

#define ANCHORAGES 3              // number of anchorages
#define ANCHORAGE_WAITING_SPOTS 2 // number of waiting spots per anchorage
#define ANCHORAGE_CARGO 145       // starting amount of cargo for each anchorage to load

#define BOATS 16                  // number of boats
#define BOAT_CARGO_CAPACITY 8     // maximum cargo capacity for each boat
#define MAXIMUM_SAIL_TIME 6       // maximum amount of time a boat will sail before returning to bay

/* Define new types for boat. */
typedef struct {
    int id;     // boat id
    int cargo;  // cargo in boat
} boat_t;

/* Define new type for anchorage. */
typedef struct {
    int id;             // anchorage id
    int cargo_to_load;  // remaining cargo to be loaded to boats 
    int cargo_unloaded; // total amount of cargo unloaded from boats
    int docked_boat;    // the id of the bock currently docked (-1 if none)
} anchorage_t;

// two threads for boats and anchorages
void *thr_boat(void*);
void *thr_anchorage(void*);

// helper functions
int dock_is_closed();
int cargo_in_boats();
int cargo_in_anchorages();
int enter_anchorage(int anchorage_id, boat_t *boat);
void wait_for_crane(int anchorage_id, boat_t *boat);
void leave_crane(int anchorage_id, int boat_id);
boat_t *boat_at_crane(int anchorage_id);
int anchorage_is_closed(int id);
void sail(int);

/* Semaphore to control available spots in docking bay. */
sem_t sem_dock;

/* Semaphore to control available waiting spots in an anchorage. */
sem_t sem_enter_anchorage[ANCHORAGES];

/* Semaphore to control when an anchorage's crane is available for moving cargo. */
sem_t sem_crane[ANCHORAGES];

/* Semaphore to control when unloading/loading cargo begins. */
sem_t sem_start_cargo[ANCHORAGES];

/* Semaphore to control when unloading/loading cargo ends. */
sem_t sem_end_cargo[ANCHORAGES];

/* Mutex for controlling the movement cargo. */
pthread_mutex_t mutex_cargo;

/* Mutex for controlling which boat is docked in an anchorage (crane). */
pthread_mutex_t mutex_docked_boat;

/* Global access to boats. */
boat_t boats[BOATS];

/* Global access to anchorages. */
anchorage_t anchorages[ANCHORAGES];

int main(int argc, char **argv)
{
    int i, total_cargo = 0;
    
    pthread_t boat_tid[BOATS];
    pthread_t anchorage_tid[ANCHORAGES];

    srand(time(NULL));

    pthread_mutex_init(&mutex_cargo, NULL);
    pthread_mutex_init(&mutex_docked_boat, NULL);
    
    sem_init(&sem_dock, 0, DOCKING_BAY);

    // create anchorage threads
    for (i = 0; i < ANCHORAGES; i++) {
        sem_init(&sem_enter_anchorage[i], 0, ANCHORAGE_WAITING_SPOTS); // controls available waiting spots in an anchorage
        sem_init(&sem_crane[i], 0, 1); // controls when an anchorage's crane is available to start moving cargo
        sem_init(&sem_start_cargo[i], 0, 0); // controls the start of unloading/loading cargo
        sem_init(&sem_end_cargo[i], 0, 0);   // controls the end of unloading/loading cargo

        anchorages[i].id = i;
        anchorages[i].docked_boat = -1;
        anchorages[i].cargo_unloaded = 0;
        anchorages[i].cargo_to_load = ANCHORAGE_CARGO;
        
        pthread_create(&anchorage_tid[i], NULL, thr_anchorage, (void *) &anchorages[i]);
    }
    
    // create boat threads
    for (i = 0; i < BOATS; i++) {
        boats[i].id = i;
        boats[i].cargo = rand() % BOAT_CARGO_CAPACITY + 1;
        
        pthread_create(&boat_tid[i], NULL, thr_boat, (void *) &boats[i]);
    }
    
    // wait for boat threads to return
    for (i = 0; i < BOATS; i++) {
        if (pthread_join(boat_tid[i], NULL) != 0) {
            perror("join boat thread");
        }
        else {
            printf("~~~~~ [B%02d] is back at sea. ~~~~~\n", i);
        }
    }
    
    puts("");
    
    // wait for anchorage threads to return
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
    
    sem_destroy(&sem_dock);
    pthread_mutex_destroy(&mutex_cargo);
    pthread_mutex_destroy(&mutex_docked_boat);
    
    exit(EXIT_SUCCESS);
}

/**
 * Anchorage thread.
 */
void *thr_anchorage(void *ptr)
{
    boat_t *boat;
    anchorage_t *anchorage = (anchorage_t*) ptr;

    int id = anchorage->id;

    while (!anchorage_is_closed(id)) {

        // wait for next boat to be ready for moving cargo
        sem_wait(&sem_start_cargo[id]);
        
        // which boat is at the crane right now?
        boat = boat_at_crane(id);
        
        // this should only happen when all boats have returned and this
        // anchorage is unblocked from the a waiting state
        if (boat == NULL) {
            sem_post(&sem_end_cargo[id]);
            continue;
        }
        
        /* ~~~~~ start critical area for unloading/loading ~~~~~~ */
        pthread_mutex_lock(&mutex_cargo);
        
        // unload the boat's cargo
        anchorage->cargo_unloaded += boat->cargo;
        boat->cargo = 0;

        // load boat with random cargo, up to full capacity
        if (anchorage->cargo_to_load > 0) {
            boat->cargo = rand() % BOAT_CARGO_CAPACITY + 1;
            if (boat->cargo > anchorage->cargo_to_load)
                boat->cargo = anchorage->cargo_to_load;
            
            anchorage->cargo_to_load -= boat->cargo;
        }
        printf(" [A%d] just unloaded [B%02d] and loaded %d more units.\n", id, boat->id, boat->cargo);

        pthread_mutex_unlock(&mutex_cargo);
        /* ~~~~~ end critical area for unloading/loading ~~~~~~ */

        sem_post(&sem_end_cargo[id]);
    }
    
    return NULL;
}

/**
 * Boat thread.
 */
void *thr_boat(void *ptr)
{
    boat_t *boat = (boat_t*) ptr;
    int i;
    
    while (!dock_is_closed()) {
        // if no available spots at bay, sail for a bit
        if (sem_trywait(&sem_dock) == 0) {
            printf("[B%02d] entered bay.\n", boat->id);

            // look for an available anchorage
            for (i = 0; !dock_is_closed(); i = (i+1) % ANCHORAGES) {
                if (enter_anchorage(i, boat)) {
                    
                    // wait until crane is available for moving cargo
                    wait_for_crane(i, boat);

                    // trigger crane to move cargo
                    printf("[B%02d] will start unloading (%d)/loading in [A%d].\n", boat->id, boat->cargo, i);
                    sem_post(&sem_start_cargo[i]);
                    
                    // at this point, the crane is being occupied,
                    // leaving the waiting spot vacant
                    sem_post(&sem_enter_anchorage[i]);

                    // wait until moving cargo is finished and leave
                    sem_wait(&sem_end_cargo[i]);

                    // now the crane is avaible for the next boat
                    leave_crane(i, boat->id);

                    printf("[B%02d] left [A%d].\n", boat->id, i);

                    break;
                }
                sleep(1); // wait 1 sec to avoid some race conditions
            }
            // boat leaving dock, so the spot is vacant
            sem_post(&sem_dock);
        }
        sail(boat->id);
    }
    
    printf(">>>>> The dock is closed. [B%02d] is on its way.\n", boat->id);
    
    return NULL;
}

/**
 * Check if a given anchorage is closed for business.
 */
int anchorage_is_closed(int id)
{
    return !anchorages[id].cargo_to_load && !cargo_in_boats();
}

/**
 * Check for any boat that has cargo to be unloaded.
 */
int cargo_in_boats()
{
    int i;
    for (i = 0; i < BOATS; i++)
        if (boats[i].cargo > 0)
            return 1;
    return 0;
}

/**
 * Check if the dock is closed or there's still cargo to move.
 */
int dock_is_closed()
{
    return !cargo_in_boats() && !cargo_in_anchorages();
}

/**
 * Check for any anchorage that has cargo to be loaded.
 */
int cargo_in_anchorages()
{
    int i;
    for (i = 0; i < ANCHORAGES; i++)
        if (anchorages[i].cargo_to_load > 0)
            return 1;
    return 0;
}

/**
 * Boat goes to one of an anchorage's waiting spots.
 */
int enter_anchorage(int anchorage_id, boat_t *boat)
{
    // first check if there's anything to do here.
    if (boat->cargo == 0 && anchorages[anchorage_id].cargo_to_load == 0)
        return 0;

    // attempt to enter an available waiting spot
    if (sem_trywait(&sem_enter_anchorage[anchorage_id]) == 0) {
        printf("[B%02d] entered [A%d].\n", boat->id, anchorage_id);
        return 1;
    }
    
    // no available spot in anchorage
    return 0;
}

/**
 * Wait for crane to be available and set boat to it.
 */
void wait_for_crane(int anchorage_id, boat_t *boat)
{
    sem_wait(&sem_crane[anchorage_id]);

    pthread_mutex_lock(&mutex_docked_boat);
    anchorages[anchorage_id].docked_boat = boat->id;
    pthread_mutex_unlock(&mutex_docked_boat);
}

/**
 * Leave crane and make it available for the next boat.
 */
void leave_crane(int anchorage_id, int boat_id)
{
    pthread_mutex_lock(&mutex_docked_boat);
    anchorages[anchorage_id].docked_boat = -1;
    pthread_mutex_unlock(&mutex_docked_boat);

    sem_post(&sem_crane[anchorage_id]);
}

/**
 * Return boat docked in a given anchorage (-1 if none).
 */
boat_t *boat_at_crane(int anchorage_id)
{
    int boat_id = anchorages[anchorage_id].docked_boat;
    
    if (boat_id == -1)
        return NULL;

    return &boats[boat_id];
}

/**
 * Boat sails for a random amount of time,
 * up to a maximum number of seconds.
 */
void sail(int boat_id)
{
    int sail_time = rand() % MAXIMUM_SAIL_TIME + 1;
    printf("[B%02d] will sail for %d secs.\n", sail_time);
    sleep(sail_time);
}

