/*
    FreeRTOS V7.0.2 - Copyright (C) 2011 Real Time Engineers Ltd.


    ***************************************************************************
     *                                                                       *
     *    FreeRTOS tutorial books are available in pdf and paperback.        *
     *    Complete, revised, and edited pdf reference manuals are also       *
     *    available.                                                         *
     *                                                                       *
     *    Purchasing FreeRTOS documentation will not only help you, by       *
     *    ensuring you get running as quickly as possible and with an        *
     *    in-depth knowledge of how to use FreeRTOS, it will also help       *
     *    the FreeRTOS project to continue with its mission of providing     *
     *    professional grade, cross platform, de facto standard solutions    *
     *    for microcontrollers - completely free of charge!                  *
     *                                                                       *
     *    >>> See http://www.FreeRTOS.org/Documentation for details. <<<     *
     *                                                                       *
     *    Thank you for using FreeRTOS, and thank you for your support!      *
     *                                                                       *
    ***************************************************************************


    This file is part of the FreeRTOS distribution.

    FreeRTOS is free software; you can redistribute it and/or modify it under
    the terms of the GNU General Public License (version 2) as published by the
    Free Software Foundation AND MODIFIED BY the FreeRTOS exception.
    >>>NOTE<<< The modification to the GPL is included to allow you to
    distribute a combined work that includes FreeRTOS without being obliged to
    provide the source code for proprietary components outside of the FreeRTOS
    kernel.  FreeRTOS is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
    or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
    more details. You should have received a copy of the GNU General Public
    License and the FreeRTOS license exception along with FreeRTOS; if not it
    can be viewed here: http://www.freertos.org/a00114.html and also obtained
    by writing to Richard Barry, contact details for whom are available on the
    FreeRTOS WEB site.

    1 tab == 4 spaces!

    http://www.FreeRTOS.org - Documentation, latest information, license and
    contact details.

    http://www.SafeRTOS.com - A version that is certified for use in safety
    critical systems.

    http://www.OpenRTOS.com - Commercial support, development, porting,
    licensing and training services.
*/

#include "wmsdk_config.h"
#include "wm_heap.h"
#if defined(CONFIG_WIFI_API_ENABLED)
#include "wm_netif.h"
#endif

/* Standard includes. */
#include <stdlib.h>
#include <stdio.h>
/* Scheduler includes. */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

const unsigned int HZ = configTICK_RATE_HZ;

/*-----------------------------------------------------------*/

/* The code generated by the Keil compiler does not maintain separate
stack and frame pointers. The portENTER_CRITICAL macro cannot therefore
use the stack as per other ports.  Instead a variable is used to keep
track of the critical section nesting.  This variable has to be stored
as part of the task context and must be initialised to a non zero value. */

#define portNO_CRITICAL_NESTING		( ( unsigned portLONG ) 0 )
volatile unsigned portLONG ulCriticalNesting = 9999UL;

volatile unsigned portLONG ulLastTaskPrio = 0;


/*-----------------------------------------------------------*/

/* Setup the timer to generate the tick interrupts. */
//static void prvSetupTimerInterrupt( void );

/*
 * The scheduler can only be started from ARM mode, so
 * vPortStartFirstSTask() is defined in portISR.c.
 */
extern void vPortStartTask(void);


/* Each task maintains its own interrupt status in the critical nesting
variable. */
static volatile unsigned portBASE_TYPE uxCriticalNesting = 0xaaaaaaaa;

/*
 * Setup the timer to generate the tick interrupts.
 */
//static void prvSetupTimerInterrupt( void );

/*
 * Exception handlers.
 */
void xPortPendSVHandler( void );

void xPortSysTickHandler( void )
{
    portLONG ulDummy;

    ulDummy = portSET_INTERRUPT_MASK_FROM_ISR();
    xTaskIncrementTick();
    portYIELD_FROM_ISR(pdTRUE);
    portCLEAR_INTERRUPT_MASK_FROM_ISR( ulDummy );
}

void vPortSVCHandler( void );

/*
 * Start first task is a separate function so it can be tested in isolation.
 */
#define vPortStartFirstTask     vPortStartTask

/*-----------------------------------------------------------*/

/*
 * See header file for description.
 */
