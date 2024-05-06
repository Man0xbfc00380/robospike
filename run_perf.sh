perf record -g ./build/exp/exp0_cuda_gpu
perf script -i perf.data &> perf.unfold
~/FlameGraph/stackcollapse-perf.pl perf.unfold &> perf.folded
~/FlameGraph/flamegraph.pl perf.folded > perf.svg