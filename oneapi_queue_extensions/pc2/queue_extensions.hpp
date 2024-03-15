#pragma once

#include <mpi.h>
#include <stdlib.h>
#include <sycl/CL/sycl.hpp>
#include <sycl/ext/intel/fpga_extensions.hpp>

namespace sycl::ext::pc2::internal {

template <typename DeviceSelector>
static sycl::device get_device(int myrank,
                               const DeviceSelector &device_selector) {

  // Hide acl1 from rank 0
  if (myrank == 0) {
    setenv("HIDEACL", "1", true);
  }

  auto platform = sycl::platform(device_selector);
  auto devices = platform.get_devices();

  if (!devices.empty()) {
    return devices.back(); // Always return last visible device
  } else {
    return sycl::device{}; // Return default host device
  }
}

template <typename DeviceSelector, typename... Args>
static sycl::queue mpi_queue(int myrank, int nranks, MPI_Comm shmcomm,
                             const DeviceSelector &device_selector, Args... args) {

  // Use std::optional to avoid default initializing the queue
  std::optional<sycl::queue> q;

  for (int i = nranks - 1; i >= 0; i--) {
    if (myrank == i) {
      try {
        sycl::device mydev = get_device(myrank, device_selector);
        sycl::context ctx(mydev);
        q = sycl::queue(ctx, mydev, args...);
      } catch (const std::exception &e) {
        std::cout << "Exception " << e.what() << std::endl;
        MPI_Abort(MPI_COMM_WORLD, 0);
      }
    }
    MPI_Barrier(shmcomm);
  }

  return q.value();
}

template <typename DeviceSelector, typename... Args>
static std::vector<sycl::queue>
mpi_queues(int myrank, int nranks, MPI_Comm shmcomm,
           const DeviceSelector &device_selector, int num_qs, Args... args) {

  std::vector<sycl::queue> qs;

  for (int i = nranks - 1; i >= 0; i--) {
    if (myrank == i) {
      try {
        sycl::device mydev = get_device(myrank, device_selector);
        sycl::context ctx(mydev);
        for (int j = 0; j < num_qs; j++) {
          qs.emplace_back(ctx, mydev, args...);
        }
      } catch (const std::exception &e) {
        std::cout << "Exception " << e.what() << std::endl;
        MPI_Abort(MPI_COMM_WORLD, 0);
      }
    }
    MPI_Barrier(shmcomm);
  }

  return qs;
}

} // namespace sycl::ext::pc2::internal

namespace sycl::ext::pc2 {

template <typename DeviceSelector, typename... Args>
static sycl::queue mpi_queue(const DeviceSelector &device_selector, Args... args) {

  int initialized;
  MPI_Initialized(&initialized);
  if (!initialized) {
    MPI_Init(NULL, NULL);
  }

  int myrank, nranks;
  MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
  MPI_Comm_size(MPI_COMM_WORLD, &nranks);

  MPI_Comm shmcomm;
  MPI_Comm_split_type(MPI_COMM_WORLD, MPI_COMM_TYPE_SHARED, 0, MPI_INFO_NULL,
                      &shmcomm);

  int shmrank, shmranks;
  MPI_Comm_rank(shmcomm, &shmrank);
  MPI_Comm_size(shmcomm, &shmranks);

  auto q = internal::mpi_queue(shmrank, shmranks, shmcomm, device_selector, args...);

  MPI_Comm_free(&shmcomm);

  return q;
}

template <typename DeviceSelector, typename... Args>
static std::vector<sycl::queue>
mpi_queues(const DeviceSelector &device_selector, int num_qs, Args... args) {

  int initialized;
  MPI_Initialized(&initialized);
  if (!initialized) {
    MPI_Init(NULL, NULL);
  }

  int myrank, nranks;
  MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
  MPI_Comm_size(MPI_COMM_WORLD, &nranks);

  MPI_Comm shmcomm;
  MPI_Comm_split_type(MPI_COMM_WORLD, MPI_COMM_TYPE_SHARED, 0, MPI_INFO_NULL,
                      &shmcomm);

  int shmrank, shmranks;
  MPI_Comm_rank(shmcomm, &shmrank);
  MPI_Comm_size(shmcomm, &shmranks);

  auto qs =
      internal::mpi_queues(shmrank, shmranks, shmcomm, device_selector, num_qs, args...);

  MPI_Comm_free(&shmcomm);

  return qs;
}

} // namespace sycl::ext::pc2
