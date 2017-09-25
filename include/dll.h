#include <dlfcn.h>
#include <stdlib.h>

void (*fromLocal)(const op_t &op);
void (*fromNet)(const trans_t &msg);
const char *ALGO_VER;

static const char *libs[] = {
    "libalgo.so", "libcscw.so", "libcss.so"
};
#define NR_LIBS (sizeof(libs) / sizeof(libs[0]))

static inline const char *setDllFuncs(int is_client, int no) {
    void *libHandle;
    const char *err;
    if ((libHandle = dlopen(libs[no], RTLD_LAZY)) == NULL)
        return dlerror();
    dlerror();
    const char **version;
    version = (const char **)dlsym(libHandle, "ALGO_VER");
    if (is_client) {
        *(void **)(&fromLocal) = dlsym(libHandle, "procLocal");
        *(void **)(&fromNet) = dlsym(libHandle, "procServer");
    }
    else {
        *(void **)(&fromLocal) = dlsym(libHandle, "procRemote");
        *(void **)(&fromNet) = dlsym(libHandle, "procClient");
    }
    if ((err = dlerror()) != NULL) {
        dlclose(libHandle);
        return err;
    }
    if (fromLocal == NULL || fromNet == NULL || version == NULL) {
        dlclose(libHandle);
        return "algorithm function is NULL";
    }
    ALGO_VER = *version;
    return NULL;
}