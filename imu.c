#include "pod.h"

long maximumSafeForwardVelocity = 25; //CHANGE ME! ARBITRARY!
long standardDistanceBeforeBraking = 75; //CHANGE ME! ARBITRARY!
long maximumSafeDistanceBeforeBraking = 125;

// TODO: I'm assuming you can get all 6 values in one datagram
typedef struct {
  // 32-Bit
  unsigned long x;
  unsigned long y;
  unsigned long z;
  unsigned long wx;
  unsigned long wy;
  unsigned long wz;
} imu_datagram_t;

long acceleration = 0; //In m/s^2
long forwardVelocity = 0; //In m/s
long totalDistanceTraveled = 0; //In m

imu_datagram_t readIMUDatagram() {
  return (imu_datagram_t){ 1UL, 0UL, 0UL, 0UL, 0UL, 0UL };
}

/**
 * Checks to be performed when the pod's state is Pushing
 */
void pushingChecks(pod_state_t *podState) {
    if (getPodField(&(podState->position_x)) > maximumSafeDistanceBeforeBraking) {
        setPodMode(Emergency);
    }
    else if (getPodField(&(podState->velocity_x)) > maximumSafeForwardVelocity) {
        setPodMode(Emergency);
    }
    else if (getPodField(&(podState->accel_x)) <= 0) {
        setPodMode(Coasting);
    }
}

/**
 * Checks to be performed when the pod's state is Coasting
 */
void coastingChecks(pod_state_t *podState) {
    if (getPodField(&(podState->position_x)) > maximumSafeDistanceBeforeBraking || getPodField(&(podState->velocity_x)) > maximumSafeForwardVelocity) {
        setPodMode(Emergency);
    }
    else if (getPodField(&(podState->position_x)) > standardDistanceBeforeBraking) {
        setPodMode(Braking);
    }
}

/**
 * Checks to be performed when the pod's state is Braking
 */
void brakingChecks(pod_state_t *podState) {
    if (outside(PRIMARY_BRAKING_ACCEL_X_MIN, getPodField(&(podState->accel_x)), PRIMARY_BRAKING_ACCEL_X_MAX)) {
        setPodMode(Emergency);
    }
    else if (getPodField(&(podState->velocity_x)) <= 0) {
        setPodMode(Shutdown);
    }
}

void * imuMain(void *arg) {
    debug("[imuMain] Thread Start");

    pod_state_t *podState = getPodState();
    pod_mode_t podMode = getPodMode();

    unsigned long long lastCheckTime = getTime();

    while (getPodMode() != Shutdown) {

        imu_datagram_t imu_reading = readIMUDatagram();

        unsigned long long currentCheckTime = getTime(); //Same as above, assume milliseconds
        unsigned long long t = lastCheckTime - currentCheckTime;
        lastCheckTime = currentCheckTime;


        unsigned long position = getPodField(&(podState->position_x));
        unsigned long velocity = getPodField(&(podState->velocity_x));
        unsigned long acceleration = getPodField(&(podState->accel_x));

        // Calculate the new_velocity (oldv + (olda + newa) / 2)

        unsigned long new_velocity = (velocity + (t * ((acceleration + imu_reading.x) / 2)));
        unsigned long new_position = (position + (t * ((new_velocity + imu_reading.x) / 2)));

        setPodField(&(podState->position_x), new_position);
        setPodField(&(podState->velocity_x), new_velocity);
        setPodField(&(podState->accel_x), imu_reading.x);

        podMode = getPodMode();

        switch (podMode) {
            case Pushing:
                pushingChecks(podState);
                break;
            case Coasting:
                coastingChecks(podState);
                break;
            case Braking:
                brakingChecks(podState);
                break;
            default:
                break;
        }

        usleep(IMU_THREAD_SLEEP);
    }

    return NULL;
}
