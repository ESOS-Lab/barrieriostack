/* Copyright (c) 2011 Samsung Electronics Co, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 *

 * Alternatively, Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.

 */

#ifndef FIMC_IS_METADATA_H_
#define FIMC_IS_METADATA_H_

#ifndef _LINUX_TYPES_H
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef signed short int16_t;
typedef signed int int32_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
#endif

struct rational {
	uint32_t num;
	uint32_t den;
};

#define CAMERA2_MAX_AVAILABLE_MODE	21
#define CAMERA2_MAX_FACES		16
#define CAMERA2_MAX_VENDER_LENGTH	400
#define CAPTURE_NODE_MAX		2
#define CAMERA2_MAX_PDAF_MULTIROI_COLUMN 9
#define CAMERA2_MAX_PDAF_MULTIROI_ROW 5

#define OPEN_MAGIC_NUMBER		0x01020304
#define SHOT_MAGIC_NUMBER		0x23456789

/*
 *controls/dynamic metadata
*/

/* android.request */

enum metadata_mode {
	METADATA_MODE_NONE,
	METADATA_MODE_FULL
};

struct camera2_request_ctl {
	uint32_t		id;
	enum metadata_mode	metadataMode;
	uint8_t			outputStreams[16];
	uint32_t		frameCount;
        uint32_t		requestCount;
};

struct camera2_request_dm {
	uint32_t		id;
	enum metadata_mode	metadataMode;
	uint32_t		frameCount;
        uint32_t		requestCount;
};

struct camera2_entry_ctl {
	/** \brief
		per-frame control for entry control
		\remarks
		low parameter is 0bit ~ 31bit flag
		high parameter is 32bit ~ 63bit flag
	*/
	uint32_t		lowIndexParam;
	uint32_t		highIndexParam;
	uint32_t		parameter[2048];
};

struct camera2_entry_dm {
	uint32_t		lowIndexParam;
	uint32_t		highIndexParam;
};

/* android.lens */

enum optical_stabilization_mode {
	OPTICAL_STABILIZATION_MODE_OFF = 0,
	OPTICAL_STABILIZATION_MODE_ON = 1,
	OPTICAL_STABILIZATION_MODE_STILL = 2,  // Still mode
	OPTICAL_STABILIZATION_MODE_VIDEO = 3,  // Recording mode
	OPTICAL_STABILIZATION_MODE_SINE_X = 4,  // factory mode x
	OPTICAL_STABILIZATION_MODE_SINE_Y = 5  // factory mode y
};

enum lens_facing {
	LENS_FACING_BACK,
	LENS_FACING_FRONT
};

struct camera2_lens_ctl {
	uint32_t				focusDistance;
	float					aperture;
	float					focalLength;
	float					filterDensity;
	enum optical_stabilization_mode		opticalStabilizationMode;

};

struct camera2_lens_dm {
	uint32_t				focusDistance;
	float					aperture;
	float					focalLength;
	float					filterDensity;
	enum optical_stabilization_mode		opticalStabilizationMode;
	float					focusRange[2];
};

struct camera2_lens_sm {
	float				minimumFocusDistance;
	float				hyperfocalDistance;
	float				availableFocalLength[2];
	float				availableApertures;
	/*assuming 1 aperture*/
	float				availableFilterDensities;
	/*assuming 1 ND filter value*/
	enum optical_stabilization_mode	availableOpticalStabilization;
	/*assuming 1*/
	uint32_t			shadingMapSize;
	float				shadingMap[3][40][30];
	uint32_t			geometricCorrectionMapSize;
	float				geometricCorrectionMap[2][3][40][30];
	enum lens_facing		facing;
	float				position[2];
};

/* android.sensor */

enum sensor_colorfilterarrangement {
	SENSOR_COLORFILTERARRANGEMENT_RGGB,
	SENSOR_COLORFILTERARRANGEMENT_GRBG,
	SENSOR_COLORFILTERARRANGEMENT_GBRG,
	SENSOR_COLORFILTERARRANGEMENT_BGGR,
	SENSOR_COLORFILTERARRANGEMENT_RGB
};

enum sensor_ref_illuminant {
	SENSOR_ILLUMINANT_DAYLIGHT = 1,
	SENSOR_ILLUMINANT_FLUORESCENT = 2,
	SENSOR_ILLUMINANT_TUNGSTEN = 3,
	SENSOR_ILLUMINANT_FLASH = 4,
	SENSOR_ILLUMINANT_FINE_WEATHER = 9,
	SENSOR_ILLUMINANT_CLOUDY_WEATHER = 10,
	SENSOR_ILLUMINANT_SHADE = 11,
	SENSOR_ILLUMINANT_DAYLIGHT_FLUORESCENT = 12,
	SENSOR_ILLUMINANT_DAY_WHITE_FLUORESCENT = 13,
	SENSOR_ILLUMINANT_COOL_WHITE_FLUORESCENT = 14,
	SENSOR_ILLUMINANT_WHITE_FLUORESCENT = 15,
	SENSOR_ILLUMINANT_STANDARD_A = 17,
	SENSOR_ILLUMINANT_STANDARD_B = 18,
	SENSOR_ILLUMINANT_STANDARD_C = 19,
	SENSOR_ILLUMINANT_D55 = 20,
	SENSOR_ILLUMINANT_D65 = 21,
	SENSOR_ILLUMINANT_D75 = 22,
	SENSOR_ILLUMINANT_D50 = 23,
	SENSOR_ILLUMINANT_ISO_STUDIO_TUNGSTEN = 24
};

struct camera2_sensor_ctl {
	/* unit : nano */
	uint64_t	exposureTime;
	/* unit : nano(It's min frame duration */
	uint64_t	frameDuration;
	/* unit : percent(need to change ISO value?) */
	uint32_t	sensitivity;
};

struct camera2_sensor_dm {
	uint64_t	exposureTime;
	uint64_t	frameDuration;
	uint32_t	sensitivity;
	uint64_t	timeStamp;
	uint32_t	analogGain;
	uint32_t	digitalGain;
};