portSTACK_TYPE *pxPortInitialiseStack( portSTACK_TYPE *pxTopOfStack, pdTASK_CODE pxCode, void *pvParameters )
{
    StackType_t *stk  = NULL;

    stk = pxTopOfStack;

    *(--stk)  = (uint32_t)pxCode;            /* Entry Point                                         */
    *(--stk)  = (uint32_t)0x80000340L;       /* PSR                                                 */
    *(--stk)  = (uint32_t)0xfffffffeL;       /* R15 (LR) (init value will cause fault if ever used) */
    *(--stk)  = (uint32_t)0x13131313L;       /* R13                                                 */
    *(--stk)  = (uint32_t)0x12121212L;       /* R12                                                 */
    *(--stk)  = (uint32_t)0x11111111L;       /* R11                                                 */
    *(--stk)  = (uint32_t)0x10101010L;       /* R10                                                 */
    *(--stk)  = (uint32_t)0x09090909L;       /* R9                                                  */
    *(--stk)  = (uint32_t)0x08080808L;       /* R8                                                  */
    *(--stk)  = (uint32_t)0x07070707L;       /* R7                                                  */
    *(--stk)  = (uint32_t)0x06060606L;       /* R6                                                  */
    *(--stk)  = (uint32_t)0x05050505L;       /* R5                                                  */
    *(--stk)  = (uint32_t)0x04040404L;       /* R4                                                  */
    *(--stk)  = (uint32_t)0x03030303L;       /* R3                                                  */
    *(--stk)  = (uint32_t)0x02020202L;       /* R2                                                  */
    *(--stk)  = (uint32_t)0x01010101L;       /* R1                                                  */
    *(--stk)  = (uint32_t)pvParameters;      /* R0 : Argument                                       */

#if SAVE_HIGH_REGISTERS
    *(--stk)  = (uint32_t)0x31313131L;       /* R31                                                 */
    *(--stk)  = (uint32_t)0x30303030L;       /* R30                                                 */
    *(--stk)  = (uint32_t)0x29292929L;       /* R29                                                 */
    *(--stk)  = (uint32_t)0x28282828L;       /* R28                                                 */
    *(--stk)  = (uint32_t)0x27272727L;       /* R27                                                 */
    *(--stk)  = (uint32_t)0x26262626L;       /* R26                                                 */
    *(--stk)  = (uint32_t)0x25252525L;       /* R25                                                 */
    *(--stk)  = (uint32_t)0x24242424L;       /* R24                                                 */
    *(--stk)  = (uint32_t)0x23232323L;       /* R23                                                 */
    *(--stk)  = (uint32_t)0x22222222L;       /* R22                                                 */
    *(--stk)  = (uint32_t)0x21212121L;       /* R21                                                 */
    *(--stk)  = (uint32_t)0x20202020L;       /* R20                                                 */
    *(--stk)  = (uint32_t)0x19191919L;       /* R19                                                 */
    *(--stk)  = (uint32_t)0x18181818L;       /* R18                                                 */
    *(--stk)  = (uint32_t)0x17171717L;       /* R17                                                 */
    *(--stk)  = (uint32_t)0x16161616L;       /* R16                                                 */
#endif

#if SAVE_VR_REGISTERS
    *(--stk)  = (uint32_t)0x12345678L;       /* VR15                                                */
    *(--stk)  = (uint32_t)0x12345678L;       /* VR14                                                */
    *(--stk)  = (uint32_t)0x12345678L;       /* VR13                                                */
    *(--stk)  = (uint32_t)0x12345678L;       /* VR12                                                */
    *(--stk)  = (uint32_t)0x12345678L;       /* VR11                                                */
    *(--stk)  = (uint32_t)0x12345678L;       /* VR10                                                */
    *(--stk)  = (uint32_t)0x12345678L;       /* VR9                                                 */
    *(--stk)  = (uint32_t)0x12345678L;       /* VR8                                                 */
    *(--stk)  = (uint32_t)0x12345678L;       /* VR7                                                 */
    *(--stk)  = (uint32_t)0x12345678L;       /* VR6                                                 */
    *(--stk)  = (uint32_t)0x12345678L;       /* VR5                                                 */
    *(--stk)  = (uint32_t)0x12345678L;       /* VR4                                                 */
    *(--stk)  = (uint32_t)0x12345678L;       /* VR3                                                 */
    *(--stk)  = (uint32_t)0x12345678L;       /* VR2                                                 */
    *(--stk)  = (uint32_t)0x12345678L;       /* VR1                                                 */
    *(--stk)  = (uint32_t)0x12345678L;       /* VR0                                                 */
#endif

    return stk;
}
/*-----------------------------------------------------------*/
/*
 * See header file for description.
 */
portBASE_TYPE xPortStartScheduler( void )
{
	/* Initialise the critical nesting count ready for the first task. */
	uxCriticalNesting = 0;

	/* Start the first task. */
	vPortStartFirstTask();

	/* Should not get here! */
	return 0;
}
/*-----------------------------------------------------------*/

void vPortEndScheduler( void )
{
	/* It is unlikely that the CM3 port will require this function as there
	is nothing to return to.  */
}
/*-----------------------------------------------------------*/

void vPortYield( void )
{
	/* Set a PendSV to request a context switch. */
}
/*-----------------------------------------------------------*/

void vPortEnterCritical( void )
{
	portDISABLE_INTERRUPTS();
	uxCriticalNesting++;
}
void cpu_intrpt_save( void )
{
	portDISABLE_INTERRUPTS();
	uxCriticalNesting++;
}
/*-----------------------------------------------------------*/

