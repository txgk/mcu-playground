#ifndef LIS3DH_H
#define LIS3DH_H
void lis3dh_initialize(void);
int lis3dh_read_device_id(void);
void lis3dh_read_acceleration(uint16_t *x, uint16_t *y, uint16_t *z);
#endif // LIS3DH_H
