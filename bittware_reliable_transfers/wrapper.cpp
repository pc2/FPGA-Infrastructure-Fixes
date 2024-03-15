#include <csignal>
#include <cstddef>
#include <cstring>
#include <dlfcn.h>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <pthread.h>
#include <set>
#include <string_view>
#include <sys/mman.h>

#include <aocl_mmd.h>

#define xstr(s) str(s)
#define str(s) #s

#ifndef BSP
#error "Cannot build without knowing target BSP. Set BSP variable."
#endif

constexpr bool DEBUG{false};
constexpr auto libbitt_path{xstr(BSP)};

constexpr unsigned char RANDOM_STRING[32]{
    u'\xb1', u'\x53', u'\xdc', u'\x2d', u'\xd4', u'\x9b', u'\x7d', u'\x81',
    u'\x5b', u'\x4c', u'\x63', u'\x81', u'\x5f', u'\xe6', u'\x51', u'\xf8',
    u'\xd6', u'\x30', u'\x9d', u'\x8d', u'\x7f', u'\x8b', u'\x07', u'\xab',
    u'\xad', u'\xff', u'\x65', u'\x74', u'\x1f', u'\x35', u'\xf7', u'\xcf'};

static std::map<int, int> gmem_interfaces{};
static std::mutex gmem_interfaces_lock{};
static std::map<int, aocl_mmd_status_handler_fn> registered_status_handlers{};
static std::mutex registered_status_handlers_lock{};
static std::set<void *> known_op_wrappings{};
static std::mutex known_op_wrappings_lock{};

template <typename T>
constexpr void check_symbol(T ptr, std::string_view name) {
  if (ptr == nullptr) {
    std::cerr << "PC2 WARNING: Function wrapped that does not exist: " << name
              << ". Please contact PC2 support!" << std::endl;
  }
}

class libbitt_t {
  void *handle{};

public:
  decltype(aocl_mmd_open) *aocl_mmd_open;
  decltype(aocl_mmd_set_status_handler) *aocl_mmd_set_status_handler;
  decltype(aocl_mmd_read) *aocl_mmd_read;
  decltype(aocl_mmd_get_offline_info) *aocl_mmd_get_offline_info;
  decltype(aocl_mmd_get_info) *aocl_mmd_get_info;
  decltype(aocl_mmd_close) *aocl_mmd_close;
  decltype(aocl_mmd_set_interrupt_handler) *aocl_mmd_set_interrupt_handler;
  decltype(aocl_mmd_yield) *aocl_mmd_yield;
  decltype(aocl_mmd_write) *aocl_mmd_write;
  decltype(aocl_mmd_program) *aocl_mmd_program;
  decltype(aocl_mmd_copy) *aocl_mmd_copy;

  decltype(aocl_mmd_sch_status) *aocl_mmd_sch_status;
  decltype(aocl_mmd_sch_ctrl) *aocl_mmd_sch_ctrl;
  decltype(aocl_mmd_sch_perfctrl) *aocl_mmd_sch_perfctrl;
  decltype(aocl_mmd_sch_rxperf) *aocl_mmd_sch_rxperf;
  decltype(aocl_mmd_sch_txperf) *aocl_mmd_sch_txperf;

  decltype(aocl_mmd_card_info) *aocl_mmd_card_info;
  decltype(aocl_mmd_set_device_interrupt_handler)
      *aocl_mmd_set_device_interrupt_handler;

  decltype(aocl_mmd_hostchannel_create) *aocl_mmd_hostchannel_create;
  decltype(aocl_mmd_hostchannel_destroy) *aocl_mmd_hostchannel_destroy;
  decltype(aocl_mmd_hostchannel_get_buffer) *aocl_mmd_hostchannel_get_buffer;
  decltype(aocl_mmd_hostchannel_ack_buffer) *aocl_mmd_hostchannel_ack_buffer;