struct camera2_sensor_sm {
	uint32_t	exposureTimeRange[2];
	uint32_t	maxFrameDuration;
	/* list of available sensitivities. */
	uint32_t	availableSensitivities[10];
	enum sensor_colorfilterarrangement colorFilterArrangement;
	float		physicalSize[2];
	uint32_t	pixelArraySize[2];
	uint32_t	activeArraySize[4];
	uint32_t	whiteLevel;
	uint32_t	blackLevelPattern[4];
	struct rational	colorTransform1[9];
	struct rational	colorTransform2[9];
	enum sensor_ref_illuminant	referenceIlluminant1;
	enum sensor_ref_illuminant	referenceIlluminant2;
	struct rational	forwardMatrix1[9];
	struct rational	forwardMatrix2[9];
	struct rational	calibrationTransform1[9];
	struct rational	calibrationTransform2[9];
	struct rational	baseGainFactor;
	uint32_t	maxAnalogSensitivity;
	float		noiseModelCoefficients[2];
	uint32_t	orientation;
};



/* android.flash */

enum flash_mode {
	CAM2_FLASH_MODE_OFF = 1,
	CAM2_FLASH_MODE_SINGLE,
	CAM2_FLASH_MODE_TORCH,
	CAM2_FLASH_MODE_BEST
};

enum capture_state {
	CAPTURE_STATE_NONE = 0,
	CAPTURE_STATE_FLASH = 1,
	CAPTURE_STATE_HDR_DARK = 12,
	CAPTURE_STATE_HDR_NORMAL = 13,
	CAPTURE_STATE_HDR_BRIGHT = 14,
	CAPTURE_STATE_ZSL_LIKE = 20,
};

struct camera2_flash_ctl {
	enum flash_mode		flashMode;
	uint32_t		firingPower;
	uint64_t		firingTime;
};

struct camera2_flash_dm {
	enum flash_mode		flashMode;
	/*10 is max power*/
	uint32_t		firingPower;
	/*unit : microseconds*/
	uint64_t		firingTime;
	/*1 : stable, 0 : unstable*/
	uint32_t		firingStable;
	/*1 : success, 0 : fail*/
	uint32_t		decision;
	/*0: None, 1 : pre, 2 : main flash ready*/
	uint32_t		flashReady;
	/*0: None, 1 : pre, 2 : main flash off ready*/
	uint32_t		flashOffReady;
};

struct camera2_flash_sm {
	uint32_t	available;
	uint64_t	chargeDuration;
};


/* android.hotpixel */

enum processing_mode {
	PROCESSING_MODE_OFF = 1,
	PROCESSING_MODE_FAST,
	PROCESSING_MODE_HIGH_QUALITY
};


struct camera2_hotpixel_ctl {
	enum processing_mode	mode;
};

struct camera2_hotpixel_dm {
	enum processing_mode	mode;
};



/* android.demosaic */

struct camera2_demosaic_ctl {
	enum processing_mode	mode;
};

struct camera2_demosaic_dm {
	enum processing_mode	mode;
};



/* android.noiseReduction */

struct camera2_noisereduction_ctl {
	enum processing_mode	mode;
	uint32_t		strength;
};

struct camera2_noisereduction_dm {
	enum processing_mode	mode;
	uint32_t		strength;
};



/* android.shading */

struct camera2_shading_ctl {
	enum processing_mode	mode;
};

struct camera2_shading_dm {
	enum processing_mode	mode;
};



/* android.geometric */

struct camera2_geometric_ctl {
	enum processing_mode	mode;
};

struct camera2_geometric_dm {
	enum processing_mode	mode;
};



/* android.colorCorrection */

enum colorcorrection_mode {
	COLORCORRECTION_MODE_FAST = 1,
	COLORCORRECTION_MODE_HIGH_QUALITY,
	COLORCORRECTION_MODE_TRANSFORM_MATRIX,
	COLORCORRECTION_MODE_EFFECT_MONO,
	COLORCORRECTION_MODE_EFFECT_NEGATIVE,
	COLORCORRECTION_MODE_EFFECT_SOLARIZE,
	COLORCORRECTION_MODE_EFFECT_SEPIA,
	COLORCORRECTION_MODE_EFFECT_POSTERIZE,
	COLORCORRECTION_MODE_EFFECT_WHITEBOARD,
	COLORCORRECTION_MODE_EFFECT_BLACKBOARD,
	COLORCORRECTION_MODE_EFFECT_AQUA,
	COLORCORRECTION_MODE_EFFECT_EMBOSS,
	COLORCORRECTION_MODE_EFFECT_EMBOSS_MONO,
	COLORCORRECTION_MODE_EFFECT_SKETCH,
	COLORCORRECTION_MODE_EFFECT_RED_YELLOW_POINT,
	COLORCORRECTION_MODE_EFFECT_GREEN_POINT,
	COLORCORRECTION_MODE_EFFECT_BLUE_POINT,
	COLORCORRECTION_MODE_EFFECT_MAGENTA_POINT,
	COLORCORRECTION_MODE_EFFECT_WARM_VINTAGE,
	COLORCORRECTION_MODE_EFFECT_COLD_VINTAGE,
	COLORCORRECTION_MODE_EFFECT_WASHED,
	COLORCORRECTION_MODE_EFFECT_BEAUTY_FACE,
	TOTAOCOUNT_COLORCORRECTION_MODE_EFFECT
};


struct camera2_colorcorrection_ctl {
	enum colorcorrection_mode	mode;
	float				transform[9];
	uint32_t			hue;
	uint32_t			saturation;
	uint32_t			brightness;
	uint32_t			contrast;
};

struct camera2_colorcorrection_dm {
	enum colorcorrection_mode	mode;
	float				transform[9];
	uint32_t			hue;
	uint32_t			saturation;
	uint32_t			brightness;
	uint32_t			contrast;
};

struct camera2_colorcorrection_sm {
	/*assuming 10 supported modes*/
	uint8_t			availableModes[CAMERA2_MAX_AVAILABLE_MODE];
	uint32_t		hueRange[2];
	uint32_t		saturationRange[2];
	uint32_t		brightnessRange[2];
	uint32_t		contrastRange[2];
};


/* android.tonemap */

enum tonemap_mode {
	TONEMAP_MODE_FAST = 1,
	TONEMAP_MODE_HIGH_QUALITY,
	TONEMAP_MODE_CONTRAST_CURVE
};

struct camera2_tonemap_ctl {
	enum tonemap_mode		mode;
	/* assuming maxCurvePoints = 64 */
	float				curveRed[64];
	float				curveGreen[64];
	float				curveBlue[64];
};

