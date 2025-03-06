# TaskPool
A C++ task pool with a focus on light weight tasks. Internally it uses one queue per worker thread. If a worker runs out of work it attempts to steal work from another threads queue.
