#include <types.h>
#include <lib.h>
#include <synchprobs.h>
#include <synch.h>
#include <opt-A1.h>

/*
 * This simple default synchronization mechanism allows only vehicle at a time
 * into the intersection.   The intersectionSem is used as a a lock.
 * We use a semaphore rather than a lock so that this code will work even
 * before locks are implemented.
 */

/*
 * Replace this default synchronization mechanism with your own (better) mechanism
 * needed for your solution.   Your mechanism may use any of the available synchronzation
 * primitives, e.g., semaphores, locks, condition variables.   You are also free to
 * declare other global variables if your solution requires them.
 */

/*
 * replace this with declarations of any synchronization and other variables you need here
 */
static struct lock * intersectionLk;

static struct cv * cvs[12];

/* 12 pairs of orgin & destination
 * 0  ---  NE
 * 1  ---  NS
 * 2  ---  NW
 * 3  ---  ES
 * 4  ---  EW
 * 5  ---  EN
 * 6  ---  SW
 * 7  ---  SN
 * 8  ---  SE
 * 9  ---  WN
 * 10 ---  WE
 * 11 ---  WS
 */

volatile int ct_dir[12];

bool block[12][12] = {{false}};

/*
 * The simulation driver will call this function once before starting
 * the simulation
 *
 * You can use it to initialize synchronization and other variables.
 *
 */
void
intersection_sync_init(void)
{
    /* replace this default implementation with your own implementation */
    
    /*intersectionSem = sem_create("intersectionSem",1);
     if (intersectionSem == NULL) {
     panic("could not create intersection semaphore");
     }
     return;*/
    intersectionLk = lock_create("intersectionLock");
    if (intersectionLk == NULL) {
        panic("Could not create intersection lock");
    }
    
    for (int i = 0; i < 12; i++) {
        if (i == 0) {
            cvs[i] = cv_create("NtoE");
        } else if (i == 1) {
            cvs[i] = cv_create("NtoS");
        } else if (i == 2) {
            cvs[i] = cv_create("NtoW");
        } else if (i == 3) {
            cvs[i] = cv_create("EtoS");
        } else if (i == 4) {
            cvs[i] = cv_create("EtoW");
        } else if (i == 5) {
            cvs[i] = cv_create("EtoN");
        } else if (i == 6) {
            cvs[i] = cv_create("StoW");
        } else if (i == 7) {
            cvs[i] = cv_create("StoN");
        } else if (i == 8) {
            cvs[i] = cv_create("StoE");
        } else if (i == 9) {
            cvs[i] = cv_create("WtoN");
        } else if (i == 10) {
            cvs[i] = cv_create("WtoE");
        } else if (i == 11) {
            cvs[i] = cv_create("WtoS");
        }
        
        if (cvs[i] == NULL) {
            panic("Could not create cvs");
        }
        
        ct_dir[i] = 0;
    }
    
    return;
}

/*
 * The simulation driver will call this function once after
 * the simulation has finished
 *
 * You can use it to clean up any synchronization and other variables.
 *
 */

void
intersection_sync_cleanup(void)
{
    /* replace this default implementation with your own implementation */
    /*KASSERT(intersectionSem != NULL);
     sem_destroy(intersectionSem);*/
    
    KASSERT(intersectionLk != NULL);
    for (int i = 0; i < 12; i++) {
        KASSERT(cvs[i] != NULL);
    }
    for (int i = 0; i < 12; i++) {
        cv_destroy(cvs[i]);
    }
    
    lock_destroy(intersectionLk);
}

/*
 * Set block true if two trip cannot happen at the same time.
 *
 * parameters:
 *    none
 *
 * return value:
 *    none
 */