void vPortExitCritical( void )
{
	if (uxCriticalNesting == 0) {
        while(1);
    }
	if (uxCriticalNesting)
		uxCriticalNesting--;
	if( uxCriticalNesting == 0 )
	{
		portENABLE_INTERRUPTS();

        //if (pendsvflag)
        //{
        //    pendsvflag = 0;
        //    portYIELD();
        //}
	}
}
void cpu_intrpt_restore( void )
{
	if (uxCriticalNesting)
		uxCriticalNesting--;
	if( uxCriticalNesting == 0 )
	{
		portENABLE_INTERRUPTS();
	}
}

/*-----------------------------------------------------------*/


/* configUSE_STATIC_ALLOCATION is set to 1, so the application must provide an
implementation of vApplicationGetIdleTaskMemory() to provide the memory that is
used by the Idle task. */
void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize )
{
/* If the buffers to be provided to the Idle task are declared inside this
function then they must be declared static - otherwise they will be allocated on
the stack and so not exists after this function exits. */
static StaticTask_t xIdleTaskTCB;
static StackType_t uxIdleTaskStack[ configMINIMAL_STACK_SIZE ];

	/* Pass out a pointer to the StaticTask_t structure in which the Idle task's
	state will be stored. */
	*ppxIdleTaskTCBBuffer = &xIdleTaskTCB;

	/* Pass out the array that will be used as the Idle task's stack. */
	*ppxIdleTaskStackBuffer = uxIdleTaskStack;

	/* Pass out the size of the array pointed to by *ppxIdleTaskStackBuffer.
	Note that, as the array is necessarily of type StackType_t,
	configMINIMAL_STACK_SIZE is specified in words, not bytes. */
	*pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}
/*-----------------------------------------------------------*/

/* configUSE_STATIC_ALLOCATION and configUSE_TIMERS are both set to 1, so the
application must provide an implementation of vApplicationGetTimerTaskMemory()
to provide the memory that is used by the Timer service task. */
void vApplicationGetTimerTaskMemory( StaticTask_t **ppxTimerTaskTCBBuffer, StackType_t **ppxTimerTaskStackBuffer, uint32_t *pulTimerTaskStackSize )
{
/* If the buffers to be provided to the Timer task are declared inside this
function then they must be declared static - otherwise they will be allocated on
the stack and so not exists after this function exits. */
static StaticTask_t xTimerTaskTCB;
static StackType_t uxTimerTaskStack[ configTIMER_TASK_STACK_DEPTH ];

	/* Pass out a pointer to the StaticTask_t structure in which the Timer
	task's state will be stored. */
	*ppxTimerTaskTCBBuffer = &xTimerTaskTCB;

	/* Pass out the array that will be used as the Timer task's stack. */
	*ppxTimerTaskStackBuffer = uxTimerTaskStack;

	/* Pass out the size of the array pointed to by *ppxTimerTaskStackBuffer.
	Note that, as the array is necessarily of type StackType_t,
	configMINIMAL_STACK_SIZE is specified in words, not bytes. */
	*pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}

#if ( configCHECK_FOR_STACK_OVERFLOW == 1 )
void vApplicationStackOverflowHook( TaskHandle_t xTask,  char * pcTaskName )
{
	//printf("\ntask[%s] priority[%d] stack over flow\n",pcTaskName, configMAX_PRIORITIES - (int)(xTask->uxPriority));
	printf("\ntask[%s] stack over flow\n",pcTaskName);
	asm("trap 0");
    for(;;);
}
#endif /* if ( configCHECK_FOR_STACK_OVERFLOW == 1 )*/

#if ( configUSE_IDLE_HOOK == 1 )
void vApplicationIdleHook( void )
{
    __WAIT();
    return;
}
#endif

void vPortCleanUpTCB(void *pxTCB)
{
#if defined(CONFIG_WIFI_API_ENABLED)
    wm_netif_cleanup_tcb(pxTCB);
#endif
}

#ifdef CONFIG_HEAP_USE_TRACING
void * pvPortMallocTracing( size_t xSize, const char *pcFile, int iLine )
{
    return wm_heap_caps_alloc_tracing( xSize, WM_HEAP_CAP_INTERNAL, pcFile, iLine );
}
#else
void * pvPortMalloc( size_t xWantedSize )
{
    return wm_heap_caps_alloc( xWantedSize, WM_HEAP_CAP_INTERNAL );
}
#endif

void vPortFree( void * pv )
{
    wm_heap_caps_free( pv );
}

size_t xPortGetFreeHeapSize( void )
{
    return wm_heap_get_free_size();
}

size_t xPortGetMinimumEverFreeHeapSize( void )
{
    return wm_heap_get_minimum_ever_free_size();
}
