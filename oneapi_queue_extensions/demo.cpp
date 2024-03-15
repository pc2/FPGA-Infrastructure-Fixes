#include <CL/sycl.hpp>
#include <iostream>
#include <mpi.h>
#include <pc2/queue_extensions.hpp>

int main() {
  MPI_Init(NULL, NULL);

  int myrank;
  MPI_Comm_rank(MPI_COMM_WORLD, &myrank);

  {
    auto q = sycl::ext::pc2::mpi_queue(sycl::ext::intel::fpga_selector_v);

    // Work with the queue
    std::cout << myrank << " running on device: "
              << q.get_device().get_info<sycl::info::device::name>() << "\n";
  }

  MPI_Barrier(MPI_COMM_WORLD);

  {
    auto qs = sycl::ext::pc2::mpi_queues(sycl::ext::intel::fpga_selector_v, 2);

    // Work with the queue
    for (auto &q : qs) {
      std::cout << myrank << " running on device: "
                << q.get_device().get_info<sycl::info::device::name>() << "\n";
    }
  }

  MPI_Finalize();
}