static void set_block(void) {
    block[0][3] = block[0][4] = block[0][6] = block[0][7] = block[0][8] = block[0][9] = block[0][10] = true;
    block[1][3] = block[1][4] = block[1][6] = block[1][9] = block[1][10] = block[1][11] = true;
    block[2][4] = block[2][6] = true;
    block[3][0] = block[3][1] = block[3][6] = block[3][7] = block[3][9] = block[3][10] = block[3][11] = true;
    block[4][0] = block[4][1] = block[4][2] = block[4][6] = block[4][7] = block[4][9] = true;
    block[5][7] = block[5][9] = true;
    block[6][0] = block[6][1] = block[6][2] = block[6][3] = block[6][4] = block[6][9] = block[6][10] = true;
    block[7][0] = block[7][3] = block[7][4] = block[7][5] = block[7][9] = block[7][10] = true;
    block[8][0] = block[8][10] = true;
    block[9][0] = block[9][1] = block[9][3] = block[9][4] = block[9][5] = block[9][6] = block[9][7] = true;
    block[10][0] = block[10][1] = block[10][3] = block[10][6] = block[10][7] = block[10][8] = true;
    block[11][1] = block[11][3] = true;
}

/*
 * Get the direction number for current vehicle
 *
 * parameters:
 *    * origin
 *    * destination
 *
 * return value:
 *    int: direction number
 */
static int get_dir(Direction origin, Direction destination) {
    int dir = -1;
    if (origin == north && destination == east) {
        dir = 0;
    } else if (origin == north && destination == south) {
        dir = 1;
    } else if (origin == north && destination == west) {
        dir = 2;
    } else if (origin == east && destination == south) {
        dir = 3;
    } else if (origin == east && destination == west) {
        dir = 4;
    } else if (origin == east && destination == north) {
        dir = 5;
    } else if (origin == south && destination == west) {
        dir = 6;
    } else if (origin == south && destination == north) {
        dir = 7;
    } else if (origin == south && destination == east) {
        dir = 8;
    } else if (origin == west && destination == north) {
        dir = 9;
    } else if (origin == west && destination == east) {
        dir = 10;
    } else if (origin == west && destination == south) {
        dir = 11;
    }
    return dir;
}


/*
 * The simulation driver will call this function each time a vehicle
 * tries to enter the intersection, before it enters.
 * This function should cause the calling simulation thread
 * to block until it is OK for the vehicle to enter the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle is arriving
 *    * destination: the Direction in which the vehicle is trying to go
 *
 * return value: none
 */

void
intersection_before_entry(Direction origin, Direction destination)
{
    /* replace this default implementation with your own implementation */
    /*(void)origin;*/  /* avoid compiler complaint about unused parameter */
    /*(void)destination;*/ /* avoid compiler complaint about unused parameter */
    /*KASSERT(intersectionSem != NULL);
     P(intersectionSem);*/
    
    KASSERT(intersectionLk != NULL);
    lock_acquire(intersectionLk);
    
    set_block();
    
    while (true) {
        int dir = get_dir(origin, destination);
        if (dir < 0) {
            panic("Invalid direction.");
        }
        
        if (ct_dir[dir] == 0) {
            for (int j = 0; j < 12; j++) {
                if (block[dir][j]) {
                    ct_dir[j] += 1;
                }
            }
            break;
        } else {
            cv_wait(cvs[dir], intersectionLk);
        }
    }
    
    lock_release(intersectionLk);
    
}


/*
 * The simulation driver will call this function each time a vehicle
 * leaves the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle arrived
 *    * destination: the Direction in which the vehicle is going
 *
 * return value: none
 */

void
intersection_after_exit(Direction origin, Direction destination)
{
    /* replace this default implementation with your own implementation */
    /*(void)origin;*/  /* avoid compiler complaint about unused parameter */
    /*(void)destination;*/ /* avoid compiler complaint about unused parameter */
    /*KASSERT(intersectionSem != NULL);
     V(intersectionSem);*/
    (void)destination;
    KASSERT(intersectionLk != NULL);
    lock_acquire(intersectionLk);
    
    set_block();
    
    int dir = get_dir(origin, destination);
    if (dir < 0) {
        panic("Invalid direction.");
    }
    
    for (int j = 0; j < 12; j++) {
        if (block[dir][j]) {
            ct_dir[j] -= 1;
        }
        if (ct_dir[j] == 0) {
            cv_broadcast(cvs[j], intersectionLk);
        }
    }
    
    lock_release(intersectionLk);
}
