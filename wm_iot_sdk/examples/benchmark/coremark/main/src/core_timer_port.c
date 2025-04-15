/******************************************************************************
 * @file     core_timer_port.c
 * @brief    the systimer for the Dhrystone
 * @version  V1.0
 * @date     20. July 2016
 ******************************************************************************/
/*
 * Copyright (C) 2017 C-SKY Microsystems Co., Ltd. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/*
 *start  systimer
 *
 */
void Timer_Open()
{
}

void Timer_Start()
{
}

void Timer_Stop()
{
}

void Timer_Close()
{
}

/* time(uS) since run */
unsigned long long clock_gettime()
{
    return xTaskGetTickCount() * portTICK_PERIOD_MS * 1000;
}
