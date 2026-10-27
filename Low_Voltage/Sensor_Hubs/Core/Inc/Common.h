#ifndef COMMON_H
#define COMMON_H

// CAN DEFINITIONS for 1 MBPS (Car)
#define CAN_CAR_Nominal_Prescaler 1
#define CAN_CAR_Nominal_TSeg_1    136
#define CAN_CAR_Nominal_TSeg_2    33
#define CAN_CAR_Nominal_SJW       33
#define CAN_CAR_Data_Prescaler    17
#define CAN_CAR_Data_SJW          3
#define CAN_CAR_Data_TSeg_1       6
#define CAN_CAR_Data_TSeg_2       3

#define QUEUE_LOW_PRIO  0
#define QUEUE_NORM_PRIO 1
#define QUEUE_MED_PRIO  2
#define QUEUE_HIGH_PRIO 3

typedef struct {
    uint8_t data[8];
    uint16_t id;
    uint32_t length;
} can_msg;

struct temp {
    uint32_t id;
    uint8_t data[8];
    uint32_t data_length;
    bool flag;
};

typedef struct {
    uint16_t lin_pot;
    uint16_t adc_2;
    uint16_t mon_5v;
} adc1_info;

typedef struct {
    uint16_t adc_3;
    uint16_t adc_4;
} adc2_info;

typedef struct board_mon_data {
    float shunt_voltage;
    float current;
    float bus_voltage;
    float power;
    float temperature;
} mon_i2c_data;

float wss;

#define COPY_STRUCT(dest_ptr, src_ptr) genericCopy(dest_ptr, src_ptr, sizeof(*(dest_ptr)))

void static inline genericCopy(void* dest_ptr, const void* src_ptr, size_t size) {
    configASSERT(dest_ptr != NULL && src_ptr != NULL);
    memcpy(dest_ptr, src_ptr, size);
}

void static inline copyWithMutexSized(osMutexId_t mutex, void* dest_ptr, const void* src_ptr,
                                      size_t size) {
    osMutexAcquire(mutex, osWaitForever);
    genericCopy(dest_ptr, src_ptr, size);
    osMutexRelease(mutex);
}

// Macro so sizeof sees the caller's real type; fails to compile if dest and src sizes differ
#define copyWithMutex(mutex, dest_ptr, src_ptr)        \
    copyWithMutexSized((mutex), (dest_ptr), (src_ptr), \
                       sizeof(*(dest_ptr)) +           \
                           (0 * sizeof(char[sizeof(*(dest_ptr)) == sizeof(*(src_ptr)) ? 1 : -1])))

void static inline bitwiseAndWithMutex(osMutexId_t mutex, uint8_t* dest_ptr, uint8_t bit) {
    osMutexAcquire(mutex, osWaitForever);
    *dest_ptr &= ~(1 << bit);
    osMutexRelease(mutex);
}

void static inline bitwiseOrWithMutex(osMutexId_t mutex, uint8_t* dest_ptr, uint8_t bit) {
    osMutexAcquire(mutex, osWaitForever);
    *dest_ptr |= (1 << bit);
    osMutexRelease(mutex);
}

#endif