  decltype(aocl_mmd_shared_mem_alloc) *aocl_mmd_shared_mem_alloc;
  decltype(aocl_mmd_shared_mem_free) *aocl_mmd_shared_mem_free;

#ifdef INCLUDE_AOCL_MMD_REPROGRAM
  int (*aocl_mmd_reprogram)(int, void *, size_t);
#endif

  libbitt_t() {
    handle = dlopen(libbitt_path, RTLD_NOW);

    aocl_mmd_open = reinterpret_cast<decltype(aocl_mmd_open)>(
        dlsym(handle, "aocl_mmd_open"));
    check_symbol(aocl_mmd_open, "aocl_mmd_open");

    aocl_mmd_set_status_handler =
        reinterpret_cast<decltype(aocl_mmd_set_status_handler)>(
            dlsym(handle, "aocl_mmd_set_status_handler"));
    check_symbol(aocl_mmd_set_status_handler, "aocl_mmd_set_status_handler");

    aocl_mmd_read = reinterpret_cast<decltype(aocl_mmd_read)>(
        dlsym(handle, "aocl_mmd_read"));
    check_symbol(aocl_mmd_read, "aocl_mmd_read");

    aocl_mmd_get_offline_info =
        reinterpret_cast<decltype(aocl_mmd_get_offline_info)>(
            dlsym(handle, "aocl_mmd_get_offline_info"));
    check_symbol(aocl_mmd_get_offline_info, "aocl_mmd_get_offline_info");

    aocl_mmd_get_info = reinterpret_cast<decltype(aocl_mmd_get_info)>(
        dlsym(handle, "aocl_mmd_get_info"));
    check_symbol(aocl_mmd_get_info, "aocl_mmd_get_info");

    aocl_mmd_close = reinterpret_cast<decltype(aocl_mmd_close)>(
        dlsym(handle, "aocl_mmd_close"));
    check_symbol(aocl_mmd_close, "aocl_mmd_close");

    aocl_mmd_set_interrupt_handler =
        reinterpret_cast<decltype(aocl_mmd_set_interrupt_handler)>(
            dlsym(handle, "aocl_mmd_set_interrupt_handler"));
    check_symbol(aocl_mmd_set_interrupt_handler,
                 "aocl_mmd_set_interrupt_handler");

    aocl_mmd_yield = reinterpret_cast<decltype(aocl_mmd_yield)>(
        dlsym(handle, "aocl_mmd_yield"));
    check_symbol(aocl_mmd_yield, "aocl_mmd_yield");

    aocl_mmd_write = reinterpret_cast<decltype(aocl_mmd_write)>(
        dlsym(handle, "aocl_mmd_write"));
    check_symbol(aocl_mmd_write, "aocl_mmd_write");

    aocl_mmd_program = reinterpret_cast<decltype(aocl_mmd_program)>(
        dlsym(handle, "aocl_mmd_program"));
    check_symbol(aocl_mmd_program, "aocl_mmd_program");

    aocl_mmd_copy = reinterpret_cast<decltype(aocl_mmd_copy)>(
        dlsym(handle, "aocl_mmd_copy"));
    check_symbol(aocl_mmd_copy, "aocl_mmd_copy");

    aocl_mmd_sch_status = reinterpret_cast<decltype(aocl_mmd_sch_status)>(
        dlsym(handle, "aocl_mmd_sch_status"));
    check_symbol(aocl_mmd_sch_status, "aocl_mmd_sch_status");

    aocl_mmd_sch_ctrl = reinterpret_cast<decltype(aocl_mmd_sch_ctrl)>(
        dlsym(handle, "aocl_mmd_sch_ctrl"));
    check_symbol(aocl_mmd_sch_ctrl, "aocl_mmd_sch_ctrl");

    aocl_mmd_sch_perfctrl = reinterpret_cast<decltype(aocl_mmd_sch_perfctrl)>(
        dlsym(handle, "aocl_mmd_sch_perfctrl"));
    check_symbol(aocl_mmd_sch_perfctrl, "aocl_mmd_sch_perfctrl");

    aocl_mmd_sch_rxperf = reinterpret_cast<decltype(aocl_mmd_sch_rxperf)>(
        dlsym(handle, "aocl_mmd_sch_rxperf"));
    check_symbol(aocl_mmd_sch_rxperf, "aocl_mmd_sch_rxperf");

    aocl_mmd_sch_txperf = reinterpret_cast<decltype(aocl_mmd_sch_txperf)>(
        dlsym(handle, "aocl_mmd_sch_txperf"));
    check_symbol(aocl_mmd_sch_txperf, "aocl_mmd_sch_txperf");

    aocl_mmd_card_info = reinterpret_cast<decltype(aocl_mmd_card_info)>(
        dlsym(handle, "aocl_mmd_card_info"));
    check_symbol(aocl_mmd_card_info, "aocl_mmd_card_info");

    aocl_mmd_set_device_interrupt_handler =
        reinterpret_cast<decltype(aocl_mmd_set_device_interrupt_handler)>(
            dlsym(handle, "aocl_mmd_set_device_interrupt_handler"));
    check_symbol(aocl_mmd_set_device_interrupt_handler,
                 "aocl_mmd_set_device_interrupt_handler");

    aocl_mmd_hostchannel_create =
        reinterpret_cast<decltype(aocl_mmd_hostchannel_create)>(
            dlsym(handle, "aocl_mmd_hostchannel_create"));
    check_symbol(aocl_mmd_hostchannel_create, "aocl_mmd_hostchannel_create");

    aocl_mmd_hostchannel_destroy =
        reinterpret_cast<decltype(aocl_mmd_hostchannel_destroy)>(
            dlsym(handle, "aocl_mmd_hostchannel_destroy"));
    check_symbol(aocl_mmd_hostchannel_destroy, "aocl_mmd_hostchannel_destroy");

    aocl_mmd_hostchannel_get_buffer =
        reinterpret_cast<decltype(aocl_mmd_hostchannel_get_buffer)>(
            dlsym(handle, "aocl_mmd_hostchannel_get_buffer"));
    check_symbol(aocl_mmd_hostchannel_get_buffer,
                 "aocl_mmd_hostchannel_get_buffer");

    aocl_mmd_hostchannel_ack_buffer =
        reinterpret_cast<decltype(aocl_mmd_hostchannel_ack_buffer)>(
            dlsym(handle, "aocl_mmd_hostchannel_ack_buffer"));
    check_symbol(aocl_mmd_hostchannel_ack_buffer,
                 "aocl_mmd_hostchannel_ack_buffer");

    aocl_mmd_shared_mem_alloc =
        reinterpret_cast<decltype(aocl_mmd_shared_mem_alloc)>(
            dlsym(handle, "aocl_mmd_shared_mem_alloc"));
    check_symbol(aocl_mmd_shared_mem_alloc, "aocl_mmd_shared_mem_alloc");

    aocl_mmd_shared_mem_free =
        reinterpret_cast<decltype(aocl_mmd_shared_mem_free)>(
            dlsym(handle, "aocl_mmd_shared_mem_free"));
    check_symbol(aocl_mmd_shared_mem_free, "aocl_mmd_shared_mem_free");

#ifdef INCLUDE_AOCL_MMD_REPROGRAM
    aocl_mmd_reprogram = reinterpret_cast<decltype(aocl_mmd_reprogram)>(
        dlsym(handle, "aocl_mmd_reprogram"));
    check_symbol(aocl_mmd_reprogram, "aocl_mmd_reprogram");
#endif
  }
};
static const libbitt_t libbitt{};

