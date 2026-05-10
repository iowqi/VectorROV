#include "motor.h"
#include "cmsis_os.h"

motor_t dmotors[4];

int float_to_uint(float x_float, float x_min, float x_max, int bits) {
    /* Converts a float to an unsigned int, given range and number of bits */
    float span = x_max - x_min;
    float offset = x_min;
    return (int)((x_float - offset) * ((float)((1 << bits) - 1)) / span);
}

float uint_to_float(int x_int, float x_min, float x_max, int bits) {
    /* converts unsigned int to float, given range and number of bits */
    float span = x_max - x_min;
    float offset = x_min;
    return ((float)x_int) * span / ((float)((1 << bits) - 1)) + offset;
}

void dm_motor_fbdata(motor_t *motor, uint8_t *rx_data) {
    motor->para.id = (rx_data[0]) & 0x0F;
    motor->para.state = (rx_data[0]) >> 4;
    motor->para.p_int = (rx_data[1] << 8) | rx_data[2];
    motor->para.v_int = (rx_data[3] << 4) | (rx_data[4] >> 4);
    motor->para.t_int = ((rx_data[4] & 0xF) << 8) | rx_data[5];
    motor->para.pos = uint_to_float(motor->para.p_int, -motor->tmp.PMAX, motor->tmp.PMAX, 16); // (-12.5,12.5)
    motor->para.vel = uint_to_float(motor->para.v_int, -motor->tmp.VMAX, motor->tmp.VMAX, 12); // (-45.0,45.0)
    motor->para.tor = uint_to_float(motor->para.t_int, -motor->tmp.TMAX, motor->tmp.TMAX, 12); // (-18.0,18.0)
    motor->para.Tmos = (float)(rx_data[6]);
    motor->para.Tcoil = (float)(rx_data[7]);
}

void mit_ctrl(hcan_t *hcan, motor_t *motor, uint16_t motor_id, float pos, float vel, float kp, float kd, float tor) {
    uint8_t data[8];
    uint16_t pos_tmp, vel_tmp, kp_tmp, kd_tmp, tor_tmp;
    uint16_t id = motor_id + MIT_MODE;

    pos_tmp = float_to_uint(pos, -motor->tmp.PMAX, motor->tmp.PMAX, 16);
    vel_tmp = float_to_uint(vel, -motor->tmp.VMAX, motor->tmp.VMAX, 12);
    tor_tmp = float_to_uint(tor, -motor->tmp.TMAX, motor->tmp.TMAX, 12);
    kp_tmp = float_to_uint(kp, KP_MIN, KP_MAX, 12);
    kd_tmp = float_to_uint(kd, KD_MIN, KD_MAX, 12);

    data[0] = (pos_tmp >> 8);
    data[1] = pos_tmp;
    data[2] = (vel_tmp >> 4);
    data[3] = ((vel_tmp & 0xF) << 4) | (kp_tmp >> 8);
    data[4] = kp_tmp;
    data[5] = (kd_tmp >> 4);
    data[6] = ((kd_tmp & 0xF) << 4) | (tor_tmp >> 8);
    data[7] = tor_tmp;

    fdcanx_send_data(hcan, id, data, 8);
}

void pos_ctrl(hcan_t *hcan, uint16_t motor_id, float pos, float vel) {
    uint16_t id;
    uint8_t *pbuf, *vbuf;
    uint8_t data[8];

    id = motor_id + POS_MODE;
    pbuf = (uint8_t *)&pos;
    vbuf = (uint8_t *)&vel;

    data[0] = *pbuf;
    data[1] = *(pbuf + 1);
    data[2] = *(pbuf + 2);
    data[3] = *(pbuf + 3);

    data[4] = *vbuf;
    data[5] = *(vbuf + 1);
    data[6] = *(vbuf + 2);
    data[7] = *(vbuf + 3);

    fdcanx_send_data(hcan, id, data, 8);
}

void spd_ctrl(hcan_t *hcan, uint16_t motor_id, float vel) {
    uint16_t id;
    uint8_t *vbuf;
    uint8_t data[4];

    id = motor_id + SPD_MODE;
    vbuf = (uint8_t *)&vel;

    data[0] = *vbuf;
    data[1] = *(vbuf + 1);
    data[2] = *(vbuf + 2);
    data[3] = *(vbuf + 3);

    fdcanx_send_data(hcan, id, data, 4);
}

