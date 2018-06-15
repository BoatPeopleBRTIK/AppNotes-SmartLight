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

/**
 * @file smart_light.c
 */

#include <stdio.h>
#include <sys/stat.h>
#include <tinyara/config.h>
#include <tinyara/fs/fs_utils.h>
#include <shell/tash.h>
#include <artik_error.h>
#include <artik_cloud.h>
#include <artik_wifi.h>
#include <errno.h>

#include "smart_light.h"

enum ServiceState current_service_state = STATE_IDLE;
pid_t smart_light_onboarding_service_pid = -1;
int light_control_state;

static void StopOnboardingService(void)
{
	StartWebServer(false, API_SET_WIFI | API_SET_CLOUD);
	StartCloudWebsocket(false, NULL);
	StartLwm2m(false, NULL);
	StopWifi();

	printf("Smart light onboarding Service stopped\n");
	current_service_state = STATE_IDLE;
	smart_light_onboarding_service_pid = -1;
}

static pthread_addr_t start_onboarding(pthread_addr_t arg)
{
	printf("Smart light onboarding service version %s\n", ONBOARDING_VERSION);

	current_service_state = STATE_ONBOARDING;

    light_control_state = 0;

	/* If we already have Wifi credentials, try to connect to the hotspot */
	if (strlen(wifi_config.ssid) > 0) {
		if (StartStationConnection(true) == S_OK) {
			/* Check if we have valid ARTIK Cloud credentials */
			if ((strlen(cloud_config.device_id) == AKC_DID_LEN) &&
			    (strlen(cloud_config.device_token) == AKC_TOKEN_LEN)) {
				if (StartCloudWebsocket(true, NULL) == S_OK) {
					if (StartLwm2m(true, NULL) == S_OK) {
						printf("ARTIK Cloud connection started\n");
						goto exit;
					} else {
						printf("Failed to start DM connection, switching back to onboarding mode\n");
					}
				} else {
					printf("Failed to start ARTIK Cloud connection, switching back to onboarding mode\n");
				}
			} else {
				printf("Invalid ARTIK Cloud credentials, switching back to onboarding mode\n");
			}
		} else {
			printf("Could not connect to access point, switching back to onboarding mode\n");
		}
	}

	/* If Cloud connection failed, start the onboarding service */
	if (StartSoftAP(true) != S_OK) {
		goto exit;
	}

	if (StartWebServer(true, API_SET_WIFI) != S_OK) {
		StartSoftAP(false);
		goto exit;
	}

	printf("Smart light onboarding Service started\n");
	current_service_state = STATE_ONBOARDING;

exit:
	return NULL;
}

#ifdef CONFIG_BUILD_KERNEL
int main(int argc, FAR char *argv[])
#else
int smart_light_main(int argc, char *argv[])
#endif
{
	int ret = 0;
	pthread_t tid;
	sigset_t mask;
	pthread_attr_t attr;

	if (argc > 1) {
		if (!strcmp(argv[1], "reset")) {
			ResetConfiguration(false);
			printf("Smart light onboarding configuration was reset.\n"
				"Reboot the board to return to onboarding mode\n");
			goto exit;
		} else if (!strcmp(argv[1], "dtid")) {
			if (argc < 3) {
				printf("Missing parameter\n");
				printf("Usage: smartlight dtid <device type ID>\n");
				goto exit;
			}

			strncpy(cloud_config.device_type_id, argv[2], AKC_DTID_LEN);
			SaveConfiguration();
			goto exit;

		} else if (!strcmp(argv[1], "ota-sig-verif")) {
			if (argc < 3) {
				printf("Missing parameter\n");
				printf("Usage: smartlight ota-sig-verif <enable|disable>\n");
				goto exit;
			}

			if (!strcmp(argv[2], "enable")) {
				printf("Signature verification enabled\n");
				lwm2m_config.ota_signature_verification = 1;
			} else if (!strcmp(argv[2], "disable")) {
				printf("Signature verification disabled\n");
				lwm2m_config.ota_signature_verification = 0;
			} else {
				printf("Bad argument\n");
				printf("Usage: smartlight ota-sig-verif <enable|disable>\n");
				goto exit;
			}

			SaveConfiguration();
			goto exit;

		} else if (!strcmp(argv[1], "config")) {
            PrintFromConfigFile();
			goto exit;
		}  else if (!strcmp(argv[1], "manual")) {
			if (argc < 4) {
				printf("Missing parameter\n");
				printf("Usage: smartlight manual <device ID> <device token>\n");
				goto exit;
			}

			strncpy(cloud_config.device_id, argv[2], AKC_DID_LEN);
			strncpy(cloud_config.device_token, argv[3], AKC_TOKEN_LEN);
			SaveConfiguration();
			goto exit;
		}  else if (!strcmp(argv[1], "ntp")) {
			if (argc < 3) {
				printf("Missing parameter\n");
				printf("Usage: smartlight ntp <NTP server URL>\n");
				goto exit;
			}

			strncpy(wifi_config.ntp_server, argv[2], NTP_SERVER_MAX_LEN);
			SaveConfiguration();
			goto exit;
		} else if (!strcmp(argv[1], "stop")) {
			if (smart_light_onboarding_service_pid != -1) {
				kill(smart_light_onboarding_service_pid, SIGUSR1);
			}

			goto exit;
		}
	}

	if (smart_light_onboarding_service_pid != -1) {
		printf("Smart light onboarding service is already launched.");
		goto exit;
	}

	/* If already in onboarding mode or trying to connect, do nothing */
	if ((current_service_state == STATE_ONBOARDING) ||
			(current_service_state == STATE_CONNECTING))
		goto exit;

	if (current_service_state == STATE_CONNECTED) {
		printf("Device is currently connected to cloud. To return to\n"
		       "onboarding mode, delete the device from your ARTIK Cloud\n"
		       "account then reboot the board.\n");
		goto exit;
	}

	if (InitConfiguration() != S_OK) {
		ret = -1;
		goto exit;
	}

    // Start lighting control
    start_lightling_control();


	pthread_attr_init(&attr);
	pthread_attr_setschedpolicy(&attr, SCHED_RR);
	pthread_attr_setstacksize(&attr, 16 * 1024);
	pthread_create(&tid, &attr, start_onboarding, NULL);
	pthread_setname_np(tid, "smart light onboarding start");
	pthread_join(tid, NULL);
	smart_light_onboarding_service_pid = getpid();

	sigemptyset(&mask);
	sigaddset(&mask, SIGUSR1);
	sigwaitinfo(&mask, NULL);

	StopOnboardingService();

exit:
	return ret;
}