struct camera2_tonemap_dm {
	enum tonemap_mode		mode;
	/* assuming maxCurvePoints = 64 */
	float				curveRed[64];
	float				curveGreen[64];
	float				curveBlue[64];
};

struct camera2_tonemap_sm {
	uint32_t	maxCurvePoints;
};

/* android.edge */

struct camera2_edge_ctl {
	enum processing_mode	mode;
	uint32_t		strength;
};

struct camera2_edge_dm {
	enum processing_mode	mode;
	uint32_t		strength;
};



/* android.scaler */

enum scaler_availableformats {
	SCALER_FORMAT_BAYER_RAW,
	SCALER_FORMAT_YV12,
	SCALER_FORMAT_NV21,
	SCALER_FORMAT_JPEG,
	SCALER_FORMAT_UNKNOWN
};

struct camera2_scaler_ctl {
	uint32_t	cropRegion[4];
	uint32_t	orientation;
};

struct camera2_scaler_dm {
	uint32_t	cropRegion[4];
	uint32_t	orientation;
};

struct camera2_scaler_sm {
	enum scaler_availableformats availableFormats[4];
	/*assuming # of availableFormats = 4*/
	uint32_t	availableRawSizes;
	uint64_t	availableRawMinDurations;
	/* needs check */
	uint32_t	availableProcessedSizes[8];
	uint64_t	availableProcessedMinDurations[8];
	uint32_t	availableJpegSizes[8][2];
	uint64_t	availableJpegMinDurations[8];
	uint32_t	availableMaxDigitalZoom[8];
};

/* android.jpeg */
struct camera2_jpeg_ctl {
	uint32_t	quality;
	uint32_t	thumbnailSize[2];
	uint32_t	thumbnailQuality;
	double		gpsCoordinates[3];
	uint32_t	gpsProcessingMethod;
	uint64_t	gpsTimestamp;
	uint32_t	orientation;
};

struct camera2_jpeg_dm {
	uint32_t	quality;
	uint32_t	thumbnailSize[2];
	uint32_t	thumbnailQuality;
	double		gpsCoordinates[3];
	uint32_t	gpsProcessingMethod;
	uint64_t	gpsTimestamp;
	uint32_t	orientation;
};

struct camera2_jpeg_sm {
	uint32_t	availableThumbnailSizes[8][2];
	uint32_t	maxSize;
	/*assuming supported size=8*/
};



/* android.statistics */

enum facedetect_mode {
	FACEDETECT_MODE_OFF = 1,
	FACEDETECT_MODE_SIMPLE,
	FACEDETECT_MODE_FULL
};

enum stats_mode {
	STATS_MODE_OFF = 1,
	STATS_MODE_ON
};

enum stats_lowlightmode {
	STATE_LLS_LEVEL_ZSL = 0,
	STATE_LLS_LEVEL_LOW = 1,
	STATE_LLS_LEVEL_HIGH = 2,
	STATE_LLS_LEVEL_SIS = 3,
	STATE_LLS_LEVEL_ZSL_LIKE = 4,
	STATE_LLS_LEVEL_FLASH = 16,
};

struct camera2_stats_ctl {
	enum facedetect_mode	faceDetectMode;
	enum stats_mode		histogramMode;
	enum stats_mode		sharpnessMapMode;
};


struct camera2_stats_dm {
	enum facedetect_mode	faceDetectMode;
	uint32_t		faceRectangles[CAMERA2_MAX_FACES][4];
	uint8_t			faceScores[CAMERA2_MAX_FACES];
	uint32_t		faceLandmarks[CAMERA2_MAX_FACES][6];
	uint32_t		faceIds[CAMERA2_MAX_FACES];
/* PAYTON_CHECK_20120712 : histogram_mode -> stats_mode */
	enum stats_mode		histogramMode;
/* [hj529.kim, 2012/07/19] androd.statistics.histogram */
	uint32_t		histogram[3 * 256];
/* PAYTON_CHECK_20120712 : sharpnessmap_mode -> stats_mode */
	enum stats_mode		sharpnessMapMode;
	/*sharpnessMap*/
	enum stats_lowlightmode LowLightMode;
	uint32_t lls_tuning_set_index;
	uint32_t lls_brightness_index;
};


struct camera2_stats_sm {
	uint8_t		availableFaceDetectModes[CAMERA2_MAX_AVAILABLE_MODE];
	/*assuming supported modes = 3;*/
	uint32_t	maxFaceCount;
	uint32_t	histogramBucketCount;
	uint32_t	maxHistogramCount;
	uint32_t	sharpnessMapSize[2];
	uint32_t	maxSharpnessMapValue;
};

/* android.control */

enum aa_capture_intent {
	AA_CAPTURE_INTENT_CUSTOM = 0,
	AA_CAPTURE_INTENT_PREVIEW,
	AA_CAPTURE_INTENT_STILL_CAPTURE,
	AA_CAPTURE_INTENT_VIDEO_RECORD,
	AA_CAPTURE_INTENT_VIDEO_SNAPSHOT,
	AA_CAPTURE_INTENT_ZERO_SHUTTER_LAG,
	AA_CAPTURE_INTENT_STILL_CAPTURE_OIS
};

enum aa_mode {
	AA_CONTROL_OFF = 1,
	AA_CONTROL_AUTO,
	AA_CONTROL_USE_SCENE_MODE
};