void psi_ctrl(hcan_t *hcan, uint16_t motor_id, float pos, float vel, float cur) {
    uint16_t id;
    uint8_t *pbuf, *vbuf, *ibuf;
    uint8_t data[8];

    uint16_t u16_vel = vel * 100;
    uint16_t u16_cur = cur * 10000;

    id = motor_id + PSI_MODE;
    pbuf = (uint8_t *)&pos;
    vbuf = (uint8_t *)&u16_vel;
    ibuf = (uint8_t *)&u16_cur;

    data[0] = *pbuf;
    data[1] = *(pbuf + 1);
    data[2] = *(pbuf + 2);
    data[3] = *(pbuf + 3);

    data[4] = *vbuf;
    data[5] = *(vbuf + 1);

    data[6] = *ibuf;
    data[7] = *(ibuf + 1);

    fdcanx_send_data(hcan, id, data, 8);
}

void enable_motor_mode(hcan_t *hcan, uint16_t motor_id, uint16_t mode_id) {
    uint8_t data[8];
    uint16_t id = motor_id + mode_id;

    data[0] = 0xFF;
    data[1] = 0xFF;
    data[2] = 0xFF;
    data[3] = 0xFF;
    data[4] = 0xFF;
    data[5] = 0xFF;
    data[6] = 0xFF;
    data[7] = 0xFC;

    fdcanx_send_data(hcan, id, data, 8);
}

void disable_motor_mode(hcan_t *hcan, uint16_t motor_id, uint16_t mode_id) {
    uint8_t data[8];
    uint16_t id = motor_id + mode_id;

    data[0] = 0xFF;
    data[1] = 0xFF;
    data[2] = 0xFF;
    data[3] = 0xFF;
    data[4] = 0xFF;
    data[5] = 0xFF;
    data[6] = 0xFF;
    data[7] = 0xFD;

    fdcanx_send_data(hcan, id, data, 8);
}

void save_pos_zero(hcan_t *hcan, uint16_t motor_id, uint16_t mode_id) {
    uint8_t data[8];
    uint16_t id = motor_id + mode_id;

    data[0] = 0xFF;
    data[1] = 0xFF;
    data[2] = 0xFF;
    data[3] = 0xFF;
    data[4] = 0xFF;
    data[5] = 0xFF;
    data[6] = 0xFF;
    data[7] = 0xFE;

    fdcanx_send_data(hcan, id, data, 8);
}

void clear_err(hcan_t *hcan, uint16_t motor_id, uint16_t mode_id) {
    uint8_t data[8];
    uint16_t id = motor_id + mode_id;

    data[0] = 0xFF;
    data[1] = 0xFF;
    data[2] = 0xFF;
    data[3] = 0xFF;
    data[4] = 0xFF;
    data[5] = 0xFF;
    data[6] = 0xFF;
    data[7] = 0xFB;

    fdcanx_send_data(hcan, id, data, 8);
}

// 通过判断 motor 结构体中的控制模式来选择使能的模式，并添加偏移的ID
void dm_motor_enable(hcan_t *hcan, motor_t *motor) {
    switch (motor->ctrl.mode) {
    case MIT_MODE:
        enable_motor_mode(hcan, motor->id, MIT_MODE);
        break;
    case POS_MODE:
        enable_motor_mode(hcan, motor->id, POS_MODE);
        break;
    case SPD_MODE:
        enable_motor_mode(hcan, motor->id, SPD_MODE);
        break;
    case PSI_MODE:
        enable_motor_mode(hcan, motor->id, PSI_MODE);
        break;
    }
}

// 通过判断 motor 结构体中的控制模式来选择失能的模式，并添加偏移的ID
void dm_motor_disable(hcan_t *hcan, motor_t *motor) {
    switch (motor->ctrl.mode) {
    case MIT_MODE:
        disable_motor_mode(hcan, motor->id, MIT_MODE);
        break;
    case POS_MODE:
        disable_motor_mode(hcan, motor->id, POS_MODE);
        break;
    case SPD_MODE:
        disable_motor_mode(hcan, motor->id, SPD_MODE);
        break;
    case PSI_MODE:
        disable_motor_mode(hcan, motor->id, PSI_MODE);
        break;
    }
    // dm_motor_clear_para(motor);
}