class env_t {
public:
  bool use_mlock{false};
  env_t() {
    if (getenv("BITTFIX_MLOCK")) {
      use_mlock = true;
    }

    std::cerr << "PC2 Bittware 520n reliable data transfer patch active. mlock "
                 "all pages "
              << (use_mlock ? "" : "not ") << "activated.\n";
  }
};
static const env_t env{};

typedef struct {
  aocl_mmd_op_t op;
  size_t len;
  void *dst;
  int interface;
  size_t offset;
  bool obsolete;
} wrapped_aocl_mmd_op_t;

class signal_guard {
  sigset_t new_mask, old_mask;

public:
  signal_guard() {
    sigfillset(&new_mask);
    pthread_sigmask(SIG_SETMASK, &new_mask, &old_mask);
  }
  ~signal_guard() { sigprocmask(SIG_SETMASK, &old_mask, NULL); }
};

// Useful for debugging. Commented out as it's currently unused.
/*
static void pretty_print(unsigned char *buf, size_t bufsize) {
  for (size_t i = 0; i < bufsize; i += 16) {
    for (size_t j = 0; j < 8; j++) {
      if ((i + j) >= bufsize)
        break;
      std::cout << std::hex << std::uppercase << std::setfill('0')
                << std::setw(2) << static_cast<unsigned>(buf[i + j]) << " ";
    }
    std::cout << "    ";
    for (size_t j = 8; j < 16; j++) {
      if ((i + j) >= bufsize)
        break;
      std::cout << std::hex << std::uppercase << std::setfill('0')
                << std::setw(2) << static_cast<unsigned>(buf[i + j]) << " ";
    }
    std::cout << std::endl;
  }
}
*/

