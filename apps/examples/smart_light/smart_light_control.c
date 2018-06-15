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

#include <stdio.h>
#include <pthread.h>
#include <tinyara/pwm.h>
#include <fcntl.h>
#include <stdlib.h>

#include <artik_module.h>
#include <artik_gpio.h>

#include "smart_light_pwm_routines.h"
#include "smart_light.h"

#define MIN_LEVEL 0x0
#define MAX_LEVEL 0xFFFF

int light_control_state = 0;
int fd_brightness_ctrl;


struct gpio_node {
    artik_list node;
    artik_gpio_handle handle;
};

static artik_list *requested_node;
static int button_press_count = 0;


void set_brightness_level(int brightness)
{
    int ret;
    
    // Boundary check
    if (brightness < MIN_LEVEL)
    {
        brightness = MIN_LEVEL;
    }

    if (brightness > MAX_LEVEL)
    {
        brightness = MAX_LEVEL;
    }

    // set brightness
    ret = pwm_generator(brightness, fd_brightness_ctrl);
    if (ret < OK)
    {
        printf("[set_brightness_level]: pwm_generator failed: %d\n", ret);
    }
}


void button_press_callback(void *user_data)
{
    button_press_count++;

    button_press_count = button_press_count % 5;
    
    switch (button_press_count) {
    case MODE_ON:
    	printf("Change to \"ON\" mode, set brightness to %d percent\n", BRIGHTNESS_ON_PERCENTAGE);
        pwm_generator(BRIGHTNESS_ON, fd_brightness_ctrl);
        break;
    case MODE_READING:
    	printf("Change to \"READING\" mode, set brightness to %d percent\n", BRIGHTNESS_READING_PERCENTAGE);
        pwm_generator(BRIGHTNESS_READING, fd_brightness_ctrl);
        break;
    case MODE_MOVIE:
    	printf("Change to \"MOVIE\" mode, set brightness to %d percent\n", BRIGHTNESS_MOVIE_PERCENTAGE);
        pwm_generator(BRIGHTNESS_MOVIE, fd_brightness_ctrl);
        break;
    case MODE_SLEEP:
    	printf("Change to \"SLEEP\" mode, set brightness to %d percent\n", BRIGHTNESS_SLEEP_PERCENTAGE);
        pwm_generator(BRIGHTNESS_SLEEP, fd_brightness_ctrl);
        break;
    case MODE_OFF:
    	printf("Change to \"OFF\" mode, set brightness to %d percent\n", BRIGHTNESS_OFF_PERCENTAGE);
        pwm_generator(BRIGHTNESS_OFF, fd_brightness_ctrl);
        break;
    }
    
    return;
}



static int watch_button_press(void)
{
    int ret = OK;
    artik_gpio_module *gpio = (artik_gpio_module *) artik_request_api_module("gpio");

    artik_gpio_config config;
    artik_gpio_handle handle;
    char name[16] = "";

    memset(&config, 0, sizeof(config));
    snprintf(name, 16, "gpio%d", config.id);
    config.name = name;
    config.id = LIGHT_CTRL_BUTTON;
    config.dir = GPIO_IN;
    config.initial_value = 0;
    config.edge = GPIO_EDGE_RISING;

    ret = gpio->request(&handle, &config);
    if (ret != S_OK) {
        fprintf(stderr, "GPIO id %u already on watching or wrong GPIO id\n", config.id);
        goto exit;
    }

    struct gpio_node *node = (struct gpio_node *)artik_list_add(&requested_node,
        (ARTIK_LIST_HANDLE)config.id, sizeof(struct gpio_node));
    if (!node)
        return E_NO_MEM;

    node->handle = handle;

    ret = gpio->set_change_callback(handle, (artik_gpio_callback) button_press_callback, NULL);
    if (ret != S_OK) {
        fprintf(stderr, "Failed, could not set GPIO change callback (%d)\n", ret);
        goto exit;
    }

    fprintf(stdout, "GPIO id %u on watching\n", config.id);

exit:
    artik_release_api_module(gpio);


    return ret;
}



static void exit_on_error(void)
{
    pwm_close(fd_brightness_ctrl);
}


int start_lightling_control(void)
{
	int ret = 0;

	if (!light_control_state)
	{
        printf("Start smart light control\n");
    
    	light_control_state = 1;
        
    	// Preparing PWM for controlling the brightness of LED
    	pwm_init(BRIGHTNESS_CTRL_PWM_PIN, &fd_brightness_ctrl);
    	ret = pwm_generator(BRIGHTNESS_OFF, fd_brightness_ctrl);
    	if (ret < OK)
    	{
    		printf("failed to set brightness [%d]\n", ret);
    		exit_on_error();
    		return ret;
    	}

        // start watching for button press
        ret = watch_button_press();
    	if (ret < OK)
    	{
    		printf("failed to register button [%d]\n", ret);
    		exit_on_error();
    		return ret;
    	}
    }
	else
	{
		printf("Start smart light control has already started!\n");
	}
    return ret;
}