// 通过判断 motor 结构体中的控制模式来选择清除错误的模式，并添加偏移的ID
void dm_motor_clear_err(hcan_t *hcan, motor_t *motor) {
    switch (motor->ctrl.mode) {
    case MIT_MODE:
        clear_err(hcan, motor->id, MIT_MODE);
        break;
    case POS_MODE:
        clear_err(hcan, motor->id, POS_MODE);
        break;
    case SPD_MODE:
        clear_err(hcan, motor->id, SPD_MODE);
        break;
    case PSI_MODE:
        clear_err(hcan, motor->id, PSI_MODE);
        break;
    }
}

// 封装模式控制函数
void dm_motor_ctrl_send(hcan_t *hcan, motor_t *motor) {
    switch (motor->ctrl.mode) {
    case MIT_MODE:
        mit_ctrl(hcan, motor, motor->id, motor->ctrl.pos_set, motor->ctrl.vel_set, motor->ctrl.kp_set, motor->ctrl.kd_set, motor->ctrl.tor_set);
        break;
    case POS_MODE:
        pos_ctrl(hcan, motor->id, motor->ctrl.pos_set, motor->ctrl.vel_set);
        break;
    case SPD_MODE:
        spd_ctrl(hcan, motor->id, motor->ctrl.vel_set);
        break;
    case PSI_MODE:
        psi_ctrl(hcan, motor->id, motor->ctrl.pos_set, motor->ctrl.vel_set, motor->ctrl.cur_set);
        break;
    }
}

uint8_t dm_motor_multi_home_and_rezero(hcan_t *hcan, uint8_t motor_num, const float *home_dir, const float *home_offset_deg,
                                       float pos_step_abs, float tor_th, float pos_resp_vel, uint16_t settle_ms) {
    uint8_t i;
    uint8_t done_cnt = 0;
    uint8_t zero_state[4] = {0}; // 0:转动中 1:发失能 2:发记零 3:发使能 4:完成
    float cmd_pos[4] = {0.0f};
    float home_offset_rad[4] = {0.0f};
    const float deg2rad = 0.0174532925f;

    if ((hcan == 0) || (home_dir == 0) || (home_offset_deg == 0)) {
        return 1;
    }
    if ((motor_num == 0) || (motor_num > 4)) {
        return 2;
    }

    for (i = 1; i <= motor_num; i++) {
        enable_motor_mode(hcan, i, POS_MODE);
        osDelay(10);
    }

    for (i = 0; i < motor_num; i++) {
        if (dmotors[i].para.state != 1) {
            return 3;
        }
        dmotors[i].tmp.PMAX = 12.5f;
        dmotors[i].tmp.VMAX = 280.0f;
        dmotors[i].tmp.TMAX = 1.0f;
        save_pos_zero(hcan, i + 1, POS_MODE);
        osDelay(2);
        cmd_pos[i] = dmotors[i].para.pos;
        home_offset_rad[i] = home_offset_deg[i] * deg2rad;
    }

    while (done_cnt < motor_num) {
        for (i = 0; i < motor_num; i++) {
            switch (zero_state[i]) {
            case 0:
                cmd_pos[i] += home_dir[i] * pos_step_abs;
                pos_ctrl(hcan, i + 1, cmd_pos[i], pos_resp_vel);
                if ((dmotors[i].para.tor > tor_th) || (dmotors[i].para.tor < -tor_th)) {
                    zero_state[i] = 1;
                }
                break;
            case 1:
                disable_motor_mode(hcan, i + 1, POS_MODE);
                zero_state[i] = 2;
                break;
            case 2:
                save_pos_zero(hcan, i + 1, POS_MODE);
                cmd_pos[i] = 0.0f;
                zero_state[i] = 3;
                break;
            case 3:
								osDelay(1);
                enable_motor_mode(hcan, i + 1, POS_MODE);
                zero_state[i] = 4;
                done_cnt++;
                break;
            default:
                break;
            }
        }
        osDelay(1);
    }

    osDelay(100);
    for (i = 0; i < motor_num; i++) {
        cmd_pos[i] = home_offset_rad[i];
    }
    for (uint16_t t = 0; t < settle_ms; t++) {
        for (i = 0; i < motor_num; i++) {
						enable_motor_mode(hcan, i + 1, POS_MODE);
            pos_ctrl(hcan, i + 1, cmd_pos[i], pos_resp_vel);
        }
        osDelay(1);
    }
    for (i = 0; i < motor_num; i++) {
        save_pos_zero(hcan, i + 1, POS_MODE);
        osDelay(1);
    }

    return 0;
}