static void stamp_pages(void *dst, size_t len) {
  uintptr_t first_byte, last_byte, first_page, last_page;
  first_byte = reinterpret_cast<uintptr_t>(dst);
  last_byte = first_byte + len - 1;
  first_page = first_byte >> 12;
  last_page = last_byte >> 12;

  if (DEBUG)
    std::cout << std::hex << first_byte << " (" << first_page << ") - "
              << last_byte << " (" << last_page << ")\n";

  // prefault all pages
  int err{0};
  if (env.use_mlock) {
    err = mlock(dst, len);
  }

  // for now only stamp first 32 byte of a page
  // => first page will be skipped if not aligned
  for (uintptr_t pp = first_page << 12; pp <= last_page << 12; pp += 4096) {
    if (pp < first_byte || pp + 31 > last_byte) {
      continue;
    }
    void *page_ptr = reinterpret_cast<void *>(pp);
    std::memcpy(page_ptr, RANDOM_STRING, 32);
  }

  if (env.use_mlock && !err) {
    munlock(dst, len);
  }
}

static int check_pages(void *dst, size_t len) {
  uintptr_t first_byte, last_byte, first_page, last_page;
  first_byte = reinterpret_cast<uintptr_t>(dst);
  last_byte = first_byte + len - 1;
  first_page = first_byte >> 12;
  last_page = last_byte >> 12;

  if (DEBUG)
    std::cout << std::hex << first_byte << " (" << first_page << ") - "
              << last_byte << " (" << last_page << ")\n";

  int ret{0};

  // for now only check first 32 byte of a page
  // => first page will be skipped if not aligned
  for (uintptr_t pp = first_page << 12; pp <= last_page << 12; pp += 4096) {
    if (pp < first_byte || pp + 31 > last_byte) {
      continue;
    }
    const void *page_ptr = reinterpret_cast<void *>(pp);
    ret |= !std::memcmp(page_ptr, RANDOM_STRING, 32);
  }

  return ret;
}

static void gc() {
  signal_guard sg{};
  std::lock_guard<std::mutex> lg{known_op_wrappings_lock};
  for (auto it = known_op_wrappings.begin(); it != known_op_wrappings.end();) {
    wrapped_aocl_mmd_op_t *wrapped_op =
        static_cast<wrapped_aocl_mmd_op_t *>(*it);
    if (wrapped_op->obsolete) {
      it = known_op_wrappings.erase(it);
      delete wrapped_op;
    } else {
      ++it;
    }
  }
}