enum aa_scene_mode {
	AA_SCENE_MODE_UNSUPPORTED = 1,
	AA_SCENE_MODE_FACE_PRIORITY,
	AA_SCENE_MODE_ACTION,
	AA_SCENE_MODE_PORTRAIT,
	AA_SCENE_MODE_LANDSCAPE,
	AA_SCENE_MODE_NIGHT,
	AA_SCENE_MODE_NIGHT_PORTRAIT,
	AA_SCENE_MODE_THEATRE,
	AA_SCENE_MODE_BEACH,
	AA_SCENE_MODE_SNOW,
	AA_SCENE_MODE_SUNSET,
	AA_SCENE_MODE_STEADYPHOTO,
	AA_SCENE_MODE_FIREWORKS,
	AA_SCENE_MODE_SPORTS,
	AA_SCENE_MODE_PARTY,
	AA_SCENE_MODE_CANDLELIGHT,
	AA_SCENE_MODE_BARCODE,
	AA_SCENE_MODE_NIGHT_CAPTURE,
	AA_SCENE_MODE_ANTISHAKE,
	AA_SCENE_MODE_HDR,
	AA_SCENE_MODE_LLS,
	AA_SCENE_MODE_FDAE,
	AA_SCENE_MODE_DUAL,
	AA_SCENE_MODE_DRAMA,
	AA_SCENE_MODE_ANIMATED,
	AA_SCENE_MODE_PANORAMA,
	AA_SCENE_MODE_GOLF,
	AA_SCENE_MODE_PREVIEW,
	AA_SCENE_MODE_VIDEO,
	AA_SCENE_MODE_SLOWMOTION_2,
	AA_SCENE_MODE_SLOWMOTION_4_8,
	AA_SCENE_MODE_DUAL_PREVIEW,
	AA_SCENE_MODE_DUAL_VIDEO,
	AA_SCENE_MODE_120_PREVIEW,
	AA_SCENE_MODE_LIGHT_TRACE,
	AA_SCENE_MODE_FOOD
};

enum aa_effect_mode {
	AA_EFFECT_OFF = 1,
	AA_EFFECT_MONO,
	AA_EFFECT_NEGATIVE,
	AA_EFFECT_SOLARIZE,
	AA_EFFECT_SEPIA,
	AA_EFFECT_POSTERIZE,
	AA_EFFECT_WHITEBOARD,
	AA_EFFECT_BLACKBOARD,
	AA_EFFECT_AQUA
};

enum aa_aemode {
	AA_AEMODE_OFF = 1,
	AA_AEMODE_LOCKED,
	AA_AEMODE_CENTER,
	AA_AEMODE_AVERAGE,
	AA_AEMODE_MATRIX,
	AA_AEMODE_SPOT,
	AA_AEMODE_CENTER_TOUCH,
	AA_AEMODE_AVERAGE_TOUCH,
	AA_AEMODE_MATRIX_TOUCH,
	AA_AEMODE_SPOT_TOUCH
};

enum aa_ae_flashmode {
	/*all flash control stop*/
	AA_FLASHMODE_OFF = 1,
	/*flash start*/
	AA_FLASHMODE_START,
	/*flash cancle*/
	AA_FLASHMODE_CANCLE,
	/*internal 3A can control flash*/
	AA_FLASHMODE_ON,
	/*internal 3A can do auto flash algorithm*/
	AA_FLASHMODE_AUTO,
	/*internal 3A can fire flash by auto result*/
	AA_FLASHMODE_CAPTURE,
	/*internal 3A can control flash forced*/
	AA_FLASHMODE_ON_ALWAYS
};

enum aa_ae_antibanding_mode {
	AA_AE_ANTIBANDING_OFF = 1,
	AA_AE_ANTIBANDING_50HZ,
	AA_AE_ANTIBANDING_60HZ,
	AA_AE_ANTIBANDING_AUTO,
	AA_AE_ANTIBANDING_AUTO_50HZ,   /*50Hz + Auto*/
	AA_AE_ANTIBANDING_AUTO_60HZ    /*60Hz + Auto*/
};

enum aa_awbmode {
	AA_AWBMODE_OFF = 1,
	AA_AWBMODE_LOCKED,
	AA_AWBMODE_WB_AUTO,
	AA_AWBMODE_WB_INCANDESCENT,
	AA_AWBMODE_WB_FLUORESCENT,
	AA_AWBMODE_WB_WARM_FLUORESCENT,
	AA_AWBMODE_WB_DAYLIGHT,
	AA_AWBMODE_WB_CLOUDY_DAYLIGHT,
	AA_AWBMODE_WB_TWILIGHT,
	AA_AWBMODE_WB_SHADE
};

enum aa_afmode {
	/* These modes are adjusted immediatly */
	AA_AFMODE_OFF = 1,
	AA_AFMODE_SLEEP,
	AA_AFMODE_INFINITY,
	AA_AFMODE_MACRO,
	AA_AFMODE_DELAYED_OFF,

	/* Single AF. These modes are adjusted when afTrigger is changed from 0 to 1 */
	AA_AFMODE_AUTO = 11,
	AA_AFMODE_AUTO_MACRO,
	AA_AFMODE_AUTO_VIDEO,
	AA_AFMODE_AUTO_FACE,

	/* Continuous AF. These modes are adjusted when afTrigger is changed from 0 to 1 */
	AA_AFMODE_CONTINUOUS_PICTURE = 21,
	AA_AFMODE_CONTINUOUS_VIDEO,
	AA_AFMODE_CONTINUOUS_PICTURE_FACE,

	/* Special modes for PDAF */
	AA_AFMODE_PDAF_OUTFOCUSING = 31,
	AA_AFMODE_PDAF_OUTFOCUSING_FACE,
	AA_AFMODE_PDAF_OUTFOCUSING_CONTINUOUS_PICTURE,
	AA_AFMODE_PDAF_OUTFOCUSING_CONTINUOUS_PICTURE_FACE,

	/* Not supported yet */
	AA_AFMODE_EDOF = 41,
};

/* camera2_aa_ctl.afRegions[4] */
enum aa_afmode_ext {
	AA_AFMODE_EXT_OFF = 1000,
	/* Increase macro range for special app */
	AA_AFMODE_EXT_ADVANCED_MACRO_FOCUS = 1001,
	/* Set AF region for OCR */
	AA_AFMODE_EXT_FOCUS_LOCATION = 1002,
};

enum aa_afstate {
	AA_AFSTATE_INACTIVE = 1,
	AA_AFSTATE_PASSIVE_SCAN,
	AA_AFSTATE_ACTIVE_SCAN,
	AA_AFSTATE_AF_ACQUIRED_FOCUS,
	AA_AFSTATE_AF_FAILED_FOCUS
};

enum ae_state {
	AE_STATE_INACTIVE = 1,
	AE_STATE_SEARCHING,
	AE_STATE_CONVERGED,
	AE_STATE_LOCKED,
	AE_STATE_FLASH_REQUIRED,
	AE_STATE_PRECAPTURE
};

enum awb_state {
	AWB_STATE_INACTIVE = 1,
	AWB_STATE_SEARCHING,
	AWB_STATE_CONVERGED,
	AWB_STATE_LOCKED
};

