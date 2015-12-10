/*
** =========================================================================
** File:
**     ImmVibeSPI.c
**
** Description:
**     Device-dependent functions called by Immersion TSP API
**     to control PWM duty cycle, amp enable/disable, save IVT file, etc...
**
** Portions Copyright (c) 2008-2010 Immersion Corporation. All Rights Reserved.
**
** This file contains Original Code and/or Modifications of Original Code
** as defined in and that are subject to the GNU Public License v2 -
** (the 'License'). You may not use this file except in compliance with the
** License. You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software Foundation, Inc.,
** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA or contact
** TouchSenseSales@immersion.com.
**
** The Original Code and all software distributed under the License are
** distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
** EXPRESS OR IMPLIED, AND IMMERSION HEREBY DISCLAIMS ALL SUCH WARRANTIES,
** INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS
** FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see
** the License for the specific language governing rights and limitations
** under the License.
** =========================================================================
*/
#include <linux/pwm.h>
#include <plat/gpio-cfg.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/mfd/max8997.h>
#include <linux/mfd/max8997-private.h>

#include "tspdrv.h"

#ifdef IMMVIBESPIAPI
#undef IMMVIBESPIAPI
#endif
#define IMMVIBESPIAPI static

/*
** This SPI supports only one actuator.
*/
#define NUM_ACTUATORS	1

#define PWM_DUTY_MAX    579 /* 13MHz / (579 + 1) = 22.4kHz */

static bool g_bAmpEnabled;

extern void (*vibtonz_en)(bool);
extern void (*vibtonz_pwm)(int);

/*
** Called to disable amp (disable output force)
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_AmpDisable(VibeUInt8 nActuatorIndex)
{
	if(vibtonz_en == NULL || vibtonz_pwm == NULL) {
		pr_info("[VIB] vibtonz_en or vibtonz_pwm is NULL\n");
		return 0;
	}

	if (g_bAmpEnabled) {

		g_bAmpEnabled = false;
		vibtonz_pwm(0);
		vibtonz_en(false);
		if (regulator_hapticmotor_enabled == 1) {
			regulator_hapticmotor_enabled = 0;
		}
	}

	return VIBE_S_SUCCESS;
}

/*
** Called to enable amp (enable output force)
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_AmpEnable(VibeUInt8 nActuatorIndex)
{
	if(vibtonz_en == NULL) {
		pr_info("[VIB] vibtonz_en is NULL\n");
		return 0;
	}

	if (!g_bAmpEnabled) {
		vibtonz_en(true);

		g_bAmpEnabled = true;
		regulator_hapticmotor_enabled = 1;
/*
		printk(KERN_DEBUG "tspdrv: %s (%d)\n", __func__,
					regulator_hapticmotor_enabled);
*/
	}

	return VIBE_S_SUCCESS;
}

/*
** Called at initialization time to set PWM freq, disable amp, etc...
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_Initialize(void)
{
    /*printk(KERN_ERR"[VIBRATOR]ImmVibeSPI_ForceOut_Initialize is called\n");*/
	DbgOut((KERN_DEBUG "ImmVibeSPI_ForceOut_Initialize.\n"));

	g_bAmpEnabled = true;   /* to force ImmVibeSPI_ForceOut_AmpDisable disabling the amp */

	/*
	** Disable amp.
	** If multiple actuators are supported, please make sure to call
	** ImmVibeSPI_ForceOut_AmpDisable for each actuator (provide the actuator index as
	** input argument).
	*/

	ImmVibeSPI_ForceOut_AmpDisable(0);

	regulator_hapticmotor_enabled = 0;

	return VIBE_S_SUCCESS;
}

/*
** Called at termination time to set PWM freq, disable amp, etc...
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_Terminate(void)
{
	DbgOut((KERN_DEBUG "ImmVibeSPI_ForceOut_Terminate.\n"));

	/*
	** Disable amp.
	** If multiple actuators are supported, please make sure to call
	** ImmVibeSPI_ForceOut_AmpDisable for each actuator (provide the actuator index as
	** input argument).
	*/
	ImmVibeSPI_ForceOut_AmpDisable(0);

	return VIBE_S_SUCCESS;
}

/*
** Called by the real-time loop to set PWM duty cycle
*/
static bool g_bOutputDataBufferEmpty = 1;

IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_SetSamples(VibeUInt8 nActuatorIndex,
							VibeUInt16 nOutputSignalBitDepth,
							VibeUInt16 nBufferSizeInBytes,
							VibeInt8 * pForceOutputBuffer)
{
	VibeInt8 nForce;

	if(vibtonz_pwm == NULL) {
		pr_info("[VIB] vibtonz_pwm is NULL\n");
		return VIBE_E_FAIL;
	}

	if (g_bOutputDataBufferEmpty) {
		nActuatorIndex = 0;
		nOutputSignalBitDepth = 8;
		nBufferSizeInBytes = 1;
		pForceOutputBuffer[0] = 0;
	}

	switch (nOutputSignalBitDepth) {
	case 8:
		/* pForceOutputBuffer is expected to contain 1 byte */
		if (nBufferSizeInBytes != 1) {
			DbgOut((KERN_ERR "[ImmVibeSPI] ImmVibeSPI_ForceOut_SetSamples nBufferSizeInBytes =  %d\n", nBufferSizeInBytes));
			return VIBE_E_FAIL;
		}
		nForce = pForceOutputBuffer[0];
		break;
	case 16:
		/* pForceOutputBuffer is expected to contain 2 byte */
		if (nBufferSizeInBytes != 2)
			return VIBE_E_FAIL;

		/* Map 16-bit value to 8-bit */
		nForce = ((VibeInt16 *)pForceOutputBuffer)[0] >> 8;
		break;
	default:
		/* Unexpected bit depth */
		return VIBE_E_FAIL;
	}

	if (nForce == 0) {
		/* Set 50% duty cycle or disable amp */
		ImmVibeSPI_ForceOut_AmpDisable(0);
	} else {
		/* Map force from [-127, 127] to [0, PWM_DUTY_MAX] */
		vibtonz_pwm(nForce);
		ImmVibeSPI_ForceOut_AmpEnable(0);
	}

	return VIBE_S_SUCCESS;
}
/*
** Called to get the device name (device name must be returned as ANSI char)
*/

IMMVIBESPIAPI VibeStatus ImmVibeSPI_Device_GetName(VibeUInt8 nActuatorIndex, char *szDevName, int nSize)
{
	return VIBE_S_SUCCESS;
}