static void wrapping_handler(int handle, void *user_data, aocl_mmd_op_t op,
                             int status) {
  if (DEBUG)
    std::cout << "Custom status status handler called.\n";

  // If this handler is called, we know that registered_status_handlers[handle]
  // is set.
  aocl_mmd_status_handler_fn orig_handler;
  {
    signal_guard sg{};
    std::lock_guard<std::mutex> lg{registered_status_handlers_lock};
    orig_handler = registered_status_handlers.at(handle);
  }

  bool is_wrapped_read;
  {
    signal_guard sg{};
    std::lock_guard<std::mutex> lg{known_op_wrappings_lock};
    is_wrapped_read = known_op_wrappings.count(op);
  }

  if (is_wrapped_read) {
    if (DEBUG)
      std::cout << "Wrapped READ detected.\n";

    wrapped_aocl_mmd_op_t *wrapped_op =
        static_cast<wrapped_aocl_mmd_op_t *>(op);

    int err = check_pages(wrapped_op->dst, wrapped_op->len);
    if (err) {
      // pretty_print((unsigned char *)(wrapped_op->dst), wrapped_op->len);
      std::cerr << "!!! Incomplete data transfer detected. Re-issuing "
                   "transfer. !!!\n";
      aocl_mmd_read(handle, NULL, wrapped_op->len, wrapped_op->dst,
                    wrapped_op->interface, wrapped_op->offset);
    }

    orig_handler(handle, user_data, wrapped_op->op, status);
    wrapped_op->obsolete = true;
  } else {
    orig_handler(handle, user_data, op, status);
  }
}

int aocl_mmd_open(const char *name) {
  if (DEBUG)
    std::cout << "aocl_mmd_open\n";

  int device_handle = libbitt.aocl_mmd_open(name);

  // determine global memory interface
  if (device_handle) {
    int ret, gmem_handle;
    size_t result_size;
    ret = aocl_mmd_get_info(device_handle, AOCL_MMD_MEMORY_INTERFACE,
                            sizeof(int), &gmem_handle, &result_size);
    if (ret == 0) {
      signal_guard sg{};
      std::lock_guard<std::mutex> lg{gmem_interfaces_lock};
      gmem_interfaces.insert_or_assign(device_handle, gmem_handle);
    }
  }

  return device_handle;
}

int aocl_mmd_set_status_handler(int handle, aocl_mmd_status_handler_fn fn,
                                void *user_data) {

  if (DEBUG)
    std::cout << "aocl_mmd_set_status_handler\n";

  {
    signal_guard sg{};
    std::lock_guard<std::mutex> lg{registered_status_handlers_lock};
    registered_status_handlers.insert_or_assign(handle, fn);
  }

  return libbitt.aocl_mmd_set_status_handler(handle, wrapping_handler,
                                             user_data);
}

int aocl_mmd_read(int handle, aocl_mmd_op_t op, size_t len, void *dst,
                  int interface, size_t offset) {

  gc();

  int gmem_interface;
  {
    signal_guard sg{};
    std::lock_guard<std::mutex> lg{gmem_interfaces_lock};
    gmem_interface = gmem_interfaces.at(handle);
  }

  if (DEBUG)
    std::cout << "aocl_mmd_read on interface "
              << interface << " (gmem: " << gmem_interface << ")\n";

  if (interface != gmem_interface) {
    return libbitt.aocl_mmd_read(handle, op, len, dst, interface, offset);
  }

  stamp_pages(dst, len);

  if (op) {
    if (DEBUG)
      std::cout << "Non-blocking call. Custom status handler will be called.\n";

    auto wrapped_op =
        new wrapped_aocl_mmd_op_t{op, len, dst, interface, offset, false};
    {
      signal_guard sg{};
      std::lock_guard<std::mutex> lg{known_op_wrappings_lock};
      known_op_wrappings.insert(wrapped_op);
    }
    return libbitt.aocl_mmd_read(handle, wrapped_op, len, dst, interface,
                                 offset);

  } else {
    int ret = libbitt.aocl_mmd_read(handle, op, len, dst, interface, offset);
    int err = check_pages(dst, len);
    if (err) {
      // pretty_print((unsigned char *)(dst), len);
      std::cerr << "!!! Incomplete data transfer detected. Re-issuing "
                   "transfer. !!!\n";
      aocl_mmd_read(handle, NULL, len, dst, interface, offset);
    }
    return ret;
  }
}