enum aa_isomode {
	AA_ISOMODE_AUTO = 1,
	AA_ISOMODE_MANUAL,
};

struct camera2_aa_ctl {
	enum aa_capture_intent		captureIntent;
	enum aa_mode			mode;
	/*enum aa_effect_mode		effectMode;*/
	enum aa_scene_mode		sceneMode;
	uint32_t			videoStabilizationMode;
	enum aa_aemode			aeMode;
	uint32_t			aeRegions[5];
	/*5 per region(x1,y1,x2,y2,weight). currently assuming 1 region.*/
	int32_t				aeExpCompensation;
	uint32_t			aeTargetFpsRange[2];
	enum aa_ae_antibanding_mode	aeAntibandingMode;
	enum aa_ae_flashmode		aeflashMode;
	enum aa_awbmode			awbMode;
	uint32_t			awbRegions[5];
	/*5 per region(x1,y1,x2,y2,weight). currently assuming 1 region.*/
	enum aa_afmode			afMode;
	uint32_t			afRegions[5];
	/*5 per region(x1,y1,x2,y2,weight). currently assuming 1 region.*/
	uint32_t			afTrigger;
	enum aa_isomode			isoMode;
	uint32_t			isoValue;
	int32_t				awbValue;
	uint32_t			reserved[10];
};

struct camera2_aa_dm {
	enum aa_mode				mode;
	enum aa_effect_mode			effectMode;
	enum aa_scene_mode			sceneMode;
	uint32_t				videoStabilizationMode;
	enum aa_aemode				aeMode;
	/*needs check*/
	uint32_t				aeRegions[5];
	/*5 per region(x1,y1,x2,y2,weight). currently assuming 1 region.*/
	enum ae_state				aeState;
	enum aa_ae_flashmode			aeflashMode;
	/*needs check*/
	enum aa_awbmode				awbMode;
	uint32_t				awbRegions[5];
	enum awb_state				awbState;
	/*5 per region(x1,y1,x2,y2,weight). currently assuming 1 region.*/
	enum aa_afmode				afMode;
	uint32_t				afRegions[5];
	/*5 per region(x1,y1,x2,y2,weight). currently assuming 1 region*/
	enum aa_afstate				afState;
	enum aa_isomode				isoMode;
	uint32_t				isoValue;
	uint32_t				reserved[10];
};

struct camera2_aa_sm {
	uint8_t		availableSceneModes[CAMERA2_MAX_AVAILABLE_MODE];
	uint8_t		availableEffects[CAMERA2_MAX_AVAILABLE_MODE];
	/*assuming # of available scene modes = 10*/
	uint32_t	maxRegions;
	uint8_t		aeAvailableModes[CAMERA2_MAX_AVAILABLE_MODE];
	/*assuming # of available ae modes = 8*/
	struct rational	aeCompensationStep;
	int32_t		aeCompensationRange[2];
	uint32_t aeAvailableTargetFpsRanges[CAMERA2_MAX_AVAILABLE_MODE][2];
	uint8_t		aeAvailableAntibandingModes[CAMERA2_MAX_AVAILABLE_MODE];
	uint8_t		awbAvailableModes[CAMERA2_MAX_AVAILABLE_MODE];
	/*assuming # of awbAvailableModes = 10*/
	uint8_t		afAvailableModes[CAMERA2_MAX_AVAILABLE_MODE];
	/*assuming # of afAvailableModes = 4*/
	uint8_t availableVideoStabilizationModes[4];
	/*assuming # of availableVideoStabilizationModes = 4*/
	uint32_t	isoRange[2];
};

struct camera2_lens_usm {
	/** Frame delay between sending command and applying frame data */
	uint32_t	focusDistanceFrameDelay;
};

struct camera2_sensor_usm {
	/** Frame delay between sending command and applying frame data */
	uint32_t	exposureTimeFrameDelay;
	uint32_t	frameDurationFrameDelay;
	uint32_t	sensitivityFrameDelay;
};

struct camera2_flash_usm {
	/** Frame delay between sending command and applying frame data */
	uint32_t	flashModeFrameDelay;
	uint32_t	firingPowerFrameDelay;
	uint64_t	firingTimeFrameDelay;
};

struct camera2_ctl {
	struct camera2_request_ctl		request;
	struct camera2_lens_ctl			lens;
	struct camera2_sensor_ctl		sensor;
	struct camera2_flash_ctl		flash;
	struct camera2_hotpixel_ctl		hotpixel;
	struct camera2_demosaic_ctl		demosaic;
	struct camera2_noisereduction_ctl	noise;
	struct camera2_shading_ctl		shading;
	struct camera2_geometric_ctl		geometric;
	struct camera2_colorcorrection_ctl	color;
	struct camera2_tonemap_ctl		tonemap;
	struct camera2_edge_ctl			edge;
	struct camera2_scaler_ctl		scaler;
	struct camera2_jpeg_ctl			jpeg;
	struct camera2_stats_ctl		stats;
	struct camera2_aa_ctl			aa;
	struct camera2_entry_ctl		entry;
};

struct camera2_dm {
	struct camera2_request_dm		request;
	struct camera2_lens_dm			lens;
	struct camera2_sensor_dm		sensor;
	struct camera2_flash_dm			flash;
	struct camera2_hotpixel_dm		hotpixel;
	struct camera2_demosaic_dm		demosaic;
	struct camera2_noisereduction_dm	noise;
	struct camera2_shading_dm		shading;
	struct camera2_geometric_dm		geometric;
	struct camera2_colorcorrection_dm	color;
	struct camera2_tonemap_dm		tonemap;
	struct camera2_edge_dm			edge;
	struct camera2_scaler_dm		scaler;
	struct camera2_jpeg_dm			jpeg;
	struct camera2_stats_dm			stats;
	struct camera2_aa_dm			aa;
	struct camera2_entry_dm			entry;
};

struct camera2_sm {
	struct camera2_lens_sm			lens;
	struct camera2_sensor_sm		sensor;
	struct camera2_flash_sm			flash;
	struct camera2_colorcorrection_sm	color;
	struct camera2_tonemap_sm		tonemap;
	struct camera2_scaler_sm		scaler;
	struct camera2_jpeg_sm			jpeg;
	struct camera2_stats_sm			stats;
	struct camera2_aa_sm			aa;

