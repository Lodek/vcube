
/*
 * rand_init seeds unix's `rand` function with the current time.
 * This wrapper function avoids the nameclash with smpl's 
 * `time` definition
 */
void rand_init();