/*
 * From here on, the remaining MMD API is implemented doing a simple forwarding
 * of function calls. No custom logic should follow below this comment.
 */

int aocl_mmd_get_offline_info(aocl_mmd_offline_info_t requested_info_id,
                              size_t param_value_size, void *param_value,
                              size_t *param_size_ret) {

  if (DEBUG)
    std::cout << "aocl_mmd_get_offline_info\n";

  return libbitt.aocl_mmd_get_offline_info(requested_info_id, param_value_size,
                                           param_value, param_size_ret);
}

int aocl_mmd_get_info(int handle, aocl_mmd_info_t requested_info_id,
                      size_t param_value_size, void *param_value,
                      size_t *param_size_ret) {

  if (DEBUG)
    std::cout << "aocl_mmd_get_info\n";

  return libbitt.aocl_mmd_get_info(handle, requested_info_id, param_value_size,
                                   param_value, param_size_ret);
}

int aocl_mmd_close(int handle) {

  if (DEBUG)
    std::cout << "aocl_mmd_close\n";

  return libbitt.aocl_mmd_close(handle);
}

int aocl_mmd_set_interrupt_handler(int handle, aocl_mmd_interrupt_handler_fn fn,
                                   void *user_data) {

  if (DEBUG)
    std::cout << "aocl_mmd_set_interrupt_handler\n";

  return libbitt.aocl_mmd_set_interrupt_handler(handle, fn, user_data);
}

int aocl_mmd_yield(int handle) {

  if (DEBUG)
    std::cout << "aocl_mmd_yield\n";

  return libbitt.aocl_mmd_yield(handle);
}

int aocl_mmd_write(int handle, aocl_mmd_op_t op, size_t len, const void *src,
                   int interface, size_t offset) {

  if (DEBUG)
    std::cout << "aocl_mmd_write\n";

  return libbitt.aocl_mmd_write(handle, op, len, src, interface, offset);
}

int aocl_mmd_copy(int handle, aocl_mmd_op_t op, size_t len, int intf,
                  size_t src_offset, size_t dst_offset) {

  if (DEBUG)
    std::cout << "aocl_mmd_copy\n";

  return libbitt.aocl_mmd_copy(handle, op, len, intf, src_offset, dst_offset);
}

int aocl_mmd_program(int handle, void *user_data, size_t size,
                     aocl_mmd_program_mode_t program_mode) {

  if (DEBUG)
    std::cout << "aocl_mmd_program\n";

  return libbitt.aocl_mmd_program(handle, user_data, size, program_mode);
}

int aocl_mmd_sch_status(const char *device_name, size_t channel_number,
                        unsigned int *param_value) {

  if (DEBUG)
    std::cout << "aocl_mmd_sch_status\n";

  return libbitt.aocl_mmd_sch_status(device_name, channel_number, param_value);
}

int aocl_mmd_sch_ctrl(const char *device_name, size_t channel_number,
                      unsigned int param_value) {

  if (DEBUG)
    std::cout << "aocl_mmd_sch_ctrl\n";

  return libbitt.aocl_mmd_sch_ctrl(device_name, channel_number, param_value);
}

int aocl_mmd_sch_perfctrl(const char *device_name, size_t channel_number,
                          unsigned int param_value) {

  if (DEBUG)
    std::cout << "aocl_mmd_sch_perfctrl\n";

  return libbitt.aocl_mmd_sch_perfctrl(device_name, channel_number,
                                       param_value);
}