	/** User-defined(ispfw specific) static metadata. */
	struct camera2_lens_usm			lensUd;
	struct camera2_sensor_usm		sensorUd;
	struct camera2_flash_usm		flashUd;
};

/** \brief
	User-defined control for lens.
*/
struct camera2_lens_uctl {
	struct camera2_lens_ctl ctl;

	/** It depends by posSize */
	uint32_t        pos;
	/** It depends by af algorithm(AF pos bit. normally 8 or 9 or 10) */
	uint32_t        posSize;
	/** It depends by af algorithm */
	uint32_t        direction;
	/** Some actuator support slew rate control. */
	uint32_t        slewRate;
};

/** \brief
	User-defined metadata for lens.
*/
struct camera2_lens_udm {
	/** It depends by posSize */
	uint32_t        pos;
	/** It depends by af algorithm(AF pos bit. normally 8 or 9 or 10) */
	uint32_t        posSize;
	/** It depends by af algorithm */
	uint32_t        direction;
	/** Some actuator support slew rate control. */
	uint32_t        slewRate;
};

/** \brief
 User-defined metadata for ae.
*/
struct camera2_ae_udm {
	/** vendor specific length */
	uint32_t	vsLength;
	/** vendor specific data array */
	uint32_t	vendorSpecific[CAMERA2_MAX_VENDER_LENGTH];
};

/** \brief
 User-defined metadata for awb.
*/
struct camera2_awb_udm {
	/** vendor specific length */
	uint32_t	vsLength;
	/** vendor specific data array */
	uint32_t	vendorSpecific[CAMERA2_MAX_VENDER_LENGTH];
};

/** \brief
 User-defined metadata for af.
*/
struct camera2_af_udm {
	/** vendor specific length */
	uint32_t	vsLength;
	/** vendor specific data array */
	uint32_t	vendorSpecific[CAMERA2_MAX_VENDER_LENGTH];
	int32_t         lensPositionInfinity;
	int32_t         lensPositionMacro;
	int32_t         lensPositionCurrent;
};

/** \brief
 User-defined metadata for anti-shading.
*/
struct camera2_as_udm {
	/** vendor specific length */
	uint32_t vsLength;
	/** vendor specific data array */
	uint32_t vendorSpecific[CAMERA2_MAX_VENDER_LENGTH];
};

/** \brief
 User-defined metadata for anti-shading.
*/
struct camera2_ipc_udm {
	/** vendor specific length */
	uint32_t vsLength;
	/** vendor specific data array */
	uint32_t vendorSpecific[CAMERA2_MAX_VENDER_LENGTH];
};

/** \brief
 User-defined metadata for aa.
*/
struct camera2_internal_udm {
	/** vendor specific data array */
	uint32_t vendorSpecific1[CAMERA2_MAX_VENDER_LENGTH];
	uint32_t vendorSpecific2[CAMERA2_MAX_VENDER_LENGTH];
	/*
	 * vendorSpecific2[0] : info
	 * vendorSpecific2[100] : 0:sirc 1:cml
	 * vendorSpecific2[101] : cml exposure
	 * vendorSpecific2[102] : cml iso(gain)
	 * vendorSpecific2[103] : cml Bv
	 */
};

/** \brief
	User-defined control for sensor.
*/
struct camera2_sensor_uctl {
	struct camera2_sensor_ctl ctl;
	/** Dynamic frame duration.
	This feature is decided to max. value between
	'sensor.exposureTime'+alpha and 'sensor.frameDuration'.
	*/
	uint64_t        dynamicFrameDuration;
	uint32_t	analogGain;
	uint32_t	digitalGain;
	uint64_t	longExposureTime; /* For supporting WDR */
	uint64_t	shortExposureTime;
	uint32_t	longAnalogGain;
	uint32_t	shortAnalogGain;
	uint32_t	longDigitalGain;
	uint32_t	shortDigitalGain;
};

struct camera2_scaler_uctl {
	/** \brief
	target address for next frame.
	\remarks
	[0] invalid address, stop
	[others] valid address
	*/
	uint32_t sccTargetAddress[4];
	uint32_t scpTargetAddress[4];
	uint32_t disTargetAddress[4];
	uint32_t taapTargetAddress[4]; /* 3AA preview DMA */
	uint32_t taacTargetAddress[4]; /* 3AA capture DMA */
	uint32_t orientation;
};

struct camera2_flash_uctl {
	struct camera2_flash_ctl ctl;
};

struct camera2_bayer_uctl {
	struct camera2_scaler_ctl ctl;
};

enum companion_drc_mode {
	COMPANION_DRC_OFF = 1,
	COMPANION_DRC_ON,
};

enum companion_wdr_mode {
	COMPANION_WDR_OFF = 1,
	COMPANION_WDR_ON,
};

enum companion_paf_mode {
	COMPANION_PAF_OFF = 1,
	COMPANION_PAF_ON,
};

enum companion_bypass_mode {
	COMPANION_FULL_BYPASS_OFF = 1,
	COMPANION_FULL_BYPASS_ON,
};

enum companion_lsc_mode {
	COMPANION_LSC_OFF = 1,
	COMPANION_LSC_ON,
};

struct camera2_companion_uctl {
	enum companion_drc_mode drc_mode;
	enum companion_wdr_mode wdr_mode;
	enum companion_paf_mode paf_mode;
};

struct camera2_bayer_udm {
	uint32_t	width;
	uint32_t	height;
};

struct camera2_pdaf_single_result {
	uint16_t        mode;
	uint16_t        goalPos;
	uint16_t        reliability;
	uint16_t        currentPos;
};

struct camera2_pdaf_multi_result {
	uint16_t        mode;
	uint16_t        goalPos;
	uint16_t        reliability;
};

struct camera2_pdaf_udm {
	uint16_t				numCol;	/* width of PDAF map, 0 means no multi PDAF data */
	uint16_t				numRow;	/* height of PDAF map, 0 means no multi PDAF data */
	struct camera2_pdaf_multi_result	multiResult[CAMERA2_MAX_PDAF_MULTIROI_COLUMN][CAMERA2_MAX_PDAF_MULTIROI_ROW];
	struct camera2_pdaf_single_result	singleResult;
	uint16_t				lensPosResolution;	/* 1023(unsigned 10bit) */
};

