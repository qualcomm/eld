template <typename T> class ThreadLocalStore {
public:
  static T *Get() {
    static __thread T *ptr = nullptr;
    if (ptr == nullptr) {
      ptr = new T();
    }
    return ptr;
  }
};

class DeviceAPI {
public:
  void *AllocWorkspace(unsigned nbytes);
  static DeviceAPI *Get();
};

class WorkspacePool {
public:
  WorkspacePool();
  void *AllocWorkspace(unsigned size);
};

void *DeviceAPI::AllocWorkspace(unsigned size) {
  return ThreadLocalStore<WorkspacePool>::Get()->AllocWorkspace(size);
}