int aocl_mmd_sch_rxperf(const char *device_name, size_t channel_number,
                        unsigned int *param_value) {

  if (DEBUG)
    std::cout << "aocl_mmd_sch_rxperf\n";

  return libbitt.aocl_mmd_sch_rxperf(device_name, channel_number, param_value);
}

int aocl_mmd_sch_txperf(const char *device_name, size_t channel_number,
                        unsigned int *param_value) {

  if (DEBUG)
    std::cout << "aocl_mmd_sch_txperf\n";

  return libbitt.aocl_mmd_sch_txperf(device_name, channel_number, param_value);
}

int aocl_mmd_card_info(const char *device_name,
                       aocl_mmd_info_t requested_info_id,
                       size_t param_value_size, void *param_value,
                       size_t *param_size_ret) {

  if (DEBUG)
    std::cout << "aocl_mmd_card_info\n";

  return libbitt.aocl_mmd_card_info(device_name, requested_info_id,
                                    param_value_size, param_value,
                                    param_size_ret);
}

int aocl_mmd_set_device_interrupt_handler(
    int handle, aocl_mmd_device_interrupt_handler_fn fn, void *user_data) {

  if (DEBUG)
    std::cout << "aocl_mmd_set_device_interrupt_handler\n";

  return libbitt.aocl_mmd_set_device_interrupt_handler(handle, fn, user_data);
}

int aocl_mmd_hostchannel_create(int handle, char *channel_name,
                                size_t queue_depth, int direction) {

  if (DEBUG)
    std::cout << "aocl_mmd_hostchannel_create\n";

  return libbitt.aocl_mmd_hostchannel_create(handle, channel_name, queue_depth,
                                             direction);
}

int aocl_mmd_hostchannel_destroy(int handle, int channel) {

  if (DEBUG)
    std::cout << "aocl_mmd_hostchannel_destroy\n";

  return libbitt.aocl_mmd_hostchannel_destroy(handle, channel);
}

void *aocl_mmd_hostchannel_get_buffer(int handle, int channel,
                                      size_t *buffer_size, int *status) {

  if (DEBUG)
    std::cout << "aocl_mmd_hostchannel_get_buffer\n";

  return libbitt.aocl_mmd_hostchannel_get_buffer(handle, channel, buffer_size,
                                                 status);
}

size_t aocl_mmd_hostchannel_ack_buffer(int handle, int channel,
                                       size_t send_size, int *status) {

  if (DEBUG)
    std::cout << "aocl_mmd_hostchannel_ack_buffer\n";

  return libbitt.aocl_mmd_hostchannel_ack_buffer(handle, channel, send_size,
                                                 status);
}

void *aocl_mmd_shared_mem_alloc(int handle, size_t size,
                                unsigned long long *device_ptr_out) {

  if (DEBUG)
    std::cout << "aocl_mmd_shared_mem_alloc\n";

  return libbitt.aocl_mmd_shared_mem_alloc(handle, size, device_ptr_out);
}

void aocl_mmd_shared_mem_free(int handle, void *host_ptr, size_t size) {

  if (DEBUG)
    std::cout << "aocl_mmd_shared_mem_free\n";

  return libbitt.aocl_mmd_shared_mem_free(handle, host_ptr, size);
}

/*
 * Must not be implemented for MMD versions 18.1 and newer. Otherwise the
 * following error is caused:

 * mmd program_device: aocl_mmd_reprogram is deprecated! Program with
 * aocl_mmd_program instead. Exit.
 */
#ifdef INCLUDE_AOCL_MMD_REPROGRAM
int aocl_mmd_reprogram(int handle, void *user_data, size_t size) {

  if (DEBUG)
    std::cout << "aocl_mmd_reprogram\n";

  return libbitt.aocl_mmd_reprogram(handle, user_data, size);
}
#endif
