#define PA_ASSERT_CMD(cmd) do { PaError err; \
    if (paNoError != (err = (cmd))) { \
        fprintf(stderr, "PortAudio error (%s): %s\n", #cmd, Pa_GetErrorText(err)); \
        exit(EXIT_FAILURE); \
    } } while(0)
