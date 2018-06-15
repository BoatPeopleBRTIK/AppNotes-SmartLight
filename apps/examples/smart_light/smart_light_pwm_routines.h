/****************************************************************************
 *
* Copyright 2018 Samsung Electronics All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the License.
 *
 ****************************************************************************/

#ifndef __SMART_LIGHT_PWM_ROUTINES_H__
#define __SMART_LIGHT_PWM_ROUTINES_H__

#include <tinyara/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <tinyara/pwm.h>
#include <sys/ioctl.h>


#define PWM_DEVPATH0 "dev/pwm0"
#define PWM_DEVPATH1 "dev/pwm1"
#define PWM_DEVPATH2 "dev/pwm2"
#define PWM_DEVPATH3 "dev/pwm3"
#define PWM_DEVPATH4 "dev/pwm4"
#define PWM_DEVPATH5 "dev/pwm5"



struct pwm_state_s
{
  bool      initialized;
  uint32_t  channel;
  FAR char *devpath;
  uint8_t   duty;
  uint32_t  freq;
  int       duration;
};

char devicename[10];

struct pwm_info_s info;

static void pwm_devpath(FAR struct pwm_state_s *pwm, FAR const char *devpath);

void pwm_init(int pwm_pin, int *fd);
int pwm_generator(int dc, int fd);
int pwm_close(int);

#endif // __SMART_LIGHT_PWM_ROUTINES_H__