struct camera2_companion_udm {
	enum companion_drc_mode drc_mode;
	enum companion_wdr_mode wdr_mode;
	enum companion_paf_mode paf_mode;
	struct camera2_pdaf_udm pdaf;
};

/** \brief
	User-defined control area.
    \remarks
	sensor, lens, flash category is empty value.
	It should be filled by a5 for SET_CAM_CONTROL command.
	Other category is filled already from host.
*/
struct camera2_uctl {
	/** \brief
	Set sensor, lens, flash control for next frame.
	\remarks
	This flag can be combined.
	[0 bit] lens
	[1 bit] sensor
	[2 bit] flash
	*/
	uint32_t uUpdateBitMap;

	/** For debugging */
	uint32_t uFrameNumber;

	/** ispfw specific control(user-defined) of lens. */
	struct camera2_lens_uctl	lensUd;
	/** ispfw specific control(user-defined) of sensor. */
	struct camera2_sensor_uctl	sensorUd;
	/** ispfw specific control(user-defined) of flash. */
	struct camera2_flash_uctl	flashUd;

	struct camera2_scaler_uctl	scalerUd;
	/** ispfw specific control(user-defined) of Bcrop1. */
	struct camera2_bayer_uctl	bayerUd;
	struct camera2_companion_uctl   companionUd;
	uint32_t			reserved[10];
};

struct camera2_udm {
	struct camera2_lens_udm		lens;
	struct camera2_ae_udm		ae;
	struct camera2_awb_udm		awb;
	struct camera2_af_udm		af;
	struct camera2_as_udm		as;
	struct camera2_ipc_udm		ipc;
	/* KJ_121129 : Add udm for sirc sdk. */
	struct camera2_internal_udm	internal;
	/* Add udm for bayer down size. */
	struct camera2_bayer_udm	bayer;
	struct camera2_companion_udm	companion;
	uint32_t			reserved[10];
};

struct camera2_shot {
	/*google standard area*/
	struct camera2_ctl	ctl;
	struct camera2_dm	dm;
	/*user defined area*/
	struct camera2_uctl	uctl;
	struct camera2_udm	udm;
	/*magic : 23456789*/
	uint32_t		magicNumber;
};

struct camera2_node_input {
	/**	\brief
		intput crop region
		\remarks
		[0] x axis
		[1] y axie
		[2] width
		[3] height
	*/
	uint32_t	cropRegion[4];
};

struct camera2_node_output {
	/**	\brief
		output crop region
		\remarks
		[0] x axis
		[1] y axie
		[2] width
		[3] height
	*/
	uint32_t	cropRegion[4];
};

struct camera2_node {
	/**	\brief
		video node id
		\remarks
		[x] video node id
	*/
	uint32_t			vid;

	/**	\brief
		stream control
		\remarks
		[0] disable stream out
		[1] enable stream out
	*/
	uint32_t			request;

	struct camera2_node_input	input;
	struct camera2_node_output	output;
};

struct camera2_node_group {
	/**	\brief
		output device node
		\remarks
		this node can pull in image
	*/
	struct camera2_node		leader;

	/**	\brief
		capture node list
		\remarks
		this node can get out image
		3AAC, 3AAP, SCC, SCP, VDISC
	*/
	struct camera2_node		capture[CAPTURE_NODE_MAX];
};

/** \brief
	Structure for interfacing between HAL and driver.
*/
struct camera2_shot_ext {
	/*
	 * ---------------------------------------------------------------------
	 * HAL Control Part
	 * ---------------------------------------------------------------------
	 */

	/**	\brief
		setfile change
		\remarks
		[x] mode for setfile
	*/
	uint32_t			setfile;

	/**	\brief
		node group control
		\remarks
		per frame control
	*/
	struct camera2_node_group	node_group;

	/**	\brief
		post processing control(DRC)
		\remarks
		[0] bypass off
		[1] bypass on
	*/
	uint32_t			drc_bypass;

	/**	\brief
		post processing control(DIS)
		\remarks
		[0] bypass off
		[1] bypass on
	*/
	uint32_t			dis_bypass;

	/**	\brief
		post processing control(3DNR)
		\remarks
		[0] bypass off
		[1] bypass on
	*/
	uint32_t			dnr_bypass;

	/**	\brief
		post processing control(FD)
		\remarks
		[0] bypass off
		[1] bypass on
	*/
	uint32_t			fd_bypass;

	/*
	 * ---------------------------------------------------------------------
	 * DRV Control Part
	 * ---------------------------------------------------------------------
	 */

	/**	\brief
		requested frames state.
		driver return the information everytime
		when dequeue is requested.
		\remarks
		[X] count
	*/
	uint32_t			free_cnt;
	uint32_t			request_cnt;
	uint32_t			process_cnt;
	uint32_t			complete_cnt;

	/* reserved for future */
	uint32_t			reserved[15];

	/**	\brief
		processing time debugging
		\remarks
		taken time(unit : struct timeval)
		[0][x] flite start
		[1][x] flite end
		[2][x] DRV Shot
		[3][x] DRV Shot done
		[4][x] DRV Meta done
	*/
	uint32_t			timeZone[10][2];

	/*
	 * ---------------------------------------------------------------------
	 * Camera API
	 * ---------------------------------------------------------------------
	 */

	struct camera2_shot		shot;
};

/** \brief
	stream structure for scaler.
*/
struct camera2_stream {
	/**	\brief
		this address for verifying conincidence of index and address
		\remarks
		[X] kernel virtual address for this buffer
	*/
	uint32_t		address;

	/**	\brief
		this frame count is from FLITE through dm.request.fcount,
		this count increases every frame end. initial value is 1.
		\remarks
		[X] frame count
	*/
	uint32_t		fcount;

	/**	\brief
		this request count is from HAL through ctl.request.fcount,
		this count is the unique.
		\remarks
		[X] request count
	*/
	uint32_t		rcount;

	/**	\brief
		frame index of isp framemgr.
		this value is for driver internal debugging
		\remarks
		[X] frame index
	*/
	uint32_t		findex;

	/**	\brief
		frame validation of isp framemgr.
		this value is for driver and HAL internal debugging
		\remarks
		[X] frame valid
	*/
	uint32_t		fvalid;

