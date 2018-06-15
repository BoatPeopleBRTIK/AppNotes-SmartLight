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

#ifndef __SMART_LIGHT_UTIL_h__
#define __SMART_LIGHT_UTIL_h__

#define BRIGHTNESS_CTRL_PWM_PIN 3
#define LIGHT_CTRL_BUTTON 44

#define LED_FREQUENCY 1000

// Preset brighness level
#define BRIGHTNESS_ON                   65535
#define BRIGHTNESS_ON_PERCENTAGE        100

#define BRIGHTNESS_READING              52428
#define BRIGHTNESS_READING_PERCENTAGE   80

#define BRIGHTNESS_MOVIE                22937
#define BRIGHTNESS_MOVIE_PERCENTAGE     35

#define BRIGHTNESS_SLEEP                5242
#define BRIGHTNESS_SLEEP_PERCENTAGE     8

#define BRIGHTNESS_OFF                  0
#define BRIGHTNESS_OFF_PERCENTAGE       0


enum SMART_LIGHT_PRESENT_MODES
{
    MODE_OFF = 0,
    MODE_ON,
    MODE_READING,
    MODE_MOVIE,
    MODE_SLEEP,
    
};


#endif // __SMART_LIGHT_UTIL_h__
