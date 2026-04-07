// output message level control
#define OUTPUT_LEVEL_OFF -1	// used in OUTPUT_CONTROL macro definition
#define OUTPUT_LEVEL_INFO 0
#define OUTPUT_LEVEL_WARNING 1
#define OUTPUT_LEVEL_ERROR 2
#define OUTPUT_LEVEL_CRITICAL 3
#define OUTPUT_LEVEL_ON 4	// used in OUTPUT_CONTROL macro definition
#define OUTPUT_LEVEL_NONE 5	// used in OUTPUT_MASK_XXX macro definition
// debug output mask (will output corresponding level and above)
#define OUTPUT_MASK_ACQUISITION     OUTPUT_LEVEL_INFO
#define OUTPUT_MASK_COH_PROC        OUTPUT_LEVEL_WARNING
#define OUTPUT_MASK_TRACKING_LOOP   OUTPUT_LEVEL_WARNING
#define OUTPUT_MASK_TRACKING_SWITCH OUTPUT_LEVEL_WARNING
#define OUTPUT_MASK_DATA_DECODE     OUTPUT_LEVEL_WARNING
#define OUTPUT_MASK_MEASUREMENT     OUTPUT_LEVEL_WARNING
#define OUTPUT_MASK_OUTPUT          OUTPUT_LEVEL_WARNING
#define OUTPUT_MASK_PVT             OUTPUT_LEVEL_INFO

// configuration of measurement
#define DEFAULT_MEAS_INTERVAL 100	// in unit of 1ms
#define ENABLE_KALMAN_FILTER 1

// definition of local time assignment
#define USE_PRE_ASSIGNED_TIME	1 // define this macro if local time uses data read from baseband message (for post process only)

// storage control
//#define MAX_FILE_ID 6
//#define STORAGE_FILE_PATH "Storage\\"

// output message control
#define MAX_STREAM_ID 4
#define STREAM_FILE_PREFIX "Stream%1d.dat"
#define USE_STDOUT_AS_STREAM0 0
#define DEFAULT_DEBUG_OUTPUT_PORT -1	// -1 as force using stdout
#define DEFAULT_BB_MEAS_PORT -1
#define DEFAULT_BB_DATA_PORT -1