	/**	\brief
		output crop region
		this value mean the output image places the axis of  memory space
		\remarks
		[0] crop x axis
		[1] crop y axis
		[2] width
		[3] height
	*/
	uint32_t		input_crop_region[4];
	uint32_t		output_crop_region[4];
};

#define CAM_LENS_CMD		(0x1 << 0x0)
#define CAM_SENSOR_CMD		(0x1 << 0x1)
#define CAM_FLASH_CMD		(0x1 << 0x2)

/* typedefs below are for firmware sources */

typedef enum metadata_mode metadata_mode_t;
typedef struct camera2_request_ctl camera2_request_ctl_t;
typedef struct camera2_request_dm camera2_request_dm_t;
typedef enum optical_stabilization_mode optical_stabilization_mode_t;
typedef enum lens_facing lens_facing_t;
typedef struct camera2_entry_ctl camera2_entry_ctl_t;
typedef struct camera2_entry_dm camera2_entry_dm_t;
typedef struct camera2_lens_ctl camera2_lens_ctl_t;
typedef struct camera2_lens_dm camera2_lens_dm_t;
typedef struct camera2_lens_sm camera2_lens_sm_t;
typedef enum sensor_colorfilterarrangement sensor_colorfilterarrangement_t;
typedef enum sensor_ref_illuminant sensor_ref_illuminant_t;
typedef struct camera2_sensor_ctl camera2_sensor_ctl_t;
typedef struct camera2_sensor_dm camera2_sensor_dm_t;
typedef struct camera2_sensor_sm camera2_sensor_sm_t;
typedef enum flash_mode flash_mode_t;
typedef struct camera2_flash_ctl camera2_flash_ctl_t;
typedef struct camera2_flash_dm camera2_flash_dm_t;
typedef struct camera2_flash_sm camera2_flash_sm_t;
typedef enum processing_mode processing_mode_t;
typedef struct camera2_hotpixel_ctl camera2_hotpixel_ctl_t;
typedef struct camera2_hotpixel_dm camera2_hotpixel_dm_t;

typedef struct camera2_demosaic_ctl camera2_demosaic_ctl_t;
typedef struct camera2_demosaic_dm camera2_demosaic_dm_t;
typedef struct camera2_noisereduction_ctl camera2_noisereduction_ctl_t;
typedef struct camera2_noisereduction_dm camera2_noisereduction_dm_t;
typedef struct camera2_shading_ctl camera2_shading_ctl_t;
typedef struct camera2_shading_dm camera2_shading_dm_t;
typedef struct camera2_geometric_ctl camera2_geometric_ctl_t;
typedef struct camera2_geometric_dm camera2_geometric_dm_t;
typedef enum colorcorrection_mode colorcorrection_mode_t;
typedef struct camera2_colorcorrection_ctl camera2_colorcorrection_ctl_t;
typedef struct camera2_colorcorrection_dm camera2_colorcorrection_dm_t;
typedef struct camera2_colorcorrection_sm camera2_colorcorrection_sm_t;
typedef enum tonemap_mode tonemap_mode_t;
typedef struct camera2_tonemap_ctl camera2_tonemap_ctl_t;
typedef struct camera2_tonemap_dm camera2_tonemap_dm_t;
typedef struct camera2_tonemap_sm camera2_tonemap_sm_t;

typedef struct camera2_edge_ctl camera2_edge_ctl_t;
typedef struct camera2_edge_dm camera2_edge_dm_t;
typedef enum scaler_availableformats scaler_availableformats_t;
typedef struct camera2_scaler_ctl camera2_scaler_ctl_t;
typedef struct camera2_scaler_dm camera2_scaler_dm_t;
typedef struct camera2_jpeg_ctl camera2_jpeg_ctl_t;
typedef struct camera2_jpeg_dm camera2_jpeg_dm_t;
typedef struct camera2_jpeg_sm camera2_jpeg_sm_t;
typedef enum facedetect_mode facedetect_mode_t;
typedef enum stats_mode stats_mode_t;
typedef struct camera2_stats_ctl camera2_stats_ctl_t;
typedef struct camera2_stats_dm camera2_stats_dm_t;
typedef struct camera2_stats_sm camera2_stats_sm_t;
typedef enum aa_capture_intent aa_capture_intent_t;
typedef enum aa_mode aa_mode_t;
typedef enum aa_scene_mode aa_scene_mode_t;
typedef enum aa_effect_mode aa_effect_mode_t;
typedef enum aa_aemode aa_aemode_t;
typedef enum aa_ae_antibanding_mode aa_ae_antibanding_mode_t;
typedef enum aa_awbmode aa_awbmode_t;
typedef enum aa_afmode aa_afmode_t;
typedef enum aa_afstate aa_afstate_t;
typedef struct camera2_aa_ctl camera2_aa_ctl_t;
typedef struct camera2_aa_dm camera2_aa_dm_t;
typedef struct camera2_aa_sm camera2_aa_sm_t;
typedef struct camera2_lens_usm camera2_lens_usm_t;
typedef struct camera2_sensor_usm camera2_sensor_usm_t;
typedef struct camera2_flash_usm camera2_flash_usm_t;
typedef struct camera2_ctl camera2_ctl_t;
typedef struct camera2_uctl camera2_uctl_t;
typedef struct camera2_dm camera2_dm_t;
typedef struct camera2_sm camera2_sm_t;

typedef struct camera2_scaler_sm camera2_scaler_sm_t;
typedef struct camera2_scaler_uctl camera2_scaler_uctl_t;

typedef struct camera2_sensor_uctl camera2_sensor_uctl_t;
typedef struct camera2_lens_uctl camera2_lens_uctl_t;
typedef struct camera2_lens_udm camera2_lens_udm_t;

typedef struct camera2_ae_udm camera2_ae_udm_t;
typedef struct camera2_awb_udm camera2_awb_udm_t;
typedef struct camera2_af_udm camera2_af_udm_t;
typedef struct camera2_as_udm camera2_as_udm_t;
typedef struct camera2_ipc_udm camera2_ipc_udm_t;
typedef struct camera2_internal_udm camera2_internal_udm_t;

typedef struct camera2_flash_uctl camera2_flash_uctl_t;

typedef struct camera2_shot camera2_shot_t;

#endif